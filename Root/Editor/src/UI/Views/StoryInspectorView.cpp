#include "UI/Views/StudioControls.h"
#include "Panels/ReflectionValueEditor.h"
#include "UI/Views/StoryInspectorView.h"
#include "UI/Presentation/StorySpeakerLabel.h"
#include "UI/Views/StoryEventInspectorView.h"
#include "UI/Styles/StoryEventStyle.h"
#include "UI/Views/StudioActionMenu.h"
#include "UI/Adapters/StoryMenuBuilder.h"
#include "UI/Styles/StudioIcons.h"
#include "UI/Styles/StoryInspectorStyle.h"
#include <QApplication>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFontMetrics>
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
#include <QResizeEvent>
#include <algorithm>

namespace EditorUi
{
QWidget* StoryInspectorView::MakeSection(const QString& Key, const QString& Caption, bool bOpen)
{
    auto* Section = new QWidget(Details);
    Section->setObjectName("StoryInspectorSection");
    auto* Layout = new QVBoxLayout(Section);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    auto* Toggle = new QToolButton(Section);
    Toggle->setObjectName(QStringLiteral("StorySection_%1").arg(Key));
    Toggle->setProperty("uiRole", "storySection");
    Toggle->setText(Caption);
    Toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Toggle->setCheckable(true);
    Toggle->setChecked(SectionStates.value(Key, bOpen));
    Toggle->setIcon(MakeStudioIcon(Toggle->isChecked() ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    Toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    Layout->addWidget(Toggle);
    auto* Fields = new QWidget(Section);
    Fields->setVisible(Toggle->isChecked());
    Layout->addWidget(Fields);
    connect(Toggle, &QToolButton::toggled, this, [this, Key, Toggle, Fields](bool bExpanded)
    {
        SectionStates[Key] = bExpanded;

        Toggle->setIcon(MakeStudioIcon(bExpanded ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    });
    AnimateDisclosure(*Toggle, *Fields);
    DetailsLayout->addWidget(Section);
    return Fields;
}

void StoryInspectorView::resizeEvent(QResizeEvent* Event)
{
    QWidget::resizeEvent(Event);
    // CSS grid: minmax(76px,.8fr) minmax(0,1.4fr), with an 8px gap.
    const int LabelWidth = std::max(StoryInspectorStyle::LabelMinimum,
                                   qRound((width() - 32 - 8) * StoryInspectorStyle::LabelFraction));
    KindCaption->setFixedWidth(LabelWidth);
    SpeakerCaption->setFixedWidth(LabelWidth);
    ContextTitle->setMaximumWidth(qRound((width() - 32) * .55));
    ContextTitle->setText(ContextTitle->fontMetrics().elidedText(Context.Title, Qt::ElideRight, ContextTitle->maximumWidth()));
}

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
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    auto* Heading = new QWidget(Body);
    Heading->setObjectName("StoryInspectorHeading");
    auto* HeadingLayout = new QHBoxLayout(Heading);
    HeadingLayout->setContentsMargins(StoryInspectorStyle::HeadingMargins());
    HeadingLayout->setSpacing(8);
    auto* Icon = new QLabel(Heading);
    Icon->setPixmap(MakeStudioIcon(StudioIcon::Message).pixmap(16, 16));
    Icon->setFixedSize(16, 16);
    HeadingLayout->addWidget(Icon);
    Title = new QLabel(Heading);
    Title->setObjectName("StoryInspectorTitle");
    HeadingLayout->addWidget(Title);
    HeadingLayout->addStretch();
    ContextTitle = new QLabel(Heading);
    ContextTitle->setObjectName("StoryInspectorContext");
    ContextTitle->setMinimumWidth(1);
    HeadingLayout->addWidget(ContextTitle);
    Layout->addWidget(Heading);
    auto* Primary = new QWidget(Body);
    Primary->setObjectName("StoryInspectorPrimary");
    auto* PrimaryLayout = new QVBoxLayout(Primary);
    PrimaryLayout->setContentsMargins(StoryInspectorStyle::PrimaryMargins());
    PrimaryLayout->setSpacing(StoryInspectorStyle::FieldGap);
    auto* Form = new QFormLayout();
    Form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    Form->setContentsMargins(0, 0, 0, 0);
    Form->setHorizontalSpacing(8);
    Form->setVerticalSpacing(10);
    Kind = new EditorUi::StudioSelect(Body);
    Kind->setObjectName("StoryNodeKind");
    Kind->addItems({tr("Реплика"), tr("Выбор"), tr("Действие"), tr("Концовка"), tr("Narrative")});
    Speaker = new EditorUi::StudioSelect(Body);
    Speaker->setObjectName("StorySpeaker");
    Speaker->setInsertPolicy(QComboBox::NoInsert);
    Form->addRow(tr("Тип блока"), Kind);
    KindCaption = Form->labelForField(Kind);
    Form->addRow(tr("Говорит"), Speaker);
    SpeakerCaption = Form->labelForField(Speaker);
    PrimaryLayout->addLayout(Form);
    Text = new QTextEdit(Primary);
    Text->setObjectName("StoryText");
    Text->setAcceptRichText(false);
    Text->setUndoRedoEnabled(false);
    Text->setFixedHeight(StoryInspectorStyle::DialogueHeight);
    StoryInspectorStyle::ApplyDialogueTypography(*Text);
    PrimaryLayout->addWidget(Text);
    Layout->addWidget(Primary);
    Details = new QWidget(Body);
    DetailsLayout = new QVBoxLayout(Details);
    DetailsLayout->setContentsMargins(0, 0, 0, 0);
    DetailsLayout->setSpacing(0);
    Layout->addWidget(Details);
    // Native-only entry selection stays available without displacing the JS form.
    Options = new QWidget(Body);
    auto* OptionsLayout = new QVBoxLayout(Options);
    OptionsLayout->setContentsMargins(0, 0, 0, 0);
    OptionsLayout->setSpacing(0);
    auto* Toggle = new QToolButton(Options);
    Toggle->setObjectName("StoryOptionsToggle");
    Toggle->setProperty("uiRole", "storySection");
    Toggle->setText(tr("Параметры истории"));
    Toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Toggle->setIcon(MakeStudioIcon(StudioIcon::ChevronRight));
    Toggle->setCheckable(true);
    Toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    OptionsLayout->addWidget(Toggle);
    auto* OptionFields = new QWidget(Options);
    auto* OptionLayout = new QVBoxLayout(OptionFields);
    OptionLayout->setContentsMargins(StoryInspectorStyle::SectionMargins());
    Start = new EditorUi::StudioCheckBox(tr("Начало истории"), OptionFields);
    Start->setObjectName("StoryStart");
    OptionLayout->addWidget(Start);
    OptionFields->hide();
    OptionsLayout->addWidget(OptionFields);
    AnimateDisclosure(*Toggle, *OptionFields);
    connect(Toggle, &QToolButton::toggled, this, [Toggle](bool bOpen)
    {
        Toggle->setIcon(MakeStudioIcon(bOpen ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    });
    Layout->addWidget(Options);
    Layout->addStretch(1);
    Scroll->setWidget(Body);
    Outer->addWidget(Scroll);
    NodeScroll = Scroll;
    EventScroll = new QScrollArea(this);
    EventScroll->setWidgetResizable(true);
    EventScroll->setFrameShape(QFrame::NoFrame);
    EventView = new StoryEventInspectorView(EventScroll);
    EventScroll->setWidget(EventView);
    Outer->addWidget(EventScroll);
    EventScroll->hide();
    TextTimer = new QTimer(this);
    TextTimer->setSingleShot(true);
    TextTimer->setInterval(400);
    connect(TextTimer, &QTimer::timeout, this, &StoryInspectorView::Commit);
    Text->installEventFilter(this);
    Kind->installEventFilter(this);
    Speaker->installEventFilter(this);
    connect(Text, &QTextEdit::textChanged, this,
            [this]()
            {
                if (!bUpdating)
                {
                    TextTimer->start();
                }
            });
    connect(Speaker, &QComboBox::textActivated, this, [this]() { Commit(); });
    connect(Kind, &QComboBox::currentIndexChanged, this,
            [this](int Value)
            {
                if (bUpdating)
                {
                    return;
                }
                if (Value == 4 && Current.NarrativeType.empty())
                {
                    const QSignalBlocker Blocker(Kind);
                    Kind->setCurrentIndex(Current.KindId);
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
    NodeScroll->show(); EventScroll->hide();
    QWidget* Focus = QApplication::focusWidget();
    // QWidget::isAncestorOf stops at a window boundary. StudioSelect's search
    // field lives in a Qt::Popup, but still belongs to this inspector. Rebuilding
    // its model on the focus-out commit would immediately close the open menu.
    bool bOwnsFocus = false;
    for (QWidget* Owner = Focus; Owner != nullptr; Owner = Owner->parentWidget())
    {
        if (Owner == this) { bOwnsFocus = true; break; }
    }
    const bool bSameDocument = Context.DocumentIdentity == Model.DocumentIdentity;
    Context = Model;
    if (!bForce && !bStructurePending && Current.Identity == Node.Identity && Current.KindId == Node.KindId &&
        bSameDocument && bOwnsFocus && isEnabled() == !Model.bReadOnly)
    {
        return;
    }
    const bool bSameText = Text->toPlainText() == Node.Text;
    const auto Cursor = Text->textCursor();
    bUpdating = true;
    TextTimer->stop();
    Current = Node;
    Current.bStart = Node.Identity == Model.Start;
    Title->setText(Node.Kind);
    ContextTitle->setText(ContextTitle->fontMetrics().elidedText(Model.Title, Qt::ElideRight, ContextTitle->maximumWidth()));
    ContextTitle->setToolTip(Model.Title);
    Kind->setCurrentIndex(Node.KindId);
    Kind->show();
    Kind->setEnabled(Node.KindId != 4);
    KindCaption->show();
    Options->show();
    Start->show();
    Details->show();
    Speaker->clear();
    const QString Narrator = tr("Рассказчик");
    // Keep the authored spelling until the user actually chooses another speaker.
    Speaker->addItem(Narrator, IsNarrator(Node.Speaker) ? Node.Speaker : QString{});
    for (const auto& Character : Model.Characters)
    {
        Speaker->addItem(Character.second, Character.first);
    }
    for (const auto& Candidate : Model.Nodes)
    {
        if (!IsNarrator(Candidate.Speaker) && Speaker->findData(Candidate.Speaker) < 0)
        {
            Speaker->addItem(Candidate.Speaker, Candidate.Speaker);
        }
    }
    Speaker->setCurrentIndex(std::max(0, Speaker->findData(Node.Speaker)));
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
    Start->setEnabled(!Current.bStart && !Current.NarrativeDefinition.bPure);
    bStructurePending = false;
    RebuildDetails();
    Details->setEnabled(true);
    setEnabled(Model.bOpen && !Model.bReadOnly);
    bUpdating = false;
}

void StoryInspectorView::Commit()
{
    if (!EventScroll->isHidden()) { EventView->Commit(); return; }
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
        bool bNarrativeField = false;
        for (QWidget* Parent = Field->parentWidget(); Parent != nullptr && Parent != Details; Parent = Parent->parentWidget())
        {
            if (Parent->objectName().startsWith(QStringLiteral("NarrativeInput_")))
            {
                bNarrativeField = true;
                break;
            }
        }
        if ((Field->objectName().startsWith(QStringLiteral("StoryChoiceLabel")) || bNarrativeField) && Field->isModified())
        {
            Field->setModified(false);
            QMetaObject::invokeMethod(Field, "editingFinished", Qt::DirectConnection);
        }
    }
    bCollecting = false;
    Current.Text = Text->toPlainText();
    Current.Speaker = Speaker->currentData().toString();
    Current.Kind = Kind->currentText();
    if (Current.KindId == 1 || Current.KindId == 4)
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
    EventScroll->hide(); NodeScroll->show();
    bUpdating = true;
    TextTimer->stop();
    Current = {};
    Title->setText(tr("Выберите ноду истории"));
    ContextTitle->clear();
    Options->hide();
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
    auto* Picker = new EditorUi::StudioSelect(Parent);
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

void StoryInspectorView::SetEvent(const StoryGraphNode& Node, const StoryGraphEventBatch& Batch,
    const StoryGraphEventBinding& Binding, const StoryGraphEvent& Event, const StoryGraphModel& Model, bool bForce)
{
    TextTimer->stop();
    NodeScroll->hide(); EventScroll->show();
    setEnabled(Model.bOpen && !Model.bReadOnly);
    EventView->SetEvent(Node, Batch, Binding, Event, Model, bForce);
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
    if (Current.KindId == 4)
    {
        auto* Section = MakeSection("narrative", QString::fromStdString(Current.NarrativeType));
        auto* Fields = new QFormLayout(Section);
        Fields->setContentsMargins(StoryInspectorStyle::SectionMargins());
        Fields->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        if (Current.NarrativeDefinition.Type.empty())
        {
            auto* Message = new QLabel(tr("Тип ноды не зарегистрирован. Проверьте Python-файл в Scripts проекта."), Section);
            Message->setWordWrap(true);
            Fields->addRow(Message);
            return;
        }
        for (const auto& Pin : Current.NarrativeDefinition.Inputs)
        {
            const auto Name = Pin.Property.Id.Value;
            auto* Container = new QWidget(Section);
            auto* Layout = new QVBoxLayout(Container);
            Layout->setContentsMargins(0, 0, 0, 0);
            auto* Source = new StudioSelect(Container);
            Source->setObjectName(QString::fromStdString("NarrativeSource_" + Name));
            Source->addItem(tr("Значение"), QStringList{});
            for (const auto& Producer : Context.Nodes)
            {
                if (Producer.Identity == Current.Identity)
                {
                    continue;
                }
                for (const auto& Output : Producer.NarrativeDefinition.Outputs)
                {
                    if (Output.Property.ValueTypeId == Pin.Property.ValueTypeId)
                    {
                        Source->addItem(Producer.Label + " / " + QString::fromStdString(Output.Property.Id.Value),
                            QStringList{Producer.Identity, QString::fromStdString(Output.Property.Id.Value)});
                    }
                }
            }
            const auto Existing = Current.NarrativeSources.find(Name);
            if (Existing != Current.NarrativeSources.end())
            {
                const QStringList Link{QString::fromStdString(Existing->second.first), QString::fromStdString(Existing->second.second)};
                int Index = Source->findData(Link);
                if (Index < 0)
                {
                    Source->addItem(tr("Источник недоступен"), Link);
                    Index = Source->count() - 1;
                }
                Source->setCurrentIndex(Index);
            }
            Layout->addWidget(Source);
            connect(Source, &QComboBox::currentIndexChanged, this, [this, Source, Name]()
            {
                const auto Link = Source->currentData().toStringList();
                Current.NarrativeSources.erase(Name);
                if (Link.size() == 2)
                {
                    Current.NarrativeSources[Name] = {Link[0].toStdString(), Link[1].toStdString()};
                }
                Commit();
                QTimer::singleShot(0, this, [this]() { RebuildDetails(); });
            });
            nlohmann::json Literal = Pin.Default;
            if (Current.NarrativeInputs.contains(Name))
            {
                Literal = Current.NarrativeInputs[Name];
            }
            QWidget* Editor = nullptr;
            if (Pin.Property.ValueTypeId.Value == "engine.ObjectHandle")
            {
                auto* Picker = new StudioSelect(Container);
                Picker->addItem(tr("Не задано"), QString{});
                for (const auto& Object : Context.NarrativeObjects)
                {
                    Picker->addItem(Object.second, Object.first);
                }
                std::string Identity;
                if (Literal.is_object())
                {
                    Identity = Literal.value("object", std::string{});
                }
                int Index = Picker->findData(QString::fromStdString(Identity));
                if (Index < 0)
                {
                    Picker->addItem(tr("Объект недоступен"), QString::fromStdString(Identity));
                    Index = Picker->count() - 1;
                }
                Picker->setCurrentIndex(Index);
                connect(Picker, &QComboBox::currentIndexChanged, this, [this, Picker, Name]()
                {
                    Current.NarrativeInputs[Name] = {{"object", Picker->currentData().toString().toStdString()}};
                    Commit();
                });
                Editor = Picker;
            }
            else
            {
                ReflectedValue Value;
                MakeDefaultReflectionValue(Pin.Property.ValueTypeId, Value);
                if (!Literal.is_null())
                {
                    ReflectionJson::JsonToReflectedValue(Pin.Property.ValueTypeId, Literal, Value);
                }
                Editor = new ReflectionValueEditor(Pin.Property.ValueTypeId, Value, Pin.Property.Attributes,
                    true, nullptr, nullptr, [this, Name, Type = Pin.Property.ValueTypeId](const ReflectedValue& Updated)
                {
                    nlohmann::json Encoded;
                    if (!ReflectionJson::ReflectedValueToJson(Type, Updated, Encoded).bOk)
                    {
                        return false;
                    }
                    Current.NarrativeInputs[Name] = std::move(Encoded);
                    Commit();
                    return true;
                }, Container);
            }
            Editor->setEnabled(Existing == Current.NarrativeSources.end());
            Editor->setObjectName(QString::fromStdString("NarrativeInput_" + Name));
            Layout->addWidget(Editor);
            QString Label = QString::fromStdString(Name);
            if (Pin.Property.Attributes.DisplayName != nullptr && Pin.Property.Attributes.DisplayName[0] != '\0')
            {
                Label = QString::fromUtf8(Pin.Property.Attributes.DisplayName);
            }
            Fields->addRow(Label, Container);
        }
        for (int Index = 0; Index < Current.Choices.size(); ++Index)
        {
            auto* Target = MakeTargetPicker(Section, Current.Choices[Index].Target);
            Fields->addRow(Current.Choices[Index].Label, Target);
            connect(Target, &QComboBox::currentIndexChanged, this, [this, Target, Index]()
            {
                Current.Choices[Index].Target = Target->currentData().toString();
                Commit();
            });
        }
        return;
    }
    if (Current.KindId == 0 || Current.KindId == 1)
    {
        auto* Staging = MakeSection("staging", tr("Постановка реплики"), false);
        auto* StagingLayout = new QVBoxLayout(Staging);
        StagingLayout->setContentsMargins(StoryInspectorStyle::SectionMargins());
        StagingLayout->setSpacing(8);
        for (int Phase = 0; Phase < 3; ++Phase)
        {
            int Count = 0;
            for (const auto& Batch : Current.EventBatches) { if (Batch.Phase == Phase) { Count += Batch.Bindings.size(); } }
            auto* Add = new QPushButton(StoryEventStyle::Phases()[Phase] + QStringLiteral(" · %1   +").arg(Count), Staging);
            Add->setObjectName(QStringLiteral("StoryStagePhase%1").arg(Phase));
            StagingLayout->addWidget(Add);
            connect(Add, &QPushButton::clicked, this, [this, Add, Phase]()
            {
                Commit();
                const QString Identity = Current.Identity;
                const QString Document = Context.DocumentIdentity;
                auto Items = BuildStoryEventMenu(Context.Events,
                    [this, Identity, Document, Phase](QString Source)
                    {
                        if (Context.DocumentIdentity == Document && !Context.bReadOnly && !Context.bLocked && AddEventRequested)
                        { AddEventRequested(Identity, Phase, Source); }
                    });
                auto* Menu = new StudioActionMenu(tr("Добавить событие"), std::move(Items), Add);
                Menu->Open(Add->mapToGlobal(QPoint(0, Add->height())));
            });
            for (const auto& Batch : Current.EventBatches) { if (Batch.Phase == Phase) { for (const auto& Binding : Batch.Bindings)
            {
                auto* Select = new QPushButton(Binding.Name, Staging); Select->setProperty("uiRole", "eventPlacement"); StagingLayout->addWidget(Select);
                connect(Select, &QPushButton::clicked, this, [this, Id = Binding.Identity]() { Commit(); if (EventSelected) { EventSelected(Current.Identity, Id); } });
            } } }
        }
        auto* Import = new QPushButton(tr("Импорт события…"), Staging);
        StagingLayout->addWidget(Import);
        connect(Import, &QPushButton::clicked, this, [this]() { Commit(); if (ImportEventRequested) { ImportEventRequested(); } });
    }
    if (Current.KindId == 0 || Current.KindId == 2)
    {
        auto* Section = MakeSection("continuation", tr("Продолжение"));
        auto* Fields = new QVBoxLayout(Section);
        Fields->setContentsMargins(StoryInspectorStyle::SectionMargins());
        auto* Target = MakeTargetPicker(Section, Current.Targets.value(0));
        Target->setObjectName("StoryContinuation");
        Fields->addWidget(Target);
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
        auto* Section = MakeSection("choices", tr("Ответы игрока"));
        auto* ChoicesLayout = new QVBoxLayout(Section);
        ChoicesLayout->setContentsMargins(StoryInspectorStyle::SectionMargins());
        ChoicesLayout->setSpacing(StoryInspectorStyle::FieldGap);
        for (int Index = 0; Index < Current.Choices.size(); ++Index)
        {
            auto* Group = new QGroupBox(tr("Ответ %1").arg(Index + 1), Section);
            auto* Layout = new QVBoxLayout(Group);
            Layout->setContentsMargins(10, 20, 10, 10);
            Layout->setSpacing(10);
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
            ChoicesLayout->addWidget(Group);
        }
        auto* Add = new QPushButton(tr("+ Вариант ответа"), Details);
        Add->setObjectName("StoryAddChoice");
        ChoicesLayout->addWidget(Add);
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
    auto* ActionsSection = MakeSection("actions", tr("Действия"));
    auto* ActionsLayout = new QVBoxLayout(ActionsSection);
    ActionsLayout->setContentsMargins(StoryInspectorStyle::SectionMargins());
    ActionsLayout->setSpacing(StoryInspectorStyle::FieldGap);
    for (int Index = 0; Index < Current.Actions.size(); ++Index)
    {
        const auto Value = Current.Actions[Index];
        auto* Group = new QGroupBox(tr("Действие %1").arg(Index + 1), ActionsSection);
        auto* Layout = new QVBoxLayout(Group);
        Layout->setContentsMargins(10, 20, 10, 10);
        Layout->setSpacing(10);
        auto* Type = new EditorUi::StudioSelect(Group);
        Type->addItems({tr("Ожидание"), tr("Переместить объект"), tr("Смена камеры"), tr("Движение по маршруту"), tr("Анимация персонажа")});
        Type->setCurrentIndex(Value.Kind);
        Type->setObjectName(QStringLiteral("StoryActionKind%1").arg(Index));
        Layout->addWidget(Type);
        auto* Form = new QFormLayout();
        Form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        Form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        Form->setHorizontalSpacing(8);
        Form->setVerticalSpacing(10);
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
                auto* Copy = new EditorUi::StudioCheckBox(tr("Положение из объекта"), Group);
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
                auto* Target = new EditorUi::StudioSelect(Group);
                Target->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                Target->setMinimumContentsLength(4);
                Target->addItem(tr("Выберите объект"), QString{});
                for (const auto& Object : Context.NarrativeObjects)
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
                            SetPendingStructure();
                            Commit();
                        });
            }
            if (Value.Kind == 4)
            {
                auto* Animation = new EditorUi::StudioSelect(Group);
                Animation->addItem(tr("Выберите анимацию"), QString{});
                for (const auto& Choice : Context.CharacterAnimations.value(Value.Target))
                {
                    Animation->addItem(Choice.second, Choice.first);
                }
                if (!Value.AnimationId.isEmpty() && Animation->findData(Value.AnimationId) < 0)
                {
                    Animation->addItem(tr("Недоступная анимация: ") + Value.AnimationId, Value.AnimationId);
                }
                Animation->setCurrentIndex(std::max(0, Animation->findData(Value.AnimationId)));
                Form->addRow(tr("Анимация"), Animation);
                connect(Animation, &QComboBox::currentIndexChanged, this, [this, Animation, Index]()
                {
                    if (bUpdating || bStructurePending)
                    {
                        return;
                    }
                    Current.Actions[Index].AnimationId = Animation->currentData().toString();
                    Commit();
                });
            }
            if (Value.Kind != 4 && (Value.Kind != 2 || !Value.bCopyTarget))
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
                    auto* Rotate = new EditorUi::StudioCheckBox(tr("Задать поворот"), Group);
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
        ActionsLayout->addWidget(Group);
    }
    auto* Add = new QPushButton(tr("+ Действие"), Details);
    Add->setObjectName("StoryAddAction");
    ActionsLayout->addWidget(Add);
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
