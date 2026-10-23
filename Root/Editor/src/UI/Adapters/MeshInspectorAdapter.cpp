#include "Panels/ReflectionInspector.h"
#include "Panels/AssetImportDialog.h"
#include "UI/Views/StudioControls.h"
#include "UI/Styles/StudioIcons.h"
#include "Assets/AssetImporter.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetRegistry.h"
#include "Assets/StaticMeshDefaults.h"
#include "Commands/StaticMeshCommands.h"
#include "Materials/MaterialDocumentIO.h"
#include "Reflection/Class.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include <QCheckBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

std::string ReflectionInspector::GetMeshPresentationState() const
{
    const auto* Mesh = dynamic_cast<MeshRendererComponent*>(GetInspectedObject());
    if (!Mesh)
    {
        return {};
    }
    nlohmann::json State;
    State["pending"] = Mesh->bPendingAsset;
    State["missing"] = Mesh->bMissingAsset;
    State["slots"] = nlohmann::json::array();
    for (const auto& Part : Mesh->Submeshes)
    {
        State["slots"].push_back(Part.MaterialSlot);
    }
    State["defaults"] = nlohmann::json::array();
    for (const auto& Entry : Mesh->DefaultMaterialSlots)
    {
        State["defaults"].push_back({Entry.first, Entry.second.Asset.ToString()});
    }
    State["physics"] = nlohmann::json::array();
    for (auto* Body : Mesh->GetGameObject()->GetComponents<PhysicsBodyComponent>())
    {
        State["physics"].push_back({Body->GetPersistentId(), Body->IsEnabled(), Body->MotionType, Body->bUseMeshDefaults});
    }
    return State.dump();
}

void ReflectionInspector::QueueMeshCommand(std::unique_ptr<EditorCommand> Command, QString Error)
{
    QTimer::singleShot(0, this, [this, Command = std::move(Command), Error = std::move(Error)]() mutable
    {
        MeshPresentationError = std::move(Error);
        bValuesDirty = true;
        if (Command && ExecuteMeshCommand)
        {
            const QPointer<ReflectionInspector> Guard(this);
            const bool bResult = ExecuteMeshCommand(std::move(Command));
            if (!Guard)
            {
                return;
            }
            if (!bResult)
            {
                MeshPresentationError = tr("Изменение отклонено. Остановите Play и проверьте выбранный объект.");
            }
            else
            {
                MeshPresentationError.clear();
                emit PropertyChanged();
            }
        }
        Rebuild();
    });
}

bool ReflectionInspector::BuildMeshPresentation()
{
    auto* Mesh = dynamic_cast<MeshRendererComponent*>(GetInspectedObject());
    if (!Mesh)
    {
        return false;
    }
    MeshPresentationState = GetMeshPresentationState();
    auto AddNote = [this](const QString& Text)
    {
        auto* Note = new QLabel(Text, this);
        Note->setObjectName("MeshInspectorNote");
        Note->setWordWrap(true);
        Note->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        PropertiesLayout->addRow(Note);
    };
    auto AddTitle = [this](const QString& Text)
    {
        auto* Title = new QLabel(Text, this);
        Title->setProperty("uiRole", "objectSectionTitle");
        PropertiesLayout->addRow(Title);
    };
    if (!MeshPresentationError.isEmpty())
    {
        AddNote(MeshPresentationError);
    }
    if (const auto* Property = Mesh->GetClass()->FindProperty(PropertyId{"mesh_asset_id"}))
    {
        AddPropertyEditor(*Property);
    }
    auto* AssetActions = new QWidget(this);
    auto* AssetLayout = new QHBoxLayout(AssetActions);
    AssetLayout->setContentsMargins(0, 0, 0, 0);
    auto* Open = new QPushButton(tr("Открыть меш"), AssetActions);
    Open->setObjectName("MeshOpenAsset");
    Open->setToolTip(tr("Открыть материалы и коллизию ресурса меша"));
    Open->setEnabled(Mesh->GetMeshAssetKey().IsValid() && static_cast<bool>(OpenMesh));
    connect(Open, &QPushButton::clicked, this, [this]()
    {
        auto* Current = dynamic_cast<MeshRendererComponent*>(GetInspectedObject());
        if (Current && OpenMesh)
        {
            OpenMesh(Current->GetMeshAssetKey());
        }
    });
    AssetLayout->addWidget(Open);
    auto* Import = new QPushButton(tr("Импортировать…"), AssetActions);
    Import->setObjectName("MeshUpload");
    Import->setEnabled(Registry != nullptr);
    AssetLayout->addWidget(Import);
    PropertiesLayout->addRow(AssetActions);
    connect(Import, &QPushButton::clicked, this, [this]()
    {
        const auto Source = QFileDialog::getOpenFileName(this, tr("Импортировать меш"), {}, tr("Меши (*.glb *.fbx)"));
        if (Source.isEmpty())
        {
            return;
        }
        AssetImportDialog Dialog({Source}, "Meshes", this);
        if (Dialog.exec() != QDialog::Accepted)
        {
            return;
        }
        ImportRequest Request;
        Request.SourcePath = std::filesystem::path(Source.toStdWString());
        QString Destination = Dialog.GetDestinationFileName(0);
        if (!Dialog.GetDestinationFolder().isEmpty())
        {
            Destination = Dialog.GetDestinationFolder() + "/" + Destination;
        }
        Request.DestinationRelativePath = Destination.toStdString();
        Request.bGenerateMissingNormals = Dialog.GetGenerateMissingNormals();
        const auto Result = AssetImporter(*Registry).Import(Request);
        if (!Result.Succeeded())
        {
            QMessageBox::warning(this, tr("Импорт не выполнен"), QString::fromStdString(Result.Diagnostic.Message));
            return;
        }
        for (const auto& Subasset : Result.Metadata.SubAssets)
        {
            if (Subasset.Type == StaticMeshAssetType)
            {
                if (const auto* Property = GetInspectedObject()->GetClass()->FindProperty(PropertyId{"mesh_asset_id"}))
                {
                    ApplyAssetValue(*Property, {Result.Metadata.Guid, Subasset.Id});
                }
                return;
            }
        }
        QMessageBox::warning(this, tr("Меш не назначен"), tr("Файл не содержит статических мешей."));
    });
    AddTitle(tr("Материалы"));
    std::set<int32_t> Slots;
    std::set<int32_t> ActualSlots;
    for (const auto& Part : Mesh->Submeshes)
    {
        ActualSlots.insert(Part.MaterialSlot);
    }
    AssetHandle<StaticMeshResource> CpuMesh;
    auto* World = Mesh->GetGameObject()->GetScene();
    if (World->GetAssetManager() && World->GetAssetManager()->TryGetLoaded(Mesh->GetMeshAssetKey(), CpuMesh))
    {
        for (const auto& Part : CpuMesh->Submeshes)
        {
            ActualSlots.insert(Part.MaterialSlot);
        }
    }
    Slots = ActualSlots;
    for (const auto& Override : Mesh->MaterialSlots.Entries)
    {
        Slots.insert(Override.Slot);
    }
    std::vector<std::pair<QString, AssetKey>> Materials;
    if (Registry)
    {
        auto Entries = Registry->FindByType(MaterialAssetType);
        const auto Instances = Registry->FindByType(MaterialInstanceAssetType);
        Entries.insert(Entries.end(), Instances.begin(), Instances.end());
        std::sort(Entries.begin(), Entries.end(), [](const auto& Left, const auto& Right)
        {
            return Left.VirtualPath < Right.VirtualPath;
        });
        for (const auto& Entry : Entries)
        {
            const AssetKey Key{Entry.Metadata.Guid, std::nullopt};
            ResolvedMaterial Resolved;
            if (!MaterialDocumentIO::Resolve(*Registry, Key, Resolved).HasError() && Resolved.Definition.Domain == MaterialDomain::Surface)
            {
                Materials.push_back({QString::fromStdString(Entry.VirtualPath), Key});
            }
        }
    }
    auto Assign = [this](int32_t Slot, const AssetKey& Material)
    {
        auto* Current = dynamic_cast<MeshRendererComponent*>(GetInspectedObject());
        if (!Current || !Registry)
        {
            return;
        }
        std::string Error;
        auto Command = MakeAssignMeshMaterialCommand(Current->GetGameObject()->GetScene(), *Registry,
            Current->GetObjectHandle(), Slot, Material, Error);
        QueueMeshCommand(std::move(Command), QString::fromStdString(Error));
    };
    auto* All = new QPushButton(tr("Назначить всем слотам…"), this);
    All->setObjectName("MeshAssignAllMaterials");
    All->setEnabled(!Slots.empty() && static_cast<bool>(ExecuteMeshCommand));
    auto* Menu = new QMenu(All);
    Menu->setObjectName("StudioActionMenu");
    All->setMenu(Menu);
    connect(Menu->addAction(tr("Вернуть материалы меша")), &QAction::triggered, this, [Assign]() { Assign(-1, {}); });
    Menu->addSeparator();
    for (const auto& Entry : Materials)
    {
        connect(Menu->addAction(Entry.first), &QAction::triggered, this, [Assign, Key = Entry.second]() { Assign(-1, Key); });
    }
    PropertiesLayout->addRow(All);
    if (Slots.empty())
    {
        AddNote(tr("Выберите меш. Его материальные слоты появятся после загрузки."));
    }
    auto MaterialName = [this](const AssetKey& Key)
    {
        AssetRegistryEntry Entry;
        if (Registry && Registry->TryResolveKey(Key, Entry))
        {
            return QFileInfo(QString::fromStdString(Entry.VirtualPath)).completeBaseName();
        }
        return tr("Недоступный материал");
    };
    for (int32_t Slot : Slots)
    {
        auto* Card = new QFrame(this);
        Card->setObjectName("MeshMaterialSlot");
        auto* Layout = new QVBoxLayout(Card);
        Layout->setContentsMargins(8, 6, 8, 8);
        Layout->setSpacing(4);
        auto* Header = new QHBoxLayout();
        QString Caption = tr("Слот %1").arg(Slot + 1);
        if (!ActualSlots.count(Slot))
        {
            Caption += tr(" (нет в меше)");
        }
        Header->addWidget(new QLabel(Caption, Card), 1);
        auto* Reset = new QPushButton(tr("Из меша"), Card);
        Reset->setObjectName(QString("MeshResetMaterial_%1").arg(Slot));
        Reset->setToolTip(tr("Убрать переопределение этого слота и использовать материал ресурса меша"));
        Header->addWidget(Reset);
        Layout->addLayout(Header);
        auto* Material = new EditorUi::StudioSelect(Card);
        Material->setObjectName(QString("MeshMaterialSlot_%1").arg(Slot));
        Material->setAccessibleName(tr("Материал слота %1").arg(Slot + 1));
        Material->setEnabled(static_cast<bool>(ExecuteMeshCommand));
        QString Inherited = tr("Из модели");
        if (CpuMesh.IsValid())
        {
            const auto Found = CpuMesh->Defaults.Materials.find(Slot);
            if (Found != CpuMesh->Defaults.Materials.end())
            {
                Inherited = tr("Из меша: %1").arg(MaterialName(Found->second));
            }
        }
        Material->addItem(Inherited);
        std::vector<AssetKey> Keys{AssetKey{}};
        AssetKey Current = Mesh->GetMaterialAssetKey();
        bool bOverride = Current.IsValid();
        for (const auto& Override : Mesh->MaterialSlots.Entries)
        {
            if (Override.Slot == Slot)
            {
                Current = Override.Material;
                bOverride = Current.IsValid();
                break;
            }
        }
        for (const auto& Entry : Materials)
        {
            Material->addItem(Entry.first);
            Material->setItemData(Material->count() - 1, Entry.first, Qt::ToolTipRole);
            Keys.push_back(Entry.second);
            if (Current == Entry.second)
            {
                Material->setCurrentIndex(Material->count() - 1);
            }
        }
        if (Current.IsValid() && Material->currentIndex() == 0)
        {
            Material->addItem(tr("Недоступен: %1").arg(QString::fromStdString(Current.Asset.ToString())));
            Keys.push_back(Current);
            Material->setCurrentIndex(Material->count() - 1);
        }
        Reset->setEnabled(bOverride && static_cast<bool>(ExecuteMeshCommand));
        connect(Reset, &QPushButton::clicked, this, [Assign, Slot]() { Assign(Slot, {}); });
        connect(Material, &QComboBox::activated, this, [Assign, Slot, Keys](int Index)
        {
            if (Index >= 0 && Index < static_cast<int>(Keys.size()))
            {
                Assign(Slot, Keys[Index]);
            }
        });
        Layout->addWidget(Material);
        PropertiesLayout->addRow(Card);
    }
    AddTitle(tr("Физика объекта"));
    const auto Bodies = Mesh->GetGameObject()->GetComponents<PhysicsBodyComponent>();
    PhysicsBodyComponent* Body = nullptr;
    if (!Bodies.empty())
    {
        Body = Bodies.front();
    }
    const bool bSpecialized = Bodies.size() > 1 || (Body && (dynamic_cast<CharacterControllerComponent*>(Body) || Body->MotionType == 3));
    auto* Collision = new QCheckBox(tr("Коллизия"), this);
    Collision->setObjectName("MeshPhysicsEnabled");
    Collision->setToolTip(tr("Участие объекта в столкновениях и физических запросах"));
    Collision->setChecked(Body && Body->IsEnabled());
    Collision->setEnabled(!bSpecialized && Mesh->GetMeshAssetKey().IsValid() && static_cast<bool>(ExecuteMeshCommand));
    PropertiesLayout->addRow(Collision);
    auto* Simulate = new QCheckBox(tr("Симулировать физику"), this);
    Simulate->setObjectName("MeshPhysicsSimulate");
    Simulate->setToolTip(tr("Динамическое тело: движется под действием гравитации, сил и столкновений в Play"));
    Simulate->setChecked(Body && Body->MotionType == 1);
    Simulate->setEnabled(Collision->isEnabled() && Collision->isChecked());
    PropertiesLayout->addRow(Simulate);
    if (bSpecialized)
    {
        AddNote(tr("У объекта контроллер персонажа или несколько физических тел. Настройте их в соответствующих компонентах."));
    }
    else if (Body && Body->IsEnabled() && Body->MotionType == 2)
    {
        AddNote(tr("Кинематическое тело: перемещается из кода, участвует в столкновениях."));
    }
    else if (Body && Body->IsEnabled() && Body->MotionType == 1)
    {
        AddNote(tr("Динамическое тело. Масса и остальные параметры доступны в компоненте физики."));
    }
    else if (Body && Body->IsEnabled())
    {
        AddNote(tr("Неподвижное препятствие. Для движения под действием сил включите симуляцию."));
    }
    else
    {
        AddNote(tr("Коллизия выключена. При первом включении используются настройки меша или Box по его границам."));
    }
    auto ChangePhysics = [this](bool bEnabled, bool bSimulate)
    {
        auto* Current = dynamic_cast<MeshRendererComponent*>(GetInspectedObject());
        if (!Current || !Registry)
        {
            return;
        }
        std::string Error;
        auto Command = MakeSetMeshPhysicsCommand(Current->GetGameObject()->GetScene(), *Registry,
            Current->GetObjectHandle(), bEnabled, bSimulate, Error);
        QString Message = QString::fromStdString(Error);
        if (Error.find("Triangle mesh") != std::string::npos)
        {
            Message = tr("Треугольная коллизия не поддерживает динамическую симуляцию. Выберите Box, Sphere или Capsule в настройках коллизии.");
        }
        QueueMeshCommand(std::move(Command), Message);
    };
    connect(Collision, &QCheckBox::toggled, this, [ChangePhysics, Simulate](bool bEnabled)
    {
        ChangePhysics(bEnabled, bEnabled && Simulate->isChecked());
    });
    connect(Simulate, &QCheckBox::toggled, this, [ChangePhysics, Collision](bool bSimulate)
    {
        ChangePhysics(Collision->isChecked(), bSimulate);
    });
    if (const auto* Property = Mesh->GetClass()->FindProperty(PropertyId{"visible"}))
    {
        AddTitle(tr("Отображение"));
        AddPropertyEditor(*Property);
    }
    return true;
}
