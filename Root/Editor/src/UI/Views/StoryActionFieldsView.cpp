#include "UI/Views/StudioControls.h"
#include "UI/Views/StoryActionFieldsView.h"
#include "UI/Styles/StoryEventStyle.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QSignalBlocker>

namespace EditorUi
{
StoryActionFieldsView::StoryActionFieldsView(StoryGraphAction Action, const StoryGraphModel& Model, QWidget* Parent)
    : QWidget(Parent), Value(std::move(Action))
{
    setObjectName("StoryEventActionFields");
    auto* Form = new QFormLayout(this);
    Form->setContentsMargins(StoryEventStyle::FieldsMargins());
    Form->setHorizontalSpacing(StoryEventStyle::LabelGap);
    Form->setVerticalSpacing(StoryEventStyle::FieldGap);
    Form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    Form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    auto* Type = new EditorUi::StudioSelect(this);
    Type->setObjectName("StoryEventActionKind");
    Type->setMinimumContentsLength(4);
    Type->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    Type->addItems(StoryEventStyle::ActionTypes());
    Type->setCurrentIndex(Value.Kind);
    Form->addRow(tr("Тип"), Type);
    connect(Type, &QComboBox::currentIndexChanged, this, [this](int Kind) {
        Commit();
        Value.Kind = Kind;
        if (Changed)
        {
            Changed(Value);
        }
    });
    auto Spin = [&](const QString& Name, double Initial, std::function<void(double)> Set, bool Positive = false) {
        auto* Field = new QDoubleSpinBox(this);
        Field->setObjectName(Name);
        Field->setButtonSymbols(QAbstractSpinBox::NoButtons);
        Field->setRange(Positive ? 0 : -1000000, 1000000);
        Field->setDecimals(3);
        Field->setValue(Initial);
        Field->setProperty("eventEdited", false);
        connect(Field, qOverload<double>(&QDoubleSpinBox::valueChanged), Field,
                [Field]() { Field->setProperty("eventEdited", true); });
        Collectors.push_back([Field, Set]() {
            Field->interpretText();
            if (!Field->property("eventEdited").toBool())
            {
                return;
            }
            Field->setProperty("eventEdited", false);
            Set(Field->value());
        });
        connect(Field, &QDoubleSpinBox::editingFinished, this, &StoryActionFieldsView::Commit);
        return Field;
    };
    Form->addRow(
        Value.Kind == 3 ? tr("Тайм-аут, с") : tr("Длительность, с"),
        Spin("StoryEventActionDuration", Value.Duration, [this](double Number) { Value.Duration = Number; }, true));
    if (Value.Kind == 2)
    {
        auto* Copy = new EditorUi::StudioCheckBox(tr("Положение из объекта"), this);
        Copy->setChecked(Value.bCopyTarget);
        Form->addRow(Copy);
        connect(Copy, &QCheckBox::toggled, this, [this](bool Checked) {
            Commit();
            Value.bCopyTarget = Checked;
            if (Changed)
            {
                Changed(Value);
            }
        });
    }
    if (Value.Kind != 0 && (Value.Kind != 2 || Value.bCopyTarget))
    {
        auto* Target = new EditorUi::StudioSelect(this);
        Target->setObjectName("StoryEventActionTarget");
        Target->setMinimumContentsLength(4);
        Target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        Target->addItem(tr("Выберите объект"), QString{});
        for (const auto& Object : Model.Objects)
        {
            Target->addItem(Object.second, Object.first);
        }
        int Index = Target->findData(Value.Target);
        if (Index < 0 || !Value.LegacyTarget.isEmpty())
        {
            Target->addItem(Value.LegacyTarget.isEmpty() ? tr("Объект недоступен") : Value.LegacyTarget, Value.Target);
            Index = Target->count() - 1;
        }
        Target->setCurrentIndex(Index);
        Form->addRow(tr("Объект"), Target);
        connect(Target, &QComboBox::currentIndexChanged, this, [this, Target]() {
            Commit();
            Value.Target = Target->currentData().toString();
            Value.LegacyTarget.clear();
            if (Changed)
            {
                Changed(Value);
            }
        });
    }
    if (Value.Kind == 4)
    {
        auto* Animation = new EditorUi::StudioSelect(this);
        Animation->setObjectName("StoryEventAnimationId");
        Animation->addItem(tr("Выберите анимацию"), QString{});
        for (const auto& Choice : Model.CharacterAnimations.value(Value.Target))
        {
            Animation->addItem(Choice.second, Choice.first);
        }
        if (!Value.AnimationId.isEmpty() && Animation->findData(Value.AnimationId) < 0)
        {
            Animation->addItem(tr("Недоступная анимация: ") + Value.AnimationId, Value.AnimationId);
        }
        Animation->setCurrentIndex(std::max(0, Animation->findData(Value.AnimationId)));
        Form->addRow(tr("Анимация"), Animation);
        connect(Animation, &QComboBox::currentIndexChanged, this, [this, Animation]()
        {
            Value.AnimationId = Animation->currentData().toString();
            if (Changed)
            {
                Changed(Value);
            }
        });
    }
    if (Value.Kind != 0 && Value.Kind != 4 && (Value.Kind != 2 || !Value.bCopyTarget))
    {
        const QStringList Axes = {"X", "Y", "Z"};
        auto* Coordinates = new QWidget(this);
        auto* CoordinateRow = new QHBoxLayout(Coordinates);
        CoordinateRow->setContentsMargins(0, 0, 0, 0);
        CoordinateRow->setSpacing(StoryEventStyle::CoordinateGap);
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            auto* Cell = new QVBoxLayout();
            Cell->setSpacing(StoryEventStyle::GroupControlGap);
            auto* Label = new QLabel(Axes[Axis], Coordinates);
            Label->setProperty("uiRole", "coordinateAxis");
            Cell->addWidget(Label);
            auto* Position = Spin(QStringLiteral("StoryEventPosition%1").arg(Axis), Value.Position[Axis],
                                  [this, Axis](double Number) { Value.Position[Axis] = Number; });
            Position->setMinimumWidth(0);
            Cell->addWidget(Position);
            CoordinateRow->addLayout(Cell);
        }
        Form->addRow(tr("Положение"), Coordinates);
        if (Value.Kind == 2)
        {
            auto* Rotate = new EditorUi::StudioCheckBox(tr("Задать поворот"), this);
            Rotate->setChecked(Value.bHasRotation);
            Form->addRow(Rotate);
            connect(Rotate, &QCheckBox::toggled, this, [this](bool Checked) {
                Commit();
                Value.bHasRotation = Checked;
                if (Changed)
                {
                    Changed(Value);
                }
            });
            if (Value.bHasRotation)
            {
                for (int Axis = 0; Axis < 3; ++Axis)
                {
                    Form->addRow(tr("Поворот ") + Axes[Axis] + QString::fromUtf8(" °"),
                                 Spin(QStringLiteral("StoryEventRotation%1").arg(Axis), Value.RotationDegrees[Axis],
                                      [this, Axis](double Number) {
                                          Value.RotationDegrees[Axis] = Number;
                                          Value.bRotationEdited = true;
                                      }));
                }
            }
        }
    }
}
void StoryActionFieldsView::Commit()
{
    bool Edited = false;
    for (auto* Spin : findChildren<QDoubleSpinBox*>())
    {
        Spin->interpretText();
        Edited |= Spin->property("eventEdited").toBool();
    }
    if (!Edited)
    {
        return;
    }
    for (const auto& Collect : Collectors)
    {
        Collect();
    }
    if (Changed)
    {
        Changed(Value);
    }
}
} // namespace EditorUi
