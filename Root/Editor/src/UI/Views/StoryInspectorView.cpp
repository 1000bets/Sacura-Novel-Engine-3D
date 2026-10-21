#include "UI/Views/StoryInspectorView.h"
#include "UI/Styles/StudioIcons.h"
#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace EditorUi
{
StoryInspectorView::StoryInspectorView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("StoryInspector");
    auto* Outer = new QVBoxLayout(this);
    Outer->setContentsMargins(0, 0, 0, 0);
    auto* Scroll = new QScrollArea(this);
    Scroll->setWidgetResizable(true);
    Scroll->setFrameShape(QFrame::NoFrame);
    auto* Body = new QWidget(Scroll);
    Body->setObjectName("StoryInspectorBody");
    auto* Layout = new QVBoxLayout(Body);
    Layout->setContentsMargins(16, 0, 16, 16);
    Layout->setSpacing(12);
    Title = new QLabel(Body);
    Title->setObjectName("InspectorIdentity");
    Title->setWordWrap(true);
    Layout->addWidget(Title);
    auto* Form = new QFormLayout();
    Form->setHorizontalSpacing(12);
    Form->setVerticalSpacing(10);
    Kind = new QComboBox(Body);
    Kind->setObjectName("StoryNodeKind");
    Kind->addItems({tr("Реплика"), tr("Выбор"), tr("Действие"), tr("Концовка")});
    Speaker = new QComboBox(Body);
    Speaker->setObjectName("StorySpeaker");
    Speaker->setEditable(true);
    Speaker->setInsertPolicy(QComboBox::NoInsert);
    Form->addRow(tr("Тип блока"), Kind);
    KindCaption = Form->labelForField(Kind);
    Form->addRow(tr("Говорит"), Speaker);
    SpeakerCaption = Form->labelForField(Speaker);
    Layout->addLayout(Form);
    Text = new QTextEdit(Body);
    Text->setObjectName("StoryText");
    Text->setAcceptRichText(false);
    Text->setUndoRedoEnabled(false);
    Text->setFixedHeight(136);
    Layout->addWidget(Text);
    Start = new QCheckBox(tr("Начало истории"), Body);
    Start->setObjectName("StoryStart");
    Layout->addWidget(Start);
    Details = new QWidget(Body);
    DetailsLayout = new QVBoxLayout(Details);
    DetailsLayout->setContentsMargins(0, 0, 0, 0);
    DetailsLayout->setSpacing(12);
    Layout->addWidget(Details);
    Layout->addStretch();
    Scroll->setWidget(Body);
    Outer->addWidget(Scroll);
    TextTimer = new QTimer(this);
    TextTimer->setSingleShot(true);
    TextTimer->setInterval(400);
    connect(TextTimer, &QTimer::timeout, this, &StoryInspectorView::Commit);
    Text->installEventFilter(this);
    Kind->installEventFilter(this);
    Speaker->installEventFilter(this);
    Speaker->lineEdit()->installEventFilter(this);
    connect(Text, &QTextEdit::textChanged, this,
            [this]()
            {
                if (!bUpdating)
                {
                    TextTimer->start();
                }
            });
    connect(Speaker->lineEdit(), &QLineEdit::editingFinished, this, &StoryInspectorView::Commit);
    connect(Speaker, &QComboBox::textActivated, this, [this]() { Commit(); });
    connect(Kind, &QComboBox::currentIndexChanged, this,
            [this](int Value)
            {
                if (bUpdating)
                {
                    return;
                }
                const int PreviousKind = Current.KindId;
                const QString PreviousTarget = Current.Targets.value(0);
                Current.KindId = Value;
                if (Value == 1 && Current.Choices.isEmpty())
                {
                    Current.Choices = {{tr("Вариант 1"), {}}, {tr("Вариант 2"), {}}};
                }
                if (Value == 2 && Current.Actions.isEmpty())
                {
                    Current.Actions.push_back({});
                }
                Current.Targets =
                    Value == 0 || Value == 2
                        ? QVector<QString>{PreviousKind == 0 || PreviousKind == 2 ? PreviousTarget : QString{}}
                        : QVector<QString>{};
                SetPendingStructure();
                Commit();
            });
    connect(Start, &QCheckBox::toggled, this,
            [this](bool bChecked)
            {
                if (!bUpdating && bChecked && StartRequested)
                {
                    Commit();
                    StartRequested(Current.Identity);
                }
            });
    Clear();
}

void StoryInspectorView::SetPendingStructure()
{
    bStructurePending = true;
    Details->setEnabled(false);
}

void StoryInspectorView::SetNode(const StoryGraphNode& Node, const StoryGraphModel& Model, bool bForce)
{
    Context = Model;
    QWidget* Focus = QApplication::focusWidget();
    if (!bForce && !bStructurePending && Current.Identity == Node.Identity && Current.KindId == Node.KindId &&
        Focus != nullptr && isAncestorOf(Focus) && isEnabled() == !Model.bReadOnly)
    {
        return;
    }
    const bool bSameText = Text->toPlainText() == Node.Text;
    const auto Cursor = Text->textCursor();
    bUpdating = true;
    TextTimer->stop();
    Current = Node;
    Current.bStart = Node.Identity == Model.Start;
    Title->setText(Node.Kind + QStringLiteral("     ") + Model.Title);
    Kind->setCurrentIndex(Node.KindId);
    Kind->show();
    KindCaption->show();
    Start->show();
    Details->show();
    Speaker->clear();
    Speaker->addItem(tr("Рассказчик"));
    for (const auto& Candidate : Model.Nodes)
    {
        if (!Candidate.Speaker.isEmpty() && Speaker->findText(Candidate.Speaker) < 0)
        {
            Speaker->addItem(Candidate.Speaker);
        }
    }
    Speaker->setCurrentText(Node.Speaker);
    Speaker->setVisible(Node.KindId == 0);
    SpeakerCaption->setVisible(Node.KindId == 0);
    if (!bSameText)
    {
        Text->setPlainText(Node.Text);
    }
    else
    {
        Text->setTextCursor(Cursor);
    }
    Text->setVisible(Node.KindId == 0 || Node.KindId == 1);
    Text->setPlaceholderText(Node.KindId == 1 ? tr("Вопрос игроку") : tr("Текст реплики"));
    Start->setChecked(Current.bStart);
    Start->setEnabled(!Current.bStart);
    bStructurePending = false;
    RebuildDetails();
    Details->setEnabled(true);
    setEnabled(Model.bOpen && !Model.bReadOnly);
    bUpdating = false;
}

void StoryInspectorView::Commit()
{
    TextTimer->stop();
    if (bUpdating || bCollecting || Current.Identity.isEmpty() || Context.bReadOnly || !Context.bOpen)
    {
        return;
    }
    // Ctrl+S/Undo can arrive before a field loses focus. Collect only actual user
    // edits; focusing a rounded numeric display must never change stored precision.
    bCollecting = true;
    for (auto* Spin : Details->findChildren<QDoubleSpinBox*>())
    {
        if (Spin->property("storyEdited").toBool())
        {
            Spin->interpretText();
            QMetaObject::invokeMethod(Spin, "editingFinished", Qt::DirectConnection);
        }
    }
    for (auto* Field : Details->findChildren<QLineEdit*>())
    {
        if (Field->objectName().startsWith(QStringLiteral("StoryChoiceLabel")) && Field->isModified())
        {
            QMetaObject::invokeMethod(Field, "editingFinished", Qt::DirectConnection);
        }
    }
    bCollecting = false;
    Current.Text = Text->toPlainText();
    Current.Speaker = Speaker->currentText();
    Current.Kind = Kind->currentText();
    if (Current.KindId == 1)
    {
        Current.Targets.clear();
        for (const auto& Choice : Current.Choices)
        {
            Current.Targets.push_back(Choice.Target);
        }
    }
    if (NodeChanged)
    {
        NodeChanged(Current);
    }
}

void StoryInspectorView::Clear()
{
    bUpdating = true;
    TextTimer->stop();
    Current = {};
    Title->setText(tr("Выберите ноду истории"));
    Text->clear();
    for (auto* Field : {static_cast<QWidget*>(Kind), KindCaption, static_cast<QWidget*>(Speaker), SpeakerCaption,
                        static_cast<QWidget*>(Text), static_cast<QWidget*>(Start), Details})
    {
        Field->hide();
    }
    setEnabled(false);
    bUpdating = false;
}

QComboBox* StoryInspectorView::MakeTargetPicker(QWidget* Parent, const QString& Target)
{
    auto* Picker = new QComboBox(Parent);
    Picker->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    Picker->setMinimumContentsLength(4);
    Picker->addItem(tr("Не соединено"), QString{});
    for (const auto& Candidate : Context.Nodes)
    {
        Picker->addItem(Candidate.Label, Candidate.Identity);
    }
    int Index = Picker->findData(Target);
    if (Index < 0)
    {
        Picker->addItem(tr("Нода недоступна"), Target);
        Index = Picker->count() - 1;
    }
    Picker->setCurrentIndex(Index);
    Picker->installEventFilter(this);
    return Picker;
}

void StoryInspectorView::RebuildDetails()
{
    while (auto* Item = DetailsLayout->takeAt(0))
    {
        if (Item->widget() != nullptr)
        {
            Item->widget()->hide();
            Item->widget()->deleteLater();
        }
        delete Item;
    }
    if (Current.KindId == 0 || Current.KindId == 2)
    {
        auto* Section = new QToolButton(Details);
        Section->setObjectName("InspectorSection");
        Section->setText(tr("Продолжение"));
        Section->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        Section->setIcon(MakeStudioIcon(StudioIcon::ChevronDown));
        Section->setCheckable(true);
        Section->setChecked(true);
        DetailsLayout->addWidget(Section);
        auto* Target = MakeTargetPicker(Details, Current.Targets.value(0));
        Target->setObjectName("StoryContinuation");
        DetailsLayout->addWidget(Target);
        connect(Section, &QToolButton::toggled, Target, &QWidget::setVisible);
        connect(Target, &QComboBox::currentIndexChanged, this,
                [this, Target]()
                {
                    if (bUpdating)
                    {
                        return;
                    }
                    Current.Targets = {Target->currentData().toString()};
                    Commit();
                });
    }
    if (Current.KindId == 1)
    {
        for (int Index = 0; Index < Current.Choices.size(); ++Index)
        {
            auto* Group = new QGroupBox(tr("Вариант %1").arg(Index + 1), Details);
            auto* Layout = new QVBoxLayout(Group);
            auto* Label = new QLineEdit(Current.Choices[Index].Label, Group);
            Label->setObjectName(QStringLiteral("StoryChoiceLabel%1").arg(Index));
            Label->installEventFilter(this);
            Layout->addWidget(Label);
            auto* Target = MakeTargetPicker(Group, Current.Choices[Index].Target);
            Target->setObjectName(QStringLiteral("StoryChoiceTarget%1").arg(Index));
            Layout->addWidget(Target);
            auto* Remove = new QPushButton(tr("Удалить вариант"), Group);
            Layout->addWidget(Remove);
            connect(Label, &QLineEdit::editingFinished, this,
                    [this, Label, Index]()
                    {
                        if (bUpdating || bStructurePending || Index >= Current.Choices.size())
                        {
                            return;
                        }
                        Label->setModified(false);
                        Current.Choices[Index].Label = Label->text();
                        Commit();
                    });
            connect(Target, &QComboBox::currentIndexChanged, this,
                    [this, Target, Index]()
                    {
                        if (bUpdating || bStructurePending || Index >= Current.Choices.size())
                        {
                            return;
                        }
                        Current.Choices[Index].Target = Target->currentData().toString();
                        Commit();
                    });
            connect(Remove, &QPushButton::clicked, this,
                    [this, Index]()
                    {
                        if (bStructurePending || Index >= Current.Choices.size())
                        {
                            return;
                        }
                        Current.Choices.removeAt(Index);
                        SetPendingStructure();
                        Commit();
                    });
            DetailsLayout->addWidget(Group);
        }
        auto* Add = new QPushButton(tr("+ Вариант ответа"), Details);
        Add->setObjectName("StoryAddChoice");
        DetailsLayout->addWidget(Add);
        connect(Add, &QPushButton::clicked, this,
                [this]()
                {
                    if (bStructurePending)
                    {
                        return;
                    }
                    Current.Choices.push_back({tr("Новый вариант"), {}});
                    SetPendingStructure();
                    Commit();
                });
    }
    if (Current.KindId != 2)
    {
        return;
    }
    for (int Index = 0; Index < Current.Actions.size(); ++Index)
    {
        const auto Value = Current.Actions[Index];
        auto* Group = new QGroupBox(tr("Действие %1").arg(Index + 1), Details);
        auto* Layout = new QVBoxLayout(Group);
        auto* Type = new QComboBox(Group);
        Type->addItems({tr("Ожидание"), tr("Переместить объект"), tr("Смена камеры"), tr("Движение по маршруту")});
        Type->setCurrentIndex(Value.Kind);
        Type->setObjectName(QStringLiteral("StoryActionKind%1").arg(Index));
        Layout->addWidget(Type);
        auto* Form = new QFormLayout();
        auto MakeSpin = [&](double Initial)
        {
            auto* Spin = new QDoubleSpinBox(Group);
            Spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            Spin->setRange(-1000000, 1000000);
            Spin->setDecimals(3);
            Spin->setValue(Initial);
            Spin->setProperty("storyEdited", false);
            connect(Spin, qOverload<double>(&QDoubleSpinBox::valueChanged), Spin,
                    [Spin]() { Spin->setProperty("storyEdited", true); });
            Spin->installEventFilter(this);
            Spin->findChild<QLineEdit*>()->installEventFilter(this);
            return Spin;
        };
        auto* Duration = MakeSpin(Value.Duration);
        Duration->setMinimum(0);
        Duration->setObjectName(QStringLiteral("StoryActionDuration%1").arg(Index));
        Form->addRow(Value.Kind == 3 ? tr("Тайм-аут, с") : tr("Длительность, с"), Duration);
        connect(Duration, &QDoubleSpinBox::editingFinished, this,
                [this, Duration, Index]()
                {
                    if (bUpdating || bStructurePending || Index >= Current.Actions.size() ||
                        !Duration->property("storyEdited").toBool())
                    {
                        return;
                    }
                    Duration->setProperty("storyEdited", false);
                    Current.Actions[Index].Duration = Duration->value();
                    Commit();
                });
        if (Value.Kind != 0)
        {
            if (Value.Kind == 2)
            {
                auto* Copy = new QCheckBox(tr("Положение из объекта"), Group);
                Copy->setChecked(Value.bCopyTarget);
                Layout->addWidget(Copy);
                connect(Copy, &QCheckBox::toggled, this,
                        [this, Index](bool bCopy)
                        {
                            if (bUpdating || bStructurePending)
                            {
                                return;
                            }
                            Current.Actions[Index].bCopyTarget = bCopy;
                            SetPendingStructure();
                            Commit();
                        });
            }
            if (Value.Kind != 2 || Value.bCopyTarget)
            {
                auto* Target = new QComboBox(Group);
                Target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                Target->setMinimumContentsLength(4);
                Target->addItem(tr("Выберите объект"), QString{});
                for (const auto& Object : Context.Objects)
                {
                    Target->addItem(Object.second, Object.first);
                }
                int TargetIndex = Target->findData(Value.Target);
                if (TargetIndex < 0 || !Value.LegacyTarget.isEmpty())
                {
                    Target->addItem(Value.LegacyTarget.isEmpty() ? tr("Объект недоступен") : Value.LegacyTarget,
                                    Value.Target);
                    TargetIndex = Target->count() - 1;
                }
                Target->setCurrentIndex(TargetIndex);
                Target->setObjectName(QStringLiteral("StoryActionTarget%1").arg(Index));
                Form->addRow(tr("Объект"), Target);
                connect(Target, &QComboBox::currentIndexChanged, this,
                        [this, Target, Index]()
                        {
                            if (bUpdating || bStructurePending)
                            {
                                return;
                            }
                            Current.Actions[Index].Target = Target->currentData().toString();
                            Current.Actions[Index].LegacyTarget.clear();
                            Commit();
                        });
            }
            if (Value.Kind != 2 || !Value.bCopyTarget)
            {
                const QStringList Axes = {QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
                for (int Axis = 0; Axis < 3; ++Axis)
                {
                    auto* Position = MakeSpin(Value.Position[Axis]);
                    Form->addRow(tr("Положение ") + Axes[Axis], Position);
                    connect(Position, &QDoubleSpinBox::editingFinished, this,
                            [this, Position, Index, Axis]()
                            {
                                if (bUpdating || bStructurePending || Index >= Current.Actions.size() ||
                                    !Position->property("storyEdited").toBool())
                                {
                                    return;
                                }
                                Position->setProperty("storyEdited", false);
                                Current.Actions[Index].Position[Axis] = Position->value();
                                Commit();
                            });
                }
                if (Value.Kind == 1 || Value.Kind == 2)
                {
                    auto* Rotate = new QCheckBox(tr("Задать поворот"), Group);
                    Rotate->setChecked(Value.bHasRotation);
                    Layout->addWidget(Rotate);
                    connect(Rotate, &QCheckBox::toggled, this,
                            [this, Index](bool bRotate)
                            {
                                if (bUpdating || bStructurePending)
                                {
                                    return;
                                }
                                Current.Actions[Index].bHasRotation = bRotate;
                                SetPendingStructure();
                                Commit();
                            });
                    if (Value.bHasRotation)
                    {
                        for (int Axis = 0; Axis < 3; ++Axis)
                        {
                            auto* Rotation = MakeSpin(Value.RotationDegrees[Axis]);
                            Form->addRow(tr("Поворот ") + Axes[Axis] + QString::fromUtf8(" °"), Rotation);
                            connect(Rotation, &QDoubleSpinBox::editingFinished, this,
                                    [this, Rotation, Index, Axis]()
                                    {
                                        if (bUpdating || bStructurePending || Index >= Current.Actions.size() ||
                                            !Rotation->property("storyEdited").toBool())
                                        {
                                            return;
                                        }
                                        Rotation->setProperty("storyEdited", false);
                                        Current.Actions[Index].RotationDegrees[Axis] = Rotation->value();
                                        Current.Actions[Index].bRotationEdited = true;
                                        Commit();
                                    });
                        }
                    }
                }
            }
        }
        Layout->addLayout(Form);
        auto* Remove = new QPushButton(tr("Удалить действие"), Group);
        Layout->addWidget(Remove);
        connect(Type, &QComboBox::currentIndexChanged, this,
                [this, Index](int KindValue)
                {
                    if (bUpdating || bStructurePending)
                    {
                        return;
                    }
                    Current.Actions[Index].Kind = KindValue;
                    SetPendingStructure();
                    Commit();
                });
        connect(Remove, &QPushButton::clicked, this,
                [this, Index]()
                {
                    if (bStructurePending)
                    {
                        return;
                    }
                    Current.Actions.removeAt(Index);
                    SetPendingStructure();
                    Commit();
                });
        DetailsLayout->addWidget(Group);
    }
    auto* Add = new QPushButton(tr("+ Действие"), Details);
    Add->setObjectName("StoryAddAction");
    DetailsLayout->addWidget(Add);
    connect(Add, &QPushButton::clicked, this,
            [this]()
            {
                if (bStructurePending)
                {
                    return;
                }
                Current.Actions.push_back({});
                SetPendingStructure();
                Commit();
            });
}

bool StoryInspectorView::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Event->type() == QEvent::ShortcutOverride || Event->type() == QEvent::KeyPress)
    {
        auto* Key = static_cast<QKeyEvent*>(Event);
        const auto Command = Key->matches(QKeySequence::Undo)   ? UndoRequested
                             : Key->matches(QKeySequence::Redo) ? RedoRequested
                             : Key->matches(QKeySequence::Save) ? SaveRequested
                                                                : std::function<void()>{};
        if (Command)
        {
            if (Event->type() == QEvent::KeyPress)
            {
                Command();
            }
            Key->accept();
            return true;
        }
    }
    if (Event->type() == QEvent::FocusOut && !bUpdating)
    {
        Commit();
    }
    return QWidget::eventFilter(Watched, Event);
}
} // namespace EditorUi
