#pragma once
#include "UI/Views/StudioControls.h"
#include "UI/Presentation/InspectorLabels.h"
#include "UI/Adapters/InspectorUpdateGuard.h"
#include "UI/Presentation/InspectorValue.h"

#include "Assets/AssetRegistry.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/ReflectionValueCodec.h"
#include "World/GameObject.h"
#include "World/Scene.h"

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <unordered_set>

class ReflectionValueEditor : public QWidget
{
public:
    using CommitFunction = std::function<bool(const ReflectedValue&)>;

    ReflectionValueEditor(TypeId Type, ReflectedValue Value, PropertyAttributes Attributes,
        bool bEditable, AssetRegistry* Assets, Scene* World, CommitFunction Commit, QWidget* Parent)
        : QWidget(Parent), Type(std::move(Type)), Value(std::move(Value)), Attributes(std::move(Attributes)),
          bEditable(bEditable), Assets(Assets), World(World), Commit(std::move(Commit))
    {
        Layout = new QVBoxLayout(this);
        Layout->setContentsMargins(0, 0, 0, 0);
        Rebuild();
    }

    void SetValue(const ReflectedValue& Updated)
    {
        if (EditorUi::SameInspectorValue(Value, Updated)) { return; }
        Value = Updated;
        Rebuild();
    }

private:
    ReflectedValue& AtPath(ReflectedValue& Root, const std::vector<size_t>& Path)
    {
        ReflectedValue* Current = &Root;
        for (size_t Index : Path)
        {
            Current = &Current->Values.at(Index);
        }
        return *Current;
    }

    void Change(const std::vector<size_t>& Path, const ReflectedValue& Updated, bool bRebuild = false)
    {
        ReflectedValue Candidate = Value;
        AtPath(Candidate, Path) = Updated;
        if (Commit(Candidate))
        {
            Value = std::move(Candidate);
        }
        else
        {
            bRebuild = true;
        }
        if (bRebuild)
        {
            QTimer::singleShot(0, this, [this]() { Rebuild(); });
        }
    }

    void Rebuild()
    {
        const EditorUi::InspectorUpdateGuard Guard(this);
        while (QLayoutItem* Item = Layout->takeAt(0))
        {
            if (auto* Widget = Item->widget())
            {
                Widget->blockSignals(true);
                for (auto* Child : Widget->findChildren<QWidget*>()) { Child->blockSignals(true); }
                Widget->hide();
                Widget->deleteLater();
            }
            delete Item;
        }
        Layout->addWidget(Build(Type, {}, Attributes, bEditable));
    }

    QWidget* Build(const TypeId& ValueType, const std::vector<size_t>& Path,
        const PropertyAttributes& FieldAttributes, bool bCanEdit)
    {
        const ReflectedValue Current = AtPath(Value, Path);
        QWidget* Container = new QWidget(this);
        Container->setEnabled(bCanEdit);
        auto* Fields = new QFormLayout(Container);
        Fields->setContentsMargins(0, 0, 0, 0);
        Fields->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        if (Current.ValueKind == ReflectedValue::Kind::Struct)
        {
            if (ValueType.Value == "engine.Color")
            {
                auto* Picker = new QPushButton(tr("Choose color"), Container);
                Fields->addRow(Picker);
                connect(Picker, &QPushButton::clicked, this, [this, Path]()
                {
                    ReflectedValue Updated = AtPath(Value, Path);
                    const QColor Initial = QColor::fromRgbF(Updated.FindField("r")->Float64Value,
                        Updated.FindField("g")->Float64Value, Updated.FindField("b")->Float64Value,
                        Updated.FindField("a")->Float64Value);
                    const QColor Selected = QColorDialog::getColor(Initial, this, tr("Color"), QColorDialog::ShowAlphaChannel);
                    if (Selected.isValid())
                    {
                        const double Channels[] = {Selected.redF(), Selected.greenF(), Selected.blueF(), Selected.alphaF()};
                        for (size_t ChannelIndex = 0; ChannelIndex < Updated.Values.size(); ++ChannelIndex)
                        {
                            Updated.Values[ChannelIndex] = ReflectedValue::MakeFloat64(Channels[ChannelIndex]);
                        }
                        Change(Path, Updated, true);
                    }
                });
            }
            for (size_t FieldIndex = 0; FieldIndex < Current.Values.size(); ++FieldIndex)
            {
                const PropertyDescriptor* Field = ReflectionSubsystem::Get().FindStructProperty(
                    ValueType, PropertyId{Current.FieldNames[FieldIndex]});
                if (Field == nullptr || Field->Attributes.bEditorHidden)
                {
                    continue;
                }
                auto ChildPath = Path;
                ChildPath.push_back(FieldIndex);
                QString Label = QString::fromStdString(Field->Id.Value);
                if (Field->Attributes.DisplayName != nullptr)
                {
                    Label = QString::fromUtf8(Field->Attributes.DisplayName);
                }
                Fields->addRow(EditorUi::InspectorLabel(Label), Build(Field->ValueTypeId, ChildPath, Field->Attributes,
                    bCanEdit && !Field->bReadOnly && HasPropertyFlag(Field->Attributes.Flags, PropertyFlags::EditorEditable)));
            }
        }
        else if (Current.ValueKind == ReflectedValue::Kind::Array)
        {
            for (size_t ElementIndex = 0; ElementIndex < Current.Values.size(); ++ElementIndex)
            {
                auto ChildPath = Path;
                ChildPath.push_back(ElementIndex);
                auto* Group = new QGroupBox(QString::number(ElementIndex + 1), Container);
                auto* GroupLayout = new QVBoxLayout(Group);
                GroupLayout->addWidget(Build(Current.TypeIdValue, ChildPath, {}, bCanEdit));
                for (int Direction : {-1, 1})
                {
                    QString Label = tr("Move up");
                    if (Direction > 0)
                    {
                        Label = tr("Move down");
                    }
                    auto* Move = new QPushButton(Label, Group);
                    Move->setEnabled(bCanEdit && (Direction < 0 || ElementIndex + 1 < Current.Values.size())
                        && (Direction > 0 || ElementIndex > 0));
                    GroupLayout->addWidget(Move);
                    connect(Move, &QPushButton::clicked, this, [this, Path, ElementIndex, Direction]()
                    {
                        ReflectedValue Updated = AtPath(Value, Path);
                        const size_t Destination = static_cast<size_t>(static_cast<int64_t>(ElementIndex) + Direction);
                        std::swap(Updated.Values[ElementIndex], Updated.Values[Destination]);
                        Change(Path, Updated, true);
                    });
                }
                auto* Remove = new QPushButton(tr("Remove"), Group);
                GroupLayout->addWidget(Remove);
                connect(Remove, &QPushButton::clicked, this, [this, Path, ElementIndex]()
                {
                    ReflectedValue Updated = AtPath(Value, Path);
                    Updated.Values.erase(Updated.Values.begin() + ElementIndex);
                    Change(Path, Updated, true);
                });
                Fields->addRow(Group);
            }
            auto* Add = new QPushButton(tr("Добавить"), Container);
            Fields->addRow(Add);
            connect(Add, &QPushButton::clicked, this, [this, Path]()
            {
                ReflectedValue Updated = AtPath(Value, Path);
                ReflectedValue Element;
                if (MakeDefaultReflectionValue(Updated.TypeIdValue, Element).bOk)
                {
                    for (size_t FieldIndex = 0; FieldIndex < Element.FieldNames.size(); ++FieldIndex)
                    {
                        const PropertyDescriptor* Field = ReflectionSubsystem::Get().FindStructProperty(
                            Updated.TypeIdValue, PropertyId{Element.FieldNames[FieldIndex]});
                        if (Field != nullptr && Field->Attributes.bCollectionIndex)
                        {
                            std::unordered_set<int64_t> UsedIndices;
                            for (const ReflectedValue& Existing : Updated.Values)
                            {
                                UsedIndices.insert(Existing.FindField(Field->Id.Value)->Int64Value);
                            }
                            while (UsedIndices.count(Element.Values[FieldIndex].Int64Value) != 0)
                            {
                                ++Element.Values[FieldIndex].Int64Value;
                            }
                        }
                    }
                    Updated.Values.push_back(std::move(Element));
                    Change(Path, Updated, true);
                }
            });
        }
        else if (Current.ValueKind == ReflectedValue::Kind::Bool)
        {
            auto* Editor = new EditorUi::StudioCheckBox(Container);
            Editor->setChecked(Current.BoolValue);
            Fields->addRow(Editor);
            connect(Editor, &QCheckBox::toggled, this, [this, Path](bool Updated)
            {
                Change(Path, ReflectedValue::MakeBool(Updated));
            });
        }
        else if (Current.ValueKind == ReflectedValue::Kind::Float64 || Current.ValueKind == ReflectedValue::Kind::Bytes)
        {
            size_t ChannelCount = 1;
            if (Current.ValueKind == ReflectedValue::Kind::Bytes)
            {
                ChannelCount = Current.BytesValue.size() / sizeof(float);
            }
            for (size_t ChannelIndex = 0; ChannelIndex < ChannelCount; ++ChannelIndex)
            {
                auto* Editor = new QDoubleSpinBox(Container);
                Editor->setDecimals(6);
                Editor->setRange(-1.e12, 1.e12);
                if (FieldAttributes.bHasRange)
                {
                    Editor->setRange(FieldAttributes.RangeMinimum, FieldAttributes.RangeMaximum);
                }
                double Initial = Current.Float64Value;
                if (Current.ValueKind == ReflectedValue::Kind::Bytes)
                {
                    float Channel = 0.f;
                    std::memcpy(&Channel, Current.BytesValue.data() + ChannelIndex * sizeof(float), sizeof(float));
                    Initial = Channel;
                }
                Editor->setValue(Initial);
                if (Current.ValueKind == ReflectedValue::Kind::Bytes)
                {
                    const char* ChannelNames[] = {"X", "Y", "Z", "W"};
                    if (ValueType.Value == "engine.EulerAngles")
                    {
                        const char* AngleNames[] = {"Pitch", "Yaw", "Roll"};
                        Editor->setSuffix(QString(QChar(0x00b0)));
                        Fields->addRow(QString::fromLatin1(AngleNames[ChannelIndex]), Editor);
                    }
                    else
                    {
                        Fields->addRow(QString::fromLatin1(ChannelNames[ChannelIndex]), Editor);
                    }
                }
                else
                {
                    Fields->addRow(Editor);
                }
                connect(Editor, qOverload<double>(&QDoubleSpinBox::valueChanged), this, [this, Path, ChannelIndex](double Selected)
                {
                    ReflectedValue Updated = AtPath(Value, Path);
                    if (Updated.ValueKind == ReflectedValue::Kind::Bytes)
                    {
                        const float Channel = static_cast<float>(Selected);
                        std::memcpy(Updated.BytesValue.data() + ChannelIndex * sizeof(float), &Channel, sizeof(float));
                    }
                    else
                    {
                        Updated.Float64Value = Selected;
                    }
                    Change(Path, Updated);
                });
            }
        }
        else if (Current.ValueKind == ReflectedValue::Kind::Int64 || Current.ValueKind == ReflectedValue::Kind::String)
        {
            QString Initial = QString::fromStdString(Current.StringValue);
            if (Current.ValueKind == ReflectedValue::Kind::Int64)
            {
                Initial = QString::number(Current.Int64Value);
            }
            auto* Editor = new QLineEdit(Initial, Container);
            Fields->addRow(Editor);
            connect(Editor, &QLineEdit::editingFinished, this, [this, Path, Editor]()
            {
                ReflectedValue Updated = AtPath(Value, Path);
                if (Updated.ValueKind == ReflectedValue::Kind::Int64)
                {
                    bool bValid = false;
                    Updated.Int64Value = Editor->text().toLongLong(&bValid);
                    if (!bValid)
                    {
                        QTimer::singleShot(0, this, [this]() { Rebuild(); });
                        return;
                    }
                }
                else
                {
                    Updated.StringValue = Editor->text().toStdString();
                }
                Change(Path, Updated);
            });
        }
        else
        {
            auto* Editor = new EditorUi::StudioSelect(Container);
            Editor->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            Editor->setMinimumWidth(70);
            Editor->addItem(tr("None"), QString());
            std::vector<ReflectedValue> Options(1, Current);
            Options.front() = {};
            if (Current.ValueKind == ReflectedValue::Kind::TypeId)
            {
                Options.front() = ReflectedValue::MakeTypeId({});
                std::string Base;
                const std::string Prefix = "engine.ClassRef<";
                if (ValueType.Value.find(Prefix) == 0)
                {
                    Base = ValueType.Value.substr(Prefix.size(), ValueType.Value.size() - Prefix.size() - 1);
                }
                for (Class* Entry : ReflectionSubsystem::Get().GetPublishedCatalog().Classes)
                {
                    if (Base.empty() || Entry->IsA(TypeId{Base}))
                    {
                        Editor->addItem(QString::fromStdString(Entry->GetTypeId().Value));
                        Options.push_back(ReflectedValue::MakeTypeId(Entry->GetTypeId()));
                    }
                }
                if (Base.empty())
                {
                    for (TypeDescriptor* Entry : ReflectionSubsystem::Get().GetPublishedCatalog().Types)
                    {
                        Editor->addItem(QString::fromStdString(Entry->Id.Value));
                        Options.push_back(ReflectedValue::MakeTypeId(Entry->Id));
                    }
                }
            }
            else if (Current.ValueKind == ReflectedValue::Kind::Enum)
            {
                Editor->clear();
                Options.clear();
                for (const auto& Entry : ReflectionSubsystem::Get().FindEnum(ValueType)->Values)
                {
                    const QString Name = QString::fromStdString(Entry.Name);
                    Editor->addItem(EditorLocalization::Get().Text("Editor.Enum." + QString::fromStdString(ValueType.Value) + "." + Name, Name));
                    ReflectedValue Option = Current;
                    Option.Int64Value = Entry.Value;
                    Options.push_back(std::move(Option));
                }
            }
            else if (Current.ValueKind == ReflectedValue::Kind::ObjectReference)
            {
                ReadReflectionObject({}, Options.front());
                if (World != nullptr)
                {
                    for (GameObject* Entry : World->GetAllObjects())
                    {
                        ReflectedValue Option;
                        ReadReflectionObject(Entry->GetObjectHandle(), Option);
                        Editor->addItem(QString::fromStdString(Entry->GetName()));
                        Options.push_back(Option);
                        for (Component* ComponentEntry : Entry->GetAllComponents())
                        {
                            ReadReflectionObject(ComponentEntry->GetObjectHandle(), Option);
                            Editor->addItem(QString::fromStdString(Entry->GetName() + "/" + ComponentEntry->GetName()));
                            Options.push_back(Option);
                        }
                    }
                }
            }
            else if (Current.ValueKind == ReflectedValue::Kind::AssetReference)
            {
                ReflectionValueCodec<AssetKey>::ToReflected({}, Options.front());
                if (Assets != nullptr)
                {
                    auto Entries = Assets->FindByDirectory("/Game");
                    const auto EngineEntries = Assets->FindByDirectory("/Engine");
                    Entries.insert(Entries.end(), EngineEntries.begin(), EngineEntries.end());
                    const auto AcceptsType = [&FieldAttributes](const AssetType& AssetKind)
                    {
                        return (FieldAttributes.AssetTypeFilter == nullptr && FieldAttributes.AllowedAssetTypes.empty())
                            || (FieldAttributes.AssetTypeFilter != nullptr && AssetKind.GetIdentifier() == FieldAttributes.AssetTypeFilter)
                            || std::find(FieldAttributes.AllowedAssetTypes.begin(), FieldAttributes.AllowedAssetTypes.end(),
                                AssetKind.GetIdentifier()) != FieldAttributes.AllowedAssetTypes.end();
                    };
                    for (const AssetRegistryEntry& Entry : Entries)
                    {
                        ReflectedValue Option;
                        if (AcceptsType(Entry.Metadata.Type))
                        {
                            ReflectionValueCodec<AssetKey>::ToReflected(AssetKey{Entry.Metadata.Guid, {}}, Option);
                            Editor->addItem(QString::fromStdString(Entry.VirtualPath));
                            Options.push_back(Option);
                        }
                        for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
                        {
                            if (AcceptsType(SubAsset.Type))
                            {
                                ReflectionValueCodec<AssetKey>::ToReflected(AssetKey{Entry.Metadata.Guid, SubAsset.Id}, Option);
                                Editor->addItem(QString::fromStdString(Entry.VirtualPath + "/" + SubAsset.Name));
                                Options.push_back(Option);
                            }
                        }
                    }
                }
            }
            bool bMatched = false;
            for (size_t OptionIndex = 0; OptionIndex < Options.size(); ++OptionIndex)
            {
                const ReflectedValue& Option = Options[OptionIndex];
                if (Option.TypeIdValue == Current.TypeIdValue && Option.Int64Value == Current.Int64Value
                    && Option.ObjectValue == Current.ObjectValue && Option.AssetValue == Current.AssetValue)
                {
                    Editor->setCurrentIndex(static_cast<int>(OptionIndex));
                    bMatched = true;
                }
            }
            if (!bMatched)
            {
                Editor->addItem(tr("Unavailable value"));
                Options.push_back(Current);
                Editor->setCurrentIndex(static_cast<int>(Options.size() - 1));
            }
            Fields->addRow(Editor);
            connect(Editor, qOverload<int>(&QComboBox::currentIndexChanged), this, [this, Path, Options](int Selected)
            {
                if (Selected >= 0)
                {
                    Change(Path, Options.at(static_cast<size_t>(Selected)));
                }
            });
        }
        return Container;
    }

    TypeId Type;
    ReflectedValue Value;
    PropertyAttributes Attributes;
    bool bEditable;
    AssetRegistry* Assets;
    Scene* World;
    CommitFunction Commit;
    QVBoxLayout* Layout;
};
