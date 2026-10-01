#include "Panels/MaterialBindingsDialog.h"
#include <QComboBox>
#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QVBoxLayout>

MaterialBindingsDialog::MaterialBindingsDialog(AssetRegistry& Registry, nlohmann::json Entries, bool bEffects, QWidget* Parent)
    : QDialog(Parent), BoundRegistry(Registry), bPostProcess(bEffects)
{
    setWindowTitle(tr("Material Bindings"));
    resize(650, 400);
    auto* Layout = new QVBoxLayout(this);
    Table = new QTableWidget(this);
    if (bPostProcess)
    {
        Table->setColumnCount(3);
        Table->setHorizontalHeaderLabels({tr("Material"), tr("Enabled"), tr("Intensity")});
    }
    else
    {
        Table->setColumnCount(2);
        Table->setHorizontalHeaderLabels({tr("Material"), tr("Slot")});
    }
    Table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    Layout->addWidget(Table);
    for (const auto& Entry : Entries)
    {
        AppendRow(Entry);
    }
    auto* Actions = new QHBoxLayout;
    auto* Add = new QPushButton(tr("Add"), this);
    auto* Remove = new QPushButton(tr("Remove"), this);
    auto* Up = new QPushButton(tr("Move Up"), this);
    auto* Down = new QPushButton(tr("Move Down"), this);
    Actions->addWidget(Add);
    Actions->addWidget(Remove);
    Actions->addWidget(Up);
    Actions->addWidget(Down);
    Layout->addLayout(Actions);
    connect(Add, &QPushButton::clicked, this, [this]()
    {
        AppendRow({{"material", AssetMetadataIO::AssetRefToJson({})}, {"slot", Table->rowCount()}});
    });
    connect(Remove, &QPushButton::clicked, this, [this]() { Table->removeRow(Table->currentRow()); });
    connect(Up, &QPushButton::clicked, this, [this]() { MoveRow(-1); });
    connect(Down, &QPushButton::clicked, this, [this]() { MoveRow(1); });
    auto* Buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    Layout->addWidget(Buttons);
    connect(Buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void MaterialBindingsDialog::AppendRow(const nlohmann::json& Entry)
{
    const int Row = Table->rowCount();
    Table->insertRow(Row);
    auto* Picker = new QComboBox(Table);
    Picker->addItem(tr("None"), QString());
    AssetKey Current;
    AssetDiagnostic Diagnostic;
    AssetMetadataIO::TryAssetRefFromJson(Entry.at("material"), Current, Diagnostic);
    for (const auto& Type : {MaterialAssetType, MaterialInstanceAssetType})
    {
        for (const auto& Asset : BoundRegistry.FindByType(Type))
        {
            Picker->addItem(QString::fromStdString(Asset.VirtualPath), QString::fromStdString(Asset.Metadata.Guid.ToString()));
        }
    }
    if (Current.IsValid())
    {
        const auto Value = QString::fromStdString(Current.Asset.ToString());
        if (Picker->findData(Value) < 0)
        {
            Picker->addItem(tr("Missing material"), Value);
        }
        Picker->setCurrentIndex(Picker->findData(Value));
    }
    Table->setCellWidget(Row, 0, Picker);
    if (bPostProcess)
    {
        auto* Enabled = new QCheckBox(Table);
        Enabled->setChecked(Entry.value("enabled", true));
        Table->setCellWidget(Row, 1, Enabled);
        auto* Intensity = new QDoubleSpinBox(Table);
        Intensity->setRange(0.0, 1.0);
        Intensity->setDecimals(3);
        Intensity->setSingleStep(0.05);
        Intensity->setValue(Entry.value("intensity", 1.0));
        Table->setCellWidget(Row, 2, Intensity);
    }
    else
    {
        auto* Slot = new QSpinBox(Table);
        Slot->setRange(0, INT32_MAX);
        Slot->setValue(Entry.value("slot", 0));
        Table->setCellWidget(Row, 1, Slot);
    }
    Table->selectRow(Row);
}

nlohmann::json MaterialBindingsDialog::GetEntries() const
{
    auto Entries = nlohmann::json::array();
    for (int Row = 0; Row < Table->rowCount(); ++Row)
    {
        AssetKey Material;
        Guid::TryParse(static_cast<QComboBox*>(Table->cellWidget(Row, 0))->currentData().toString().toStdString(), Material.Asset);
        nlohmann::json Entry = {{"material", AssetMetadataIO::AssetRefToJson(Material)}};
        if (bPostProcess)
        {
            Entry["enabled"] = static_cast<QCheckBox*>(Table->cellWidget(Row, 1))->isChecked();
            Entry["intensity"] = static_cast<QDoubleSpinBox*>(Table->cellWidget(Row, 2))->value();
        }
        else
        {
            Entry["slot"] = static_cast<QSpinBox*>(Table->cellWidget(Row, 1))->value();
        }
        Entries.push_back(Entry);
    }
    return Entries;
}

void MaterialBindingsDialog::MoveRow(int Direction)
{
    const int Selected = Table->currentRow();
    const int Destination = Selected + Direction;
    if (Selected < 0 || Destination < 0 || Destination >= Table->rowCount())
    {
        return;
    }
    auto Entries = GetEntries();
    std::swap(Entries[Selected], Entries[Destination]);
    Table->setRowCount(0);
    for (const auto& Entry : Entries)
    {
        AppendRow(Entry);
    }
    Table->selectRow(Destination);
}
