#include "ReflectionInspector.h"

#include "Gameplay/Object.h"
#include "Assets/AssetRegistry.h"
#include "Assets/Guid.h"
#include "Reflection/Class.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectedValue.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cstring>
#include <algorithm>

ReflectionInspector::ReflectionInspector(QWidget* Parent)
    : QWidget(Parent)
{
    setObjectName("ReflectionInspector");
    RootLayout = new QVBoxLayout(this);
    TypeLabel = new QLabel(this);
    TypeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    ResetAllButton = new QPushButton(tr("Reset All"), this);
    PropertiesLayout = new QFormLayout();
    PropertiesLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    RootLayout->addWidget(TypeLabel);
    RootLayout->addWidget(ResetAllButton);
    RootLayout->addLayout(PropertiesLayout);
    RootLayout->addStretch(1);

    connect(ResetAllButton, &QPushButton::clicked, this, &ReflectionInspector::OnResetAllClicked);
    ResetAllButton->setEnabled(false);
    ResetAllButton->setVisible(false);
}

void ReflectionInspector::SetInspectedObject(Object* Instance)
{
    InspectedObject = Instance;
    Rebuild();
}

Object* ReflectionInspector::GetInspectedObject() const
{
    return InspectedObject;
}

void ReflectionInspector::SetPropertyCommitCallback(PropertyCommitCallback Callback)
{
    CommitCallback = std::move(Callback);
}

void ReflectionInspector::SetPropertyResetCallback(PropertyResetCallback Callback)
{
    ResetCallback = std::move(Callback);
}

void ReflectionInspector::SetAssetRegistry(AssetRegistry* InRegistry)
{
    Registry = InRegistry;
    Rebuild();
}

void ReflectionInspector::SetAssetCommitCallback(AssetCommitCallback Callback)
{
    AssetCallback = std::move(Callback);
}

bool ReflectionInspector::CommitProperty(const PropertyId& Property, const ReflectedValue& NewValue)
{
    if (InspectedObject == nullptr)
    {
        return false;
    }
    if (CommitCallback)
    {
        return CommitCallback(InspectedObject, Property, NewValue);
    }
    ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
        InspectedObject,
        Property,
        NewValue,
        PropertyAccessContext::Inspector);
    return SetResult.bOk;
}

bool ReflectionInspector::CommitReset(const PropertyId& Property)
{
    if (InspectedObject == nullptr)
    {
        return false;
    }
    if (ResetCallback)
    {
        return ResetCallback(InspectedObject, Property);
    }
    ReflectionDiagnostic Reset = ResetPropertyToDefault(InspectedObject, Property);
    return Reset.bOk;
}

void ReflectionInspector::Rebuild()
{
    ClearPropertyEditors();

    if (InspectedObject == nullptr || InspectedObject->GetClass() == nullptr)
    {
        TypeLabel->setText(tr("Select an object in Hierarchy"));
        ResetAllButton->setEnabled(false);
        ResetAllButton->setVisible(false);
        return;
    }

    TypeLabel->setText(QString::fromStdString(InspectedObject->GetClass()->GetTypeId().Value));
    ResetAllButton->setEnabled(true);
    ResetAllButton->setVisible(true);

    for (const PropertyDescriptor* Descriptor : ListEditableProperties(InspectedObject))
    {
        if (Descriptor != nullptr)
        {
            AddPropertyEditor(*Descriptor);
        }
    }
}

void ReflectionInspector::ClearPropertyEditors()
{
    while (PropertiesLayout->rowCount() > 0)
    {
        PropertiesLayout->removeRow(0);
    }
}

std::vector<const PropertyDescriptor*> ReflectionInspector::ListEditableProperties(Object* Instance)
{
    std::vector<const PropertyDescriptor*> Result;
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return Result;
    }

    for (const PropertyDescriptor& Descriptor : Instance->GetClass()->GetProperties())
    {
        if (Descriptor.Attributes.bEditorHidden)
        {
            continue;
        }
        if (HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorVisible)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable)
            || HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorReadOnly))
        {
            Result.push_back(&Descriptor);
        }
    }
    return Result;
}

ReflectionDiagnostic ReflectionInspector::ResetPropertyToDefault(Object* Instance, const PropertyId& Property)
{
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }

    Object* DefaultObject = Instance->GetClass()->GetClassDefaultObject();
    if (DefaultObject == nullptr)
    {
        return ReflectionDiagnostic::Fail("Class has no CDO");
    }

    ReflectedValue DefaultValue;
    ReflectionDiagnostic Got = PropertyAccess::GetProperty(DefaultObject, Property, DefaultValue);
    if (!Got.bOk)
    {
        return Got;
    }
    return PropertyAccess::SetProperty(Instance, Property, DefaultValue, PropertyAccessContext::Inspector);
}

ReflectionDiagnostic ReflectionInspector::ResetAllToDefault(Object* Instance)
{
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return ReflectionDiagnostic::Fail("Instance is null");
    }

    for (const PropertyDescriptor* Descriptor : ListEditableProperties(Instance))
    {
        if (Descriptor == nullptr || Descriptor->bReadOnly)
        {
            continue;
        }
        if (!HasPropertyFlag(Descriptor->Attributes.Flags, PropertyFlags::EditorEditable))
        {
            continue;
        }
        ReflectionDiagnostic Reset = ResetPropertyToDefault(Instance, Descriptor->Id);
        if (!Reset.bOk)
        {
            return Reset;
        }
    }
    return ReflectionDiagnostic::Ok();
}

void ReflectionInspector::ApplyBoolValue(const PropertyId& Property, bool Value)
{
    if (CommitProperty(Property, ReflectedValue::MakeBool(Value)))
    {
        emit PropertyChanged();
    }
    else
    {
        Rebuild();
    }
}

void ReflectionInspector::ApplyFloatValue(const PropertyId& Property, double Value)
{
    if (CommitProperty(Property, ReflectedValue::MakeFloat(static_cast<float>(Value))))
    {
        emit PropertyChanged();
    }
    else
    {
        Rebuild();
    }
}

void ReflectionInspector::ApplyStringValue(const PropertyId& Property, const QString& Value)
{
    if (CommitProperty(Property, ReflectedValue::MakeString(Value.toStdString())))
    {
        emit PropertyChanged();
    }
    else
    {
        Rebuild();
    }
}

void ReflectionInspector::ApplyVector3Value(const PropertyId& Property, float X, float Y, float Z)
{
    float Values[3] = {X, Y, Z};
    ReflectedValue Updated = ReflectedValue::MakeEmpty();
    Updated.ValueKind = ReflectedValue::Kind::Bytes;
    Updated.BytesValue.resize(sizeof(Values));
    std::memcpy(Updated.BytesValue.data(), Values, sizeof(Values));
    if (CommitProperty(Property, Updated))
    {
        emit PropertyChanged();
    }
    else
    {
        Rebuild();
    }
}

void ReflectionInspector::ApplyColorValue(const PropertyId& Property, float R, float G, float B, float A)
{
    float Values[4] = {R, G, B, A};
    ReflectedValue Updated = ReflectedValue::MakeEmpty();
    Updated.ValueKind = ReflectedValue::Kind::Bytes;
    Updated.BytesValue.resize(sizeof(Values));
    std::memcpy(Updated.BytesValue.data(), Values, sizeof(Values));
    if (CommitProperty(Property, Updated))
    {
        emit PropertyChanged();
    }
    else
    {
        Rebuild();
    }
}

void ReflectionInspector::ApplyAssetValue(const PropertyDescriptor& Descriptor, const AssetKey& Key)
{
    const PropertyId Companion = Descriptor.Attributes.CompanionProperty != nullptr
        ? PropertyId{Descriptor.Attributes.CompanionProperty}
        : PropertyId{};
    bool bCommitted = false;
    if (AssetCallback)
    {
        bCommitted = AssetCallback(InspectedObject, Descriptor.Id, Companion, Key);
    }
    else
    {
        const std::string AssetValue = Key.Asset.IsValid() ? Key.Asset.ToString() : std::string{};
        bCommitted = CommitProperty(Descriptor.Id, ReflectedValue::MakeString(AssetValue));
        if (bCommitted && !Companion.Value.empty())
        {
            const std::string SubAssetValue = Key.HasSubAsset() ? Key.SubAsset->ToString() : std::string{};
            bCommitted = CommitProperty(Companion, ReflectedValue::MakeString(SubAssetValue));
        }
    }
    if (bCommitted)
    {
        Rebuild();
        emit PropertyChanged();
    }
}

void ReflectionInspector::OnResetPropertyClicked(const PropertyId Property)
{
    if (CommitReset(Property))
    {
        Rebuild();
        emit PropertyChanged();
    }
}

void ReflectionInspector::OnResetAllClicked()
{
    if (InspectedObject == nullptr)
    {
        return;
    }

    bool bAny = false;
    for (const PropertyDescriptor* Descriptor : ListEditableProperties(InspectedObject))
    {
        if (Descriptor == nullptr || Descriptor->bReadOnly)
        {
            continue;
        }
        if (!HasPropertyFlag(Descriptor->Attributes.Flags, PropertyFlags::EditorEditable))
        {
            continue;
        }
        if (CommitReset(Descriptor->Id))
        {
            bAny = true;
        }
    }

    if (bAny)
    {
        Rebuild();
        emit PropertyChanged();
    }
}

void ReflectionInspector::AddPropertyEditor(const PropertyDescriptor& Descriptor)
{
    ReflectedValue CurrentValue;
    ReflectionDiagnostic Got = PropertyAccess::GetProperty(
        InspectedObject,
        Descriptor,
        CurrentValue,
        PropertyAccessContext::Inspector);
    if (!Got.bOk)
    {
        return;
    }

    const QString Label = Descriptor.Attributes.DisplayName != nullptr
        ? QString::fromUtf8(Descriptor.Attributes.DisplayName)
        : QString::fromStdString(Descriptor.Id.Value);
    const bool bEditable = HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable)
        && !Descriptor.bReadOnly;
    const PropertyId Property = Descriptor.Id;

    QWidget* FieldWidget = new QWidget(this);
    QHBoxLayout* FieldLayout = new QHBoxLayout(FieldWidget);
    FieldLayout->setContentsMargins(0, 0, 0, 0);

    if (Descriptor.Attributes.AssetTypeFilter != nullptr)
    {
        AssetKey CurrentKey{};
        Guid::TryParse(CurrentValue.StringValue, CurrentKey.Asset);
        if (Descriptor.Attributes.CompanionProperty != nullptr && InspectedObject->GetClass() != nullptr)
        {
            ReflectedValue CompanionValue;
            const PropertyDescriptor* Companion = InspectedObject->GetClass()->FindProperty(
                PropertyId{Descriptor.Attributes.CompanionProperty});
            if (Companion != nullptr
                && PropertyAccess::GetProperty(InspectedObject, *Companion, CompanionValue, PropertyAccessContext::Inspector).bOk)
            {
                SubAssetId SubAsset{};
                if (Guid::TryParse(CompanionValue.StringValue, SubAsset) && SubAsset.IsValid())
                {
                    CurrentKey.SubAsset = SubAsset;
                }
            }
        }

        QString Display = tr("None");
        AssetRegistryEntry CurrentEntry{};
        const SubAssetRecord* CurrentSubAsset = nullptr;
        if (Registry != nullptr && Registry->TryResolveKey(CurrentKey, CurrentEntry, &CurrentSubAsset))
        {
            Display = QString::fromStdString(CurrentEntry.VirtualPath);
            if (CurrentSubAsset != nullptr)
            {
                Display += QString::fromUtf8(" :: ") + QString::fromStdString(CurrentSubAsset->Name);
            }
        }

        QPushButton* Picker = new QPushButton(Display, FieldWidget);
        Picker->setEnabled(bEditable && Registry != nullptr);
        Picker->setToolTip(Display);
        QMenu* Menu = new QMenu(Picker);
        QAction* ClearAction = Menu->addAction(tr("None"));
        connect(ClearAction, &QAction::triggered, this, [this, Descriptor]()
        {
            ApplyAssetValue(Descriptor, AssetKey{});
        });
        Menu->addSeparator();

        AssetType Filter = AssetType::Unknown;
        TryParseAssetType(Descriptor.Attributes.AssetTypeFilter, Filter);
        std::vector<AssetRegistryEntry> Entries;
        if (Registry != nullptr)
        {
            Entries = Registry->FindByDirectory("/Game");
            std::vector<AssetRegistryEntry> EngineEntries = Registry->FindByDirectory("/Engine");
            Entries.insert(Entries.end(), EngineEntries.begin(), EngineEntries.end());
        }
        std::sort(Entries.begin(), Entries.end(), [](const AssetRegistryEntry& First, const AssetRegistryEntry& Second)
        {
            return First.VirtualPath < Second.VirtualPath;
        });
        for (const AssetRegistryEntry& Entry : Entries)
        {
            if (Entry.Metadata.Type == Filter)
            {
                const AssetKey Key{Entry.Metadata.Guid, std::nullopt};
                QAction* Action = Menu->addAction(QString::fromStdString(Entry.VirtualPath));
                connect(Action, &QAction::triggered, this, [this, Descriptor, Key]()
                {
                    ApplyAssetValue(Descriptor, Key);
                });
            }
            for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
            {
                if (SubAsset.Type != Filter)
                {
                    continue;
                }
                AssetKey Key{Entry.Metadata.Guid, SubAsset.Id};
                QAction* Action = Menu->addAction(
                    QString::fromStdString(Entry.VirtualPath + " :: " + SubAsset.Name));
                connect(Action, &QAction::triggered, this, [this, Descriptor, Key]()
                {
                    ApplyAssetValue(Descriptor, Key);
                });
            }
        }
        Picker->setMenu(Menu);
        FieldLayout->addWidget(Picker, 1);
    }
    else if (Descriptor.ValueTypeId.Value == "engine.bool")
    {
        QCheckBox* CheckBox = new QCheckBox(FieldWidget);
        CheckBox->setChecked(CurrentValue.BoolValue);
        CheckBox->setEnabled(bEditable);
        FieldLayout->addWidget(CheckBox);
        if (bEditable)
        {
            connect(CheckBox, &QCheckBox::toggled, this, [this, Property](bool Value)
            {
                ApplyBoolValue(Property, Value);
            });
        }
    }
    else if (Descriptor.ValueTypeId.Value == "engine.int64")
    {
        QSpinBox* SpinBox = new QSpinBox(FieldWidget);
        if (Descriptor.Attributes.bHasRange)
        {
            SpinBox->setRange(
                static_cast<int>(Descriptor.Attributes.RangeMinimum),
                static_cast<int>(Descriptor.Attributes.RangeMaximum));
        }
        else
        {
            SpinBox->setRange(-1000000, 1000000);
        }
        SpinBox->setValue(static_cast<int>(CurrentValue.Int64Value));
        SpinBox->setEnabled(bEditable);
        FieldLayout->addWidget(SpinBox, 1);
        if (bEditable)
        {
            connect(SpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this, Property](int Value)
            {
                if (CommitProperty(Property, ReflectedValue::MakeInt64(Value)))
                {
                    emit PropertyChanged();
                }
                else
                {
                    Rebuild();
                }
            });
        }
    }
    else if (Descriptor.ValueTypeId.Value == "engine.float" || Descriptor.ValueTypeId.Value == "engine.double")
    {
        QDoubleSpinBox* SpinBox = new QDoubleSpinBox(FieldWidget);
        SpinBox->setDecimals(4);
        SpinBox->setSingleStep(0.1);
        if (Descriptor.Attributes.bHasRange)
        {
            SpinBox->setRange(Descriptor.Attributes.RangeMinimum, Descriptor.Attributes.RangeMaximum);
        }
        else
        {
            SpinBox->setRange(-1.0e9, 1.0e9);
        }
        SpinBox->setValue(CurrentValue.Float64Value);
        SpinBox->setEnabled(bEditable);
        FieldLayout->addWidget(SpinBox, 1);

        if (Descriptor.Attributes.bHasRange)
        {
            QSlider* Slider = new QSlider(Qt::Horizontal, FieldWidget);
            const double Minimum = Descriptor.Attributes.RangeMinimum;
            const double Maximum = Descriptor.Attributes.RangeMaximum;
            const int SliderSteps = 1000;
            Slider->setRange(0, SliderSteps);
            const double Normalized = (CurrentValue.Float64Value - Minimum)
                / (Maximum - Minimum);
            Slider->setValue(static_cast<int>(Normalized * SliderSteps));
            Slider->setEnabled(bEditable);
            FieldLayout->addWidget(Slider, 1);

            if (bEditable)
            {
                connect(Slider, &QSlider::valueChanged, this, [this, Property, SpinBox, Minimum, Maximum, SliderSteps](int SliderValue)
                {
                    const double Value = Minimum
                        + (static_cast<double>(SliderValue) / static_cast<double>(SliderSteps))
                            * (Maximum - Minimum);
                    SpinBox->blockSignals(true);
                    SpinBox->setValue(Value);
                    SpinBox->blockSignals(false);
                    ApplyFloatValue(Property, Value);
                });
                connect(SpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, Property, Slider, Minimum, Maximum, SliderSteps](double Value)
                {
                    const double NormalizedValue = (Value - Minimum) / (Maximum - Minimum);
                    Slider->blockSignals(true);
                    Slider->setValue(static_cast<int>(NormalizedValue * SliderSteps));
                    Slider->blockSignals(false);
                    ApplyFloatValue(Property, Value);
                });
            }
        }
        else if (bEditable)
        {
            connect(SpinBox, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, Property](double Value)
            {
                ApplyFloatValue(Property, Value);
            });
        }
    }
    else if (Descriptor.ValueTypeId.Value == "engine.string")
    {
        QLineEdit* LineEdit = new QLineEdit(FieldWidget);
        LineEdit->setText(QString::fromStdString(CurrentValue.StringValue));
        LineEdit->setEnabled(bEditable);
        FieldLayout->addWidget(LineEdit, 1);
        if (bEditable)
        {
            connect(LineEdit, &QLineEdit::editingFinished, this, [this, Property, LineEdit]()
            {
                ApplyStringValue(Property, LineEdit->text());
            });
        }
    }
    else if (Descriptor.ValueTypeId.Value == "engine.Vector3")
    {
        float Values[3] = {};
        if (CurrentValue.ValueKind == ReflectedValue::Kind::Bytes && CurrentValue.BytesValue.size() == sizeof(float) * 3)
        {
            std::memcpy(Values, CurrentValue.BytesValue.data(), sizeof(Values));
        }

        QDoubleSpinBox* SpinX = new QDoubleSpinBox(FieldWidget);
        QDoubleSpinBox* SpinY = new QDoubleSpinBox(FieldWidget);
        QDoubleSpinBox* SpinZ = new QDoubleSpinBox(FieldWidget);
        for (QDoubleSpinBox* SpinBox : {SpinX, SpinY, SpinZ})
        {
            SpinBox->setDecimals(4);
            SpinBox->setSingleStep(0.1);
            SpinBox->setRange(-1.0e9, 1.0e9);
            SpinBox->setEnabled(bEditable);
            FieldLayout->addWidget(SpinBox, 1);
        }
        SpinX->setValue(Values[0]);
        SpinY->setValue(Values[1]);
        SpinZ->setValue(Values[2]);

        if (bEditable)
        {
            auto CommitVector = [this, Property, SpinX, SpinY, SpinZ]()
            {
                ApplyVector3Value(
                    Property,
                    static_cast<float>(SpinX->value()),
                    static_cast<float>(SpinY->value()),
                    static_cast<float>(SpinZ->value()));
            };
            connect(SpinX, qOverload<double>(&QDoubleSpinBox::valueChanged), this, CommitVector);
            connect(SpinY, qOverload<double>(&QDoubleSpinBox::valueChanged), this, CommitVector);
            connect(SpinZ, qOverload<double>(&QDoubleSpinBox::valueChanged), this, CommitVector);
        }
    }
    else if (Descriptor.ValueTypeId.Value == "engine.Color")
    {
        float Values[4] = {1.f, 1.f, 1.f, 1.f};
        if (CurrentValue.ValueKind == ReflectedValue::Kind::Bytes && CurrentValue.BytesValue.size() == sizeof(float) * 4)
        {
            std::memcpy(Values, CurrentValue.BytesValue.data(), sizeof(Values));
        }

        QPushButton* ColorButton = new QPushButton(FieldWidget);
        const QColor CurrentColor = QColor::fromRgbF(Values[0], Values[1], Values[2], Values[3]);
        ColorButton->setText(CurrentColor.name(QColor::HexArgb));
        ColorButton->setEnabled(bEditable);
        FieldLayout->addWidget(ColorButton, 1);

        if (bEditable)
        {
            connect(ColorButton, &QPushButton::clicked, this, [this, Property, ColorButton, CurrentColor]()
            {
                const QColor Picked = QColorDialog::getColor(
                    CurrentColor,
                    this,
                    tr("Pick Color"),
                    QColorDialog::ShowAlphaChannel);
                if (!Picked.isValid())
                {
                    return;
                }
                ColorButton->setText(Picked.name(QColor::HexArgb));
                ApplyColorValue(
                    Property,
                    static_cast<float>(Picked.redF()),
                    static_cast<float>(Picked.greenF()),
                    static_cast<float>(Picked.blueF()),
                    static_cast<float>(Picked.alphaF()));
            });
        }
    }
    else
    {
        QLabel* Unsupported = new QLabel(tr("unsupported"), FieldWidget);
        FieldLayout->addWidget(Unsupported);
    }

    if (bEditable)
    {
        QPushButton* ResetButton = new QPushButton(tr("Reset"), FieldWidget);
        FieldLayout->addWidget(ResetButton);
        connect(ResetButton, &QPushButton::clicked, this, [this, Property]()
        {
            OnResetPropertyClicked(Property);
        });
    }

    PropertiesLayout->addRow(Label, FieldWidget);
}
