#include "UI/Views/AssetReferenceField.h"
#include "UI/Adapters/AssetThumbnails.h"
#include "UI/Views/StudioControls.h"
#include "UI/Presentation/InspectorLabels.h"
#include "Panels/ReflectionInspector.h"
#include "UI/Adapters/InspectorUpdateGuard.h"
#include "UI/Presentation/InspectorValue.h"
#include "Core/Object/MemorySubsystem.h"
#include <QSignalBlocker>
#include <QTimer>
#include <limits>
#include "Panels/ReflectionValueEditor.h"

#include "Core/Object/Object.h"
#include "Assets/AssetRegistry.h"
#include "Assets/Guid.h"
#include "Reflection/Class.h"
#include "Reflection/PropertyAccess.h"
#include "Reflection/ReflectedValue.h"

#include <QCheckBox>
#include <QAbstractSpinBox>
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
#include "UI/Styles/StudioIcons.h"

namespace
{
bool RefreshSimpleField(QWidget* Field, const PropertyDescriptor& Descriptor, const ReflectedValue& Value)
{
    if (Descriptor.Attributes.AssetTypeFilter || !Descriptor.Attributes.AllowedAssetTypes.empty()) { return false; }
    if (Value.ValueKind == ReflectedValue::Kind::Bool)
    {
        auto* Check = Field->findChild<QCheckBox*>();
        if (!Check) { return false; }
        const QSignalBlocker Blocker(Check);
        Check->setChecked(Value.BoolValue);
        return true;
    }
    if (Value.ValueKind == ReflectedValue::Kind::Int64)
    {
        if (auto* Spin = Field->findChild<QSpinBox*>())
        {
            if (Value.Int64Value < Spin->minimum() || Value.Int64Value > Spin->maximum()) { return false; }
            const QSignalBlocker Blocker(Spin);
            Spin->setValue(static_cast<int>(Value.Int64Value));
            return true;
        }
        if (auto* Edit = Field->findChild<QLineEdit*>())
        {
            const QSignalBlocker Blocker(Edit);
            const auto Text = QString::number(Value.Int64Value);
            if (Edit->text() != Text) { Edit->setText(Text); }
            return true;
        }
    }
    if (Value.ValueKind == ReflectedValue::Kind::Float64)
    {
        auto* Spin = Field->findChild<QDoubleSpinBox*>();
        if (!Spin) { return false; }
        const QSignalBlocker Blocker(Spin);
        Spin->setValue(Value.Float64Value);
        if (auto* Slider = Field->findChild<QSlider*>())
        {
            const QSignalBlocker SliderBlocker(Slider);
            const double Range = Descriptor.Attributes.RangeMaximum - Descriptor.Attributes.RangeMinimum;
            Slider->setValue(Range > 0 ? static_cast<int>((Value.Float64Value - Descriptor.Attributes.RangeMinimum) / Range * 1000) : 0);
        }
        return true;
    }
    if (Value.ValueKind == ReflectedValue::Kind::String)
    {
        const auto Text = QString::fromStdString(Value.StringValue);
        if (auto* Edit = Field->findChild<QLineEdit*>())
        {
            const QSignalBlocker Blocker(Edit);
            if (Edit->text() != Text) { Edit->setText(Text); }
            return true;
        }
        if (auto* Label = Field->findChild<QLabel*>())
        {
            if (Text.isEmpty()) { return false; }
            Label->setText(Text);
            return true;
        }
    }
    if (Descriptor.ValueTypeId.Value == "engine.Vector3" && Value.BytesValue.size() == 3 * sizeof(float))
    {
        const auto Spins = Field->findChildren<QDoubleSpinBox*>();
        if (Spins.size() != 3) { return false; }
        float Channels[3];
        std::memcpy(Channels, Value.BytesValue.data(), sizeof(Channels));
        for (int Index = 0; Index < 3; ++Index)
        {
            const QSignalBlocker Blocker(Spins[Index]);
            Spins[Index]->setValue(Channels[Index]);
        }
        return true;
    }
    return false;
}

void RetirePropertyEditor(QLayoutItem* Item)
{
    if (!Item) { return; }
    if (auto* Widget = Item->widget())
    {
        // Rebuild may run inside activated/valueChanged. Qt still uses the
        // sender after our callback returns (e.g. to emit textActivated).
        // Silence and hide the old controls now, destroy after dispatch ends.
        Widget->blockSignals(true);
        for (auto* Child : Widget->findChildren<QWidget*>()) { Child->blockSignals(true); }
        Widget->hide();
        Widget->deleteLater();
    }
    else if (auto* Layout = Item->layout())
    {
        while (auto* Child = Layout->takeAt(0)) { RetirePropertyEditor(Child); }
    }
    delete Item;
}
}

ReflectionInspector::ReflectionInspector(QWidget* Parent)
    : QWidget(Parent)
{
    setObjectName("ReflectionInspector");
    RootLayout = new QVBoxLayout(this);
    RootLayout->setContentsMargins(16, 8, 16, 14);
    RootLayout->setSpacing(10);
    TypeLabel = new QLabel(this);
    TypeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    TypeLabel->setWordWrap(true);
    TypeLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ResetAllButton = new QPushButton(tr("Сбросить свойства"), this);
    PropertiesLayout = new QFormLayout();
    PropertiesLayout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    PropertiesLayout->setRowWrapPolicy(QFormLayout::WrapAllRows);
    PropertiesLayout->setVerticalSpacing(8);
    PropertiesLayout->setHorizontalSpacing(8);

    RootLayout->addWidget(TypeLabel);
    RootLayout->addWidget(ResetAllButton);
    RootLayout->addLayout(PropertiesLayout);
    RootLayout->addStretch(1);

    connect(ResetAllButton, &QPushButton::clicked, this, &ReflectionInspector::OnResetAllClicked);
    ResetAllButton->setEnabled(false);
    ResetAllButton->setVisible(false);
    connect(this, &ReflectionInspector::PropertyChanged, this, [this]()
    {
        if (bRefreshQueued) { return; }
        bRefreshQueued = true;
        QTimer::singleShot(0, this, [this]() { bRefreshQueued = false; RefreshValues(); });
    });
}

void ReflectionInspector::SetInspectedObject(Object* Instance)
{
    InspectedIdentity = Instance ? Instance->GetObjectHandle() : ObjectHandle{};
    Rebuild();
}

Object* ReflectionInspector::GetInspectedObject() const
{
    auto* Memory = MemorySubsystem::Get();
    return Memory ? Memory->ResolveHandle(InspectedIdentity) : nullptr;
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

void ReflectionInspector::SetTypeLabelVisible(bool bVisible)
{
    TypeLabel->setVisible(bVisible);
}

bool ReflectionInspector::CommitProperty(const PropertyId& Property, const ReflectedValue& NewValue)
{
    if (GetInspectedObject() == nullptr)
    {
        return false;
    }
    bValuesDirty = true;
    if (CommitCallback)
    {
        return CommitCallback(GetInspectedObject(), Property, NewValue);
    }
    ReflectionDiagnostic SetResult = PropertyAccess::SetProperty(
        GetInspectedObject(),
        Property,
        NewValue,
        PropertyAccessContext::Inspector);
    return SetResult.bOk;
}

bool ReflectionInspector::CommitReset(const PropertyId& Property)
{
    if (GetInspectedObject() == nullptr)
    {
        return false;
    }
    bValuesDirty = true;
    if (ResetCallback)
    {
        return ResetCallback(GetInspectedObject(), Property);
    }
    ReflectionDiagnostic Reset = ResetPropertyToDefault(GetInspectedObject(), Property);
    return Reset.bOk;
}

void ReflectionInspector::Rebuild()
{
    const EditorUi::InspectorUpdateGuard Guard(this);
    ClearPropertyEditors();
    DisplayedValues.clear();
    bValuesDirty = false;

    if (GetInspectedObject() == nullptr || GetInspectedObject()->GetClass() == nullptr)
    {
        TypeLabel->setText(tr("Select an object in Hierarchy"));
        ResetAllButton->setEnabled(false);
        ResetAllButton->setVisible(false);
        return;
    }

    for (const auto* Property : GetInspectedObject()->GetClass()->GetAllProperties())
    {
        ReflectedValue Value;
        if (PropertyAccess::GetProperty(GetInspectedObject(), *Property, Value).bOk)
        {
            DisplayedValues.emplace_back(Property->Id, std::move(Value));
        }
    }
    TypeLabel->setText(QString::fromStdString(GetInspectedObject()->GetClass()->GetDisplayName()));
    TypeLabel->setToolTip(QString::fromStdString(GetInspectedObject()->GetClass()->GetTypeId().Value));
    ResetAllButton->setEnabled(true);
    ResetAllButton->setVisible(true);

    if (bMeshPresentation && BuildMeshPresentation())
    {
        ResetAllButton->hide();
        return;
    }

    for (const PropertyDescriptor* Descriptor : ListEditableProperties(GetInspectedObject()))
    {
        if (Descriptor != nullptr)
        {
            AddPropertyEditor(*Descriptor);
        }
    }
}

void ReflectionInspector::RefreshValues()
{
    auto* Instance = GetInspectedObject();
    if (!Instance || !Instance->GetClass())
    {
        if (!DisplayedValues.empty()) { Rebuild(); }
        return;
    }
    std::vector<std::pair<PropertyId, ReflectedValue>> Current;
    bool Changed = bValuesDirty;
    for (const auto* Property : Instance->GetClass()->GetAllProperties())
    {
        ReflectedValue Value;
        if (PropertyAccess::GetProperty(Instance, *Property, Value).bOk)
        {
            const size_t Index = Current.size();
            Changed = Changed || Index >= DisplayedValues.size() || DisplayedValues[Index].first != Property->Id
                || !EditorUi::SameInspectorValue(DisplayedValues[Index].second, Value);
            Current.emplace_back(Property->Id, std::move(Value));
        }
    }
    Changed = Changed || Current.size() != DisplayedValues.size();
    if (bMeshPresentation && MeshPresentationState != GetMeshPresentationState())
    {
        Changed = true;
    }
    if (!Changed) { return; }
    if (bMeshPresentation) { Rebuild(); return; }
    const EditorUi::InspectorUpdateGuard Guard(this);
    for (const auto* Descriptor : ListEditableProperties(Instance))
    {
        ReflectedValue Value;
        if (!PropertyAccess::GetProperty(Instance, *Descriptor, Value).bOk) { continue; }
        const auto Refresh = PropertyRefreshers.find(Descriptor->Id.Value);
        if (Refresh == PropertyRefreshers.end())
        {
            if (Value.ValueKind == ReflectedValue::Kind::String && Value.StringValue.empty()) { continue; }
            Rebuild();
            return;
        }
        if (!Refresh->second(Value)) { Rebuild(); return; }
    }
    DisplayedValues = std::move(Current);
    bValuesDirty = false;
}

void ReflectionInspector::ClearPropertyEditors()
{
    PropertyRefreshers.clear();
    while (PropertiesLayout->rowCount() > 0)
    {
        const auto Row = PropertiesLayout->takeRow(0);
        RetirePropertyEditor(Row.labelItem);
        RetirePropertyEditor(Row.fieldItem);
    }
}

std::vector<const PropertyDescriptor*> ReflectionInspector::ListEditableProperties(Object* Instance)
{
    std::vector<const PropertyDescriptor*> Result;
    if (Instance == nullptr || Instance->GetClass() == nullptr)
    {
        return Result;
    }

    for (const PropertyDescriptor* Property : Instance->GetClass()->GetAllProperties())
    {
        const PropertyDescriptor& Descriptor = *Property;
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
    if (CommitProperty(Property, ReflectedValue::MakeFloat64(Value)))
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

void ReflectionInspector::ApplyAssetValue(const PropertyDescriptor& Descriptor, const AssetKey& Key)
{
    const PropertyId Companion = Descriptor.Attributes.CompanionProperty != nullptr
        ? PropertyId{Descriptor.Attributes.CompanionProperty}
        : PropertyId{};
    bool bCommitted = false;
    if (AssetCallback)
    {
        bCommitted = AssetCallback(GetInspectedObject(), Descriptor.Id, Companion, Key);
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
        RefreshValues();
        emit PropertyChanged();
    }
}

void ReflectionInspector::OnResetAllClicked()
{
    if (GetInspectedObject() == nullptr)
    {
        return;
    }

    bool bAny = false;
    for (const PropertyDescriptor* Descriptor : ListEditableProperties(GetInspectedObject()))
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
        RefreshValues();
        emit PropertyChanged();
    }
}

void ReflectionInspector::AddPropertyEditor(const PropertyDescriptor& Descriptor)
{
    ReflectedValue CurrentValue;
    ReflectionDiagnostic Got = PropertyAccess::GetProperty(
        GetInspectedObject(),
        Descriptor,
        CurrentValue,
        PropertyAccessContext::Inspector);
    if (!Got.bOk)
    {
        return;
    }

    const QString Label = EditorUi::InspectorLabel(Descriptor.Attributes.DisplayName != nullptr
        ? QString::fromUtf8(Descriptor.Attributes.DisplayName)
        : QString::fromStdString(Descriptor.Id.Value));
    const bool bEditable = HasPropertyFlag(Descriptor.Attributes.Flags, PropertyFlags::EditorEditable)
        && !Descriptor.bReadOnly;
    const PropertyId Property = Descriptor.Id;

    // An empty read-only diagnostic is absence of a message, not an editable-looking blank field.
    if (!bEditable && CurrentValue.ValueKind == ReflectedValue::Kind::String && CurrentValue.StringValue.empty())
    { return; }

    QWidget* FieldWidget = new QWidget(this);
    FieldWidget->setObjectName("Property_" + QString::fromStdString(Property.Value));
    QHBoxLayout* FieldLayout = new QHBoxLayout(FieldWidget);
    FieldLayout->setContentsMargins(0, 0, 0, 0);

    if (CurrentValue.ValueKind == ReflectedValue::Kind::Struct
        || CurrentValue.ValueKind == ReflectedValue::Kind::Array
        || CurrentValue.ValueKind == ReflectedValue::Kind::TypeId
        || CurrentValue.ValueKind == ReflectedValue::Kind::ObjectReference
        || CurrentValue.ValueKind == ReflectedValue::Kind::AssetReference
        || CurrentValue.ValueKind == ReflectedValue::Kind::Enum
        || Descriptor.ValueTypeId.Value == "engine.Quaternion"
        || Descriptor.ValueTypeId.Value == "engine.EulerAngles")
    {
        Scene* World = nullptr;
        if (Component* InspectedComponent = dynamic_cast<Component*>(GetInspectedObject()))
        {
            if (GameObject* Owner = InspectedComponent->GetGameObject())
            {
                World = Owner->GetScene();
            }
        }
        auto* Editor = new ReflectionValueEditor(Descriptor.ValueTypeId, CurrentValue, Descriptor.Attributes,
            bEditable, Registry, World, [this, Property](const ReflectedValue& Updated)
            {
                const bool bCommitted = CommitProperty(Property, Updated);
                if (bCommitted)
                {
                    emit PropertyChanged();
                }
                return bCommitted;
            }, FieldWidget);
        PropertyRefreshers[Property.Value] = [Editor](const ReflectedValue& Updated)
        {
            Editor->SetValue(Updated);
            return true;
        };
        Editor->setObjectName(QString::fromStdString(Property.Value));
        FieldLayout->addWidget(Editor);
    }
    else if (Descriptor.Attributes.AssetTypeFilter != nullptr || !Descriptor.Attributes.AllowedAssetTypes.empty())
    {
        AssetKey CurrentKey{};
        Guid::TryParse(CurrentValue.StringValue, CurrentKey.Asset);
        if (Descriptor.Attributes.CompanionProperty != nullptr && GetInspectedObject()->GetClass() != nullptr)
        {
            ReflectedValue CompanionValue;
            const PropertyDescriptor* Companion = GetInspectedObject()->GetClass()->FindProperty(
                PropertyId{Descriptor.Attributes.CompanionProperty});
            if (Companion != nullptr
                && PropertyAccess::GetProperty(GetInspectedObject(), *Companion, CompanionValue, PropertyAccessContext::Inspector).bOk)
            {
                SubAssetId SubAsset{};
                if (Guid::TryParse(CompanionValue.StringValue, SubAsset) && SubAsset.IsValid())
                {
                    CurrentKey.SubAsset = SubAsset;
                }
            }
        }

        auto* Picker = new EditorUi::StudioSelect(FieldWidget);
        Picker->setEnabled(bEditable && Registry != nullptr);
        Picker->setAccessibleName(Label);
        Picker->addItem(tr("Не назначено"));
        if (Registry != nullptr)
        {
            AssetThumbnails::Bind(*Picker, 0, *Registry, {});
        }
        std::vector<AssetKey> Keys{AssetKey{}};
        int SelectedIndex = 0;
        auto AddOption = [&](const QString& Caption, const AssetKey& Key) {
            Picker->addItem(Caption);
            if (Registry != nullptr)
            {
                AssetThumbnails::Bind(*Picker, Picker->count() - 1, *Registry, Key);
            }
            Keys.push_back(Key);
            if (Key == CurrentKey) { SelectedIndex = Picker->count() - 1; }
        };
        AssetType Filter = UnknownAssetType;
        if (Descriptor.Attributes.AssetTypeFilter != nullptr)
        {
            TryParseAssetType(Descriptor.Attributes.AssetTypeFilter, Filter);
        }
        const auto AcceptsType = [&](const AssetType& Type)
        {
            return Type == Filter || std::find(Descriptor.Attributes.AllowedAssetTypes.begin(),
                Descriptor.Attributes.AllowedAssetTypes.end(), Type.GetIdentifier()) != Descriptor.Attributes.AllowedAssetTypes.end();
        };
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
            if (AcceptsType(Entry.Metadata.Type))
            {
                const AssetKey Key{Entry.Metadata.Guid, std::nullopt};
                AddOption(QString::fromStdString(Entry.VirtualPath), Key);
            }
            for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
            {
                if (!AcceptsType(SubAsset.Type))
                {
                    continue;
                }
                AssetKey Key{Entry.Metadata.Guid, SubAsset.Id};
                AddOption(QString::fromStdString(Entry.VirtualPath + " :: " + SubAsset.Name), Key);
            }
        }
        if (CurrentKey.IsValid() && SelectedIndex == 0)
        {
            AddOption(tr("Ресурс недоступен"), CurrentKey);
        }
        Picker->setCurrentIndex(SelectedIndex);
        Picker->setToolTip(Picker->currentText());
        connect(Picker, &QComboBox::activated, this, [this, Descriptor, Keys](int Index) {
            if (Index >= 0 && Index < static_cast<int>(Keys.size())) { ApplyAssetValue(Descriptor, Keys[Index]); }
        });
        if (Registry)
        {
            FieldLayout->addWidget(new AssetReferenceField(*Picker, *Registry), 1);
        }
        else
        {
            FieldLayout->addWidget(Picker, 1);
        }
    }
    else if (Descriptor.ValueTypeId.Value == "engine.bool")
    {
        QCheckBox* CheckBox = new EditorUi::StudioCheckBox(Label, FieldWidget);
        if (CheckBox->fontMetrics().horizontalAdvance(Label) > 160)
        {
            const qsizetype BreakPosition = Label.lastIndexOf(' ', Label.size() / 2);
            if (BreakPosition > 0)
            {
                QString WrappedLabel = Label;
                WrappedLabel[BreakPosition] = '\n';
                CheckBox->setText(WrappedLabel);
            }
        }
        CheckBox->setChecked(CurrentValue.BoolValue);
        CheckBox->setEnabled(bEditable);
        FieldLayout->addWidget(CheckBox, 1);
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
        const bool bNeedsWideInteger = CurrentValue.Int64Value < std::numeric_limits<int>::min()
            || CurrentValue.Int64Value > std::numeric_limits<int>::max()
            || (Descriptor.Attributes.bHasRange && (Descriptor.Attributes.RangeMinimum < std::numeric_limits<int>::min()
                || Descriptor.Attributes.RangeMaximum > std::numeric_limits<int>::max()));
        if (bNeedsWideInteger)
        {
            QLineEdit* IntegerEdit = new QLineEdit(QString::number(CurrentValue.Int64Value), FieldWidget);
            IntegerEdit->setEnabled(bEditable);
            FieldLayout->addWidget(IntegerEdit, 1);
            if (bEditable)
            {
                connect(IntegerEdit, &QLineEdit::editingFinished, this, [this, Property, IntegerEdit]()
                {
                    bool bValid = false;
                    const qlonglong Value = IntegerEdit->text().toLongLong(&bValid);
                    if (bValid && CommitProperty(Property, ReflectedValue::MakeInt64(Value)))
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
        else
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
    else if (Descriptor.ValueTypeId.Value == "engine.string" && !bEditable)
    {
        auto* Message = new QLabel(QString::fromStdString(CurrentValue.StringValue), FieldWidget);
        Message->setWordWrap(true);
        Message->setTextInteractionFlags(Qt::TextSelectableByMouse);
        FieldLayout->addWidget(Message, 1);
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
    else
    {
        QLabel* Unsupported = new QLabel(tr("unsupported"), FieldWidget);
        FieldLayout->addWidget(Unsupported);
    }

    if (bEditable)
    {
        QPushButton* ResetButton = new QPushButton(EditorUi::MakeStudioIcon(EditorUi::StudioIcon::Rotate), {}, FieldWidget);
        ResetButton->setProperty("uiRole", "propertyReset");
        ResetButton->setToolTip(tr("Сбросить свойство"));
        ResetButton->setAccessibleName(tr("Reset property to default"));
        ResetButton->setFixedWidth(24);
        FieldLayout->addWidget(ResetButton);
        connect(ResetButton, &QPushButton::clicked, this, [this, Property]()
        {
            OnResetPropertyClicked(Property);
        });
    }

    if (PropertyRefreshers.find(Property.Value) == PropertyRefreshers.end())
    {
        PropertyRefreshers[Property.Value] = [FieldWidget, Descriptor](const ReflectedValue& Updated)
        {
            return RefreshSimpleField(FieldWidget, Descriptor, Updated);
        };
    }
    for (QAbstractSpinBox* NumericEditor : FieldWidget->findChildren<QAbstractSpinBox*>())
    {
        NumericEditor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        NumericEditor->setMinimumWidth(55);
    }
    if (Descriptor.ValueTypeId.Value == "engine.bool")
    {
        PropertiesLayout->addRow(FieldWidget);
    }
    else
    {
        // A spanning property column avoids QFormLayout's narrow label role
        // wrapping short captions even when WrapAllRows has ample full-row space.
        auto* Column = new QWidget(this);
        auto* ColumnLayout = new QVBoxLayout(Column);
        ColumnLayout->setContentsMargins(0, 0, 0, 0);
        ColumnLayout->setSpacing(4);
        QLabel* PropertyLabel = new QLabel(Label, Column);
        PropertyLabel->setWordWrap(true);
        if (OverrideMarker && OverrideMarker(GetInspectedObject(), Descriptor.Id))
        {
            QFont Font = PropertyLabel->font();
            Font.setBold(true);
            PropertyLabel->setFont(Font);
            PropertyLabel->setToolTip(tr("Prefab override"));
        }
        ColumnLayout->addWidget(PropertyLabel);
        ColumnLayout->addWidget(FieldWidget);
        PropertiesLayout->addRow(Column);
    }
}
