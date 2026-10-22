#include "Panels/ReflectionInspector.h"
#include "Panels/AssetImportDialog.h"
#include "UI/Views/StudioControls.h"
#include "UI/Styles/StudioIcons.h"
#include "Assets/AssetImporter.h"
#include "Assets/AssetRegistry.h"
#include "Reflection/Class.h"
#include "Reflection/ReflectionValueCodec.h"
#include "World/Components/MeshRendererComponent.h"
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

// A presentation adapter over the same reflected properties and command callbacks.
// Mesh/material identities, slot serialization and runtime loading stay in Engine.
bool ReflectionInspector::BuildMeshPresentation()
{
    auto* Mesh = dynamic_cast<MeshRendererComponent*>(GetInspectedObject());
    if (!Mesh) { return false; }
    auto AddNote = [this](const QString& Text) {
        auto* Note = new QLabel(Text, this);
        Note->setObjectName("MeshInspectorNote");
        Note->setWordWrap(true);
        Note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        PropertiesLayout->addRow(Note);
    };
    auto* Caption = new QLabel(tr("Custom mesh"), this);
    Caption->setProperty("uiRole", "objectSectionTitle");
    PropertiesLayout->addRow(Caption);
    AddNote(tr("GLB / FBX · статическая геометрия. GLB со встроенными текстурами. Размеры и начало координат сохраняются."));
    auto* Upload = new QPushButton(EditorUi::MakeFrontendIcon("Upload"), tr("Загрузить меш"), this);
    Upload->setObjectName("MeshUpload");
    Upload->setEnabled(Registry != nullptr);
    PropertiesLayout->addRow(Upload);
    connect(Upload, &QPushButton::clicked, this, [this]() {
        const auto Source = QFileDialog::getOpenFileName(this, tr("Загрузить меш"), {}, tr("Меши (*.glb *.fbx)"));
        if (Source.isEmpty()) { return; }
        AssetImportDialog Dialog({Source}, "Meshes", this);
        if (Dialog.exec() != QDialog::Accepted) { return; }
        ImportRequest Request;
        Request.SourcePath = std::filesystem::path(Source.toStdWString());
        Request.DestinationRelativePath = (Dialog.GetDestinationFolder().isEmpty() ? Dialog.GetDestinationFileName(0) :
            Dialog.GetDestinationFolder() + "/" + Dialog.GetDestinationFileName(0)).toStdString();
        Request.bGenerateMissingNormals = Dialog.GetGenerateMissingNormals();
        const auto Result = AssetImporter(*Registry).Import(Request);
        if (!Result.Succeeded())
        {
            QMessageBox::warning(this, tr("Не удалось загрузить меш"), QString::fromStdString(Result.Diagnostic.Message));
            return;
        }
        for (const auto& SubAsset : Result.Metadata.SubAssets)
        {
            if (SubAsset.Type == StaticMeshAssetType)
            {
                const auto* Property = GetInspectedObject()->GetClass()->FindProperty(PropertyId{"mesh_asset_id"});
                if (Property) { ApplyAssetValue(*Property, {Result.Metadata.Guid, SubAsset.Id}); }
                return;
            }
        }
        QMessageBox::warning(this, tr("Не удалось назначить меш"), tr("Файл не содержит статических мешей."));
    });
    // Keep selection of already imported assets immediately available too.
    if (const auto* Property = Mesh->GetClass()->FindProperty(PropertyId{"mesh_asset_id"}))
    { AddPropertyEditor(*Property); }
    auto* Target = new EditorUi::StudioSelect(this);
    Target->setObjectName("MeshMaterialTarget");
    Target->setAccessibleName(tr("Назначить материал"));
    Target->addItem(tr("Весь объект"), -1);
    std::set<int> Slots;
    for (const auto& Part : Mesh->Submeshes) { Slots.insert(Part.MaterialSlot); }
    for (const auto& Override : Mesh->MaterialSlots.Entries) { Slots.insert(Override.Slot); }
    for (int Slot : Slots) { Target->addItem(tr("Слот %1").arg(Slot + 1), Slot); }
    if (SelectedMaterialSlot >= 0 && !Slots.count(SelectedMaterialSlot)) { SelectedMaterialSlot = -1; }
    Target->setCurrentIndex(Target->findData(SelectedMaterialSlot));
    PropertiesLayout->addRow(new QLabel(tr("Назначить материал"), this));
    PropertiesLayout->addRow(Target);
    connect(Target, &QComboBox::activated, this, [this, Target](int Index) {
        SelectedMaterialSlot = Target->itemData(Index).toInt();
        Rebuild();
    });
    auto* Material = new EditorUi::StudioSelect(this);
    Material->setObjectName("MeshMaterial");
    Material->setAccessibleName(tr("Материал"));
    Material->setEnabled(Registry != nullptr);
    Material->addItem(tr("Исходные материалы"));
    std::vector<AssetKey> Keys{AssetKey{}};
    AssetKey Current = Mesh->GetMaterialAssetKey();
    if (SelectedMaterialSlot >= 0)
    {
        Current = {};
        for (const auto& Override : Mesh->MaterialSlots.Entries)
        { if (Override.Slot == SelectedMaterialSlot) { Current = Override.Material; break; } }
    }
    auto AddMaterial = [&](const QString& Label, const AssetKey& Key) {
        Material->addItem(Label);
        Material->setItemData(Material->count() - 1, Label, Qt::ToolTipRole);
        Keys.push_back(Key);
        if (Key == Current) { Material->setCurrentIndex(Material->count() - 1); }
    };
    if (Registry)
    {
        auto Entries = Registry->FindByDirectory("/Game");
        const auto EngineEntries = Registry->FindByDirectory("/Engine");
        Entries.insert(Entries.end(), EngineEntries.begin(), EngineEntries.end());
        std::sort(Entries.begin(), Entries.end(), [](const auto& Left, const auto& Right) { return Left.VirtualPath < Right.VirtualPath; });
        for (const auto& Entry : Entries)
        {
            if (Entry.Metadata.Type == MaterialAssetType || Entry.Metadata.Type == MaterialInstanceAssetType)
            { AddMaterial(QString::fromStdString(Entry.VirtualPath), {Entry.Metadata.Guid, std::nullopt}); }
        }
    }
    if (Current.IsValid() && Material->currentIndex() == 0) { AddMaterial(tr("Ресурс недоступен"), Current); }
    PropertiesLayout->addRow(new QLabel(tr("Материал"), this));
    PropertiesLayout->addRow(Material);
    connect(Material, &QComboBox::activated, this, [this, Keys](int Index) {
        if (Index < 0 || Index >= static_cast<int>(Keys.size())) { return; }
        auto* CurrentMesh = dynamic_cast<MeshRendererComponent*>(GetInspectedObject());
        if (!CurrentMesh) { return; }
        if (SelectedMaterialSlot < 0)
        {
            if (const auto* Property = CurrentMesh->GetClass()->FindProperty(PropertyId{"material_asset_id"}))
            { ApplyAssetValue(*Property, Keys[Index]); }
            return;
        }
        auto Overrides = CurrentMesh->MaterialSlots;
        auto& Entries = Overrides.Entries;
        Entries.erase(std::remove_if(Entries.begin(), Entries.end(), [this](const auto& Entry) {
            return Entry.Slot == SelectedMaterialSlot;
        }), Entries.end());
        if (Keys[Index].IsValid())
        {
            MaterialSlotOverride Entry;
            Entry.Slot = SelectedMaterialSlot;
            Entry.Material = Keys[Index];
            Entries.push_back(Entry);
        }
        ReflectedValue Value;
        if (ReflectionValueCodec<MaterialSlotOverrides>::ToReflected(Overrides, Value).bOk &&
            CommitProperty(PropertyId{"material_slots"}, Value))
        { emit PropertyChanged(); }
        Rebuild();
    });
    AddNote(tr("Назначения сохраняются в проекте. Материал слота имеет приоритет над материалом всего объекта."));
    auto* Details = new QToolButton(this);
    Details->setObjectName("MeshDetails");
    Details->setText(tr("Дополнительно"));
    Details->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Details->setCheckable(true);
    Details->setChecked(bMeshDetailsExpanded);
    Details->setIcon(EditorUi::MakeStudioIcon(bMeshDetailsExpanded ? EditorUi::StudioIcon::ChevronDown : EditorUi::StudioIcon::ChevronRight));
    PropertiesLayout->addRow(Details);
    connect(Details, &QToolButton::toggled, this, [this](bool Expanded) { bMeshDetailsExpanded = Expanded; Rebuild(); });
    if (bMeshDetailsExpanded)
    {
        for (const auto* Property : ListEditableProperties(Mesh))
        {
            if (Property && Property->Id.Value != "mesh_asset_id" && Property->Id.Value != "material_asset_id")
            { AddPropertyEditor(*Property); }
        }
        auto* Reset = new QPushButton(tr("Сбросить свойства"), this);
        PropertiesLayout->addRow(Reset);
        connect(Reset, &QPushButton::clicked, this, &ReflectionInspector::OnResetAllClicked);
    }
    return true;
}
