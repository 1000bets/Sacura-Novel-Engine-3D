#include "UI/Presentation/LocalizedEditorUi.h"
#include "UI/Views/StudioControls.h"
#include "UI/Views/StoryEventInspectorView.h"
#include "UI/Views/StoryActionFieldsView.h"
#include "UI/Styles/StoryEventStyle.h"
#include "UI/Styles/StudioIcons.h"
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace EditorUi
{
StoryEventInspectorView::StoryEventInspectorView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("StoryEventInspector");
    Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
}
void StoryEventInspectorView::SetEvent(const StoryGraphNode& InNode, const StoryGraphEventBatch& InBatch,
                                       const StoryGraphEventBinding& InBinding, const StoryGraphEvent& InEvent,
                                       const StoryGraphModel& Model, bool bForce)
{
    auto* Focus = QApplication::focusWidget();
    if (!bForce && !bPending && Binding.Identity == InBinding.Identity && Focus && isAncestorOf(Focus) &&
        isEnabled() == !Model.bReadOnly)
    {
        Context = Model;
        return;
    }
    const bool Same = Binding.Identity == InBinding.Identity;
    Node = InNode;
    Batch = InBatch;
    Binding = InBinding;
    Source = InEvent;
    Effective = Source;
    Context = Model;
    for (auto& Group : Effective.Groups)
    {
        for (auto& Action : Group.Actions)
        {
            for (const auto& Override : Binding.Overrides)
            {
                if (Override.Identity == Action.Identity)
                {
                    Action = Override;
                    break;
                }
            }
        }
    }
    if (!Same)
    {
        SelectedAction.clear();
        bEditingSource = false;
    }
    bool Found = false;
    for (const auto& Group : Effective.Groups)
    {
        for (const auto& Action : Group.Actions)
        {
            Found |= Action.Identity == SelectedAction;
        }
    }
    if (!Found)
    {
        SelectedAction.clear();
    }
    bPending = false;
    Rebuild();
    setEnabled(Model.bOpen && !Model.bReadOnly);
}
QWidget* StoryEventInspectorView::Section(const QString& Key, const QString& Caption, bool bOpen)
{
    auto* Container = new QWidget(this);
    auto* Stack = new QVBoxLayout(Container);
    Stack->setContentsMargins(0, 0, 0, 0);
    Stack->setSpacing(0);
    auto* Toggle = new QToolButton(Container);
    Toggle->setProperty("uiRole", "storySection");
    Toggle->setText(Caption);
    Toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Toggle->setCheckable(true);
    Toggle->setChecked(Sections.value(Key, bOpen));
    Toggle->setIcon(MakeStudioIcon(Toggle->isChecked() ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    Toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* Fields = new QWidget(Container);
    Fields->setVisible(Toggle->isChecked());
    Stack->addWidget(Toggle);
    Stack->addWidget(Fields);
    connect(Toggle, &QToolButton::toggled, this, [this, Key, Toggle, Fields](bool Checked) {
        Sections[Key] = Checked;

        Toggle->setIcon(MakeStudioIcon(Checked ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    });
    AnimateDisclosure(*Toggle, *Fields);
    Layout->addWidget(Container);
    return Fields;
}
void StoryEventInspectorView::RebuildLater()
{
    bPending = true;
    QTimer::singleShot(0, this, [this]() {
        if (bPending)
        {
            bPending = false;
            Rebuild();
        }
    });
}
void StoryEventInspectorView::PublishStructure()
{
    bPending = true;
    if (EventChanged)
    {
        EventChanged(Source);
    }
}
void StoryEventInspectorView::Rebuild()
{
    bUpdating = true;
    Effective = Source;
    if (!bEditingSource)
    {
        for (auto& Group : Effective.Groups)
        {
            for (auto& Action : Group.Actions)
            {
                for (const auto& Override : Binding.Overrides)
                {
                    if (Override.Identity == Action.Identity)
                    {
                        Action = Override;
                        break;
                    }
                }
            }
        }
    }
    Name = nullptr;
    Description = nullptr;
    ActionFields = nullptr;
    while (auto* Item = Layout->takeAt(0))
    {
        if (auto* Widget = Item->widget())
        {
            Widget->hide();
            Widget->deleteLater();
        }
        delete Item;
    }
    auto* Heading = new QWidget(this);
    Heading->setObjectName("StoryInspectorHeading");
    auto* HeadingRow = new QHBoxLayout(Heading);
    HeadingRow->setContentsMargins(StoryEventStyle::HeadingMargins());
    HeadingRow->setSpacing(StoryEventStyle::HeadingGap);
    auto* Glyph = new QLabel(Heading);
    Glyph->setPixmap(MakeStudioIcon(StudioIcon::Layers).pixmap(16, 16));
    HeadingRow->addWidget(Glyph);
    auto* Title = new QLabel(SelectedAction.isEmpty() ? tr("Событие") : tr("Действие"), Heading);
    Title->setObjectName("StoryInspectorTitle");
    HeadingRow->addWidget(Title);
    HeadingRow->addStretch();
    auto* ContextLabel = new QLabel(bEditingSource ? tr("Общее событие") : tr("В этой реплике"), Heading);
    ContextLabel->setObjectName("StoryInspectorContext");
    ContextLabel->setMinimumWidth(1);
    ContextLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    HeadingRow->addWidget(ContextLabel, 1);
    Layout->addWidget(Heading);
    if (!SelectedAction.isEmpty())
    {
        auto* Back = new QPushButton(QStringLiteral("‹ ") + Source.Name, this);
        Back->setObjectName("StoryEventBack");
        Layout->addWidget(Back);
        connect(Back, &QPushButton::clicked, this, [this]() {
            Commit();
            SelectedAction.clear();
            RebuildLater();
        });
    }
    auto* ContextRow = new QWidget(this);
    ContextRow->setObjectName("StoryEventContext");
    auto* ContextLayout = new QHBoxLayout(ContextRow);
    ContextLayout->setContentsMargins(StoryEventStyle::ContextMargins());
    ContextLayout->setSpacing(6);
    auto* Preview = new QLabel(bEditingSource ? tr("Общее событие") : Node.Text.simplified(), ContextRow);
    Preview->setToolTip(Preview->text());
    Preview->setMinimumWidth(1);
    Preview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    ContextLayout->addWidget(Preview, 1);
    auto* SourceButton = new QToolButton(ContextRow);
    SourceButton->setIcon(MakeStudioIcon(bEditingSource ? StudioIcon::ChevronRight : StudioIcon::ExternalLink));
    SourceButton->setToolTip(bEditingSource ? tr("Вернуться к размещению") : tr("Открыть исходный шаблон"));
    SourceButton->setObjectName("StoryEventSource");
    ContextLayout->addWidget(SourceButton);
    Layout->addWidget(ContextRow);
    connect(SourceButton, &QToolButton::clicked, this, [this]() {
        Commit();
        bEditingSource = !bEditingSource;
        RebuildLater();
    });
    auto Placement = [&](QFormLayout* PrimaryForm = nullptr) {
        QWidget* Fields =
            PrimaryForm ? PrimaryForm->parentWidget() : Section("placement", tr("Размещение события"), false);
        auto* Form = PrimaryForm ? PrimaryForm : new QFormLayout(Fields);
        if (!PrimaryForm)
        {
            Form->setContentsMargins(StoryEventStyle::FieldsMargins());
        }
        Form->setSpacing(StoryEventStyle::FieldGap);
        Form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        Form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        auto* Phase = new EditorUi::StudioSelect(Fields);
        Phase->setObjectName("StoryEventPhase");
        Phase->addItems(StoryEventStyle::Phases());
        Phase->setCurrentIndex(Batch.Phase);
        auto* Join = new EditorUi::StudioSelect(Fields);
        Join->setObjectName("StoryEventJoin");
        Join->addItems(StoryEventStyle::Joins());
        Join->setCurrentIndex(Binding.Join);
        for (auto* Picker : {Phase, Join})
        {
            Picker->setMinimumContentsLength(4);
            Picker->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        }
        Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Момент запуска")), Phase);
        Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Продолжение")), Join);
        connect(Phase, &QComboBox::currentIndexChanged, this, [this, Phase, Join]() {
            Commit();
            bPending = true;
            if (PlacementChanged)
            {
                PlacementChanged(Phase->currentIndex(), Join->currentIndex());
            }
        });
        connect(Join, &QComboBox::currentIndexChanged, this, [this, Phase, Join]() {
            Commit();
            bPending = true;
            if (PlacementChanged)
            {
                PlacementChanged(Phase->currentIndex(), Join->currentIndex());
            }
        });
        auto* Reset = BindEditorCaption(new QPushButton(tr("Сбросить изменения здесь"), Fields), QString::fromUtf8("Сбросить изменения здесь"));
        Reset->setObjectName("StoryEventReset");
        Reset->setEnabled(!Binding.Overrides.isEmpty());
        Form->addRow(Reset);
        connect(Reset, &QPushButton::clicked, this, [this]() {
            bPending = true;
            if (ResetRequested)
            {
                ResetRequested();
            }
        });
    };
    if (!SelectedAction.isEmpty())
    {
        for (const auto& Group : Effective.Groups)
        {
            for (const auto& Action : Group.Actions)
            {
                if (Action.Identity != SelectedAction)
                {
                    continue;
                }
                auto* SectionBody = Section("action", tr("Параметры действия"));
                auto* FieldsLayout = new QVBoxLayout(SectionBody);
                FieldsLayout->setContentsMargins(0, 0, 0, 0);
                ActionFields = new StoryActionFieldsView(Action, Context, SectionBody);
                FieldsLayout->addWidget(ActionFields);
                ActionFields->Changed = [this](StoryGraphAction Value) {
                    if (bUpdating)
                    {
                        return;
                    }
                    for (auto& ItemGroup : Effective.Groups)
                    {
                        for (auto& Item : ItemGroup.Actions)
                        {
                            if (Item.Identity == Value.Identity)
                            {
                                bPending |= Item.Kind != Value.Kind || Item.bCopyTarget != Value.bCopyTarget ||
                                            Item.bHasRotation != Value.bHasRotation;
                                Item = Value;
                            }
                        }
                    }
                    if (bEditingSource)
                    {
                        for (auto& Group : Source.Groups)
                        {
                            for (auto& Item : Group.Actions)
                            {
                                if (Item.Identity == Value.Identity)
                                {
                                    Item = Value;
                                }
                            }
                        }
                        if (EventChanged)
                        {
                            EventChanged(Source);
                        }
                    }
                    else if (ActionChanged)
                    {
                        ActionChanged(Value);
                    }
                };
            }
        }
        if (!bEditingSource)
        {
            Placement();
        }
    }
    else
    {
        auto* Primary = new QWidget(this);
        auto* Form = new QFormLayout(Primary);
        Form->setContentsMargins(StoryEventStyle::FieldsMargins());
        Form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        Name = new QLineEdit(Source.Name, Primary);
        Name->setObjectName("StoryEventName");
        Form->addRow(BindEditorCaption(new QLabel(), QString::fromUtf8("Название")), Name);
        connect(Name, &QLineEdit::editingFinished, this, &StoryEventInspectorView::Commit);
        if (!bEditingSource)
        {
            Placement(Form);
        }
        Layout->addWidget(Primary);
        auto* Actions = Section("actions", tr("Действия"));
        auto* GroupsLayout = new QVBoxLayout(Actions);
        GroupsLayout->setContentsMargins(StoryEventStyle::GroupsMargins());
        GroupsLayout->setSpacing(StoryEventStyle::FieldGap);
        for (int GroupIndex = 0; GroupIndex < Source.Groups.size(); ++GroupIndex)
        {
            const auto& Group = Source.Groups[GroupIndex];
            auto* Card = new QWidget(Actions);
            Card->setObjectName("StoryEventGroup");
            auto* Stack = new QVBoxLayout(Card);
            Stack->setContentsMargins(0, 0, 0, 0);
            Stack->setSpacing(0);
            auto* GroupHeading = new QWidget(Card);
            GroupHeading->setObjectName("StoryEventGroupHeading");
            auto* Row = new QHBoxLayout(GroupHeading);
            Row->setContentsMargins(StoryEventStyle::GroupHeadingMargins());
            Row->setSpacing(StoryEventStyle::GroupControlGap);
            auto* GroupName = new QLineEdit(Group.Name, Card);
            GroupName->setMinimumWidth(0);
            GroupName->setObjectName("StoryEventGroupName");
            GroupName->setProperty("groupIdentity", Group.Identity);
            Row->addWidget(GroupName);
            connect(GroupName, &QLineEdit::editingFinished, this, [this, GroupName, GroupIndex]() {
                if (!bUpdating && !bPending && GroupName->isModified())
                {
                    Source.Groups[GroupIndex].Name = GroupName->text();
                    GroupName->setModified(false);
                    if (EventChanged)
                    {
                        EventChanged(Source);
                    }
                }
            });
            auto Button = [&](const QString& Text, std::function<void()> Callback) {
                auto* Control = new QToolButton(Card);
                Control->setText(Text);
                Row->addWidget(Control);
                connect(Control, &QToolButton::clicked, this, [this, Callback]() {
                    if (!bPending)
                    {
                        Commit();
                        Callback();
                        PublishStructure();
                    }
                });
                return Control;
            };
            auto* Up = Button(QString::fromUtf8("↑"),
                              [this, GroupIndex]() { Source.Groups.swapItemsAt(GroupIndex, GroupIndex - 1); });
            Up->setEnabled(GroupIndex > 0);
            auto* Down = Button(QString::fromUtf8("↓"),
                                [this, GroupIndex]() { Source.Groups.swapItemsAt(GroupIndex, GroupIndex + 1); });
            Down->setEnabled(GroupIndex + 1 < Source.Groups.size());
            Button(QString::fromUtf8("×"), [this, GroupIndex]() { Source.Groups.removeAt(GroupIndex); });
            Stack->addWidget(GroupHeading);
            for (int ActionIndex = 0; ActionIndex < Group.Actions.size(); ++ActionIndex)
            {
                const auto& Action = Effective.Groups[GroupIndex].Actions[ActionIndex];
                auto* ActionRow = new QHBoxLayout();
                auto* Select = new QPushButton(StoryEventStyle::Summary(Action), Card);
                Select->setObjectName("StoryEventAction");
                Select->setMinimumWidth(0);
                Select->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
                Select->setToolTip(Select->text());
                Select->setProperty("actionIdentity", Action.Identity);
                ActionRow->addWidget(Select, 1);
                connect(Select, &QPushButton::clicked, this, [this, Identity = Action.Identity]() {
                    Commit();
                    SelectedAction = Identity;
                    RebuildLater();
                });
                auto* Delete = new QToolButton(Card);
                Delete->setText(QString::fromUtf8("×"));
                ActionRow->addWidget(Delete);
                connect(Delete, &QToolButton::clicked, this, [this, GroupIndex, ActionIndex]() {
                    if (bPending)
                    {
                        return;
                    }
                    Commit();
                    Source.Groups[GroupIndex].Actions.removeAt(ActionIndex);
                    PublishStructure();
                });
                Stack->addLayout(ActionRow);
            }
            auto* Add = BindEditorCaption(new QPushButton(tr("+ Добавить действие"), Card), QString::fromUtf8("+ Добавить действие"));
            Add->setObjectName("StoryEventAddAction");
            Stack->addWidget(Add);
            connect(Add, &QPushButton::clicked, this, [this, GroupIndex]() {
                if (bPending)
                {
                    return;
                }
                Commit();
                StoryGraphAction Action;
                Action.Duration = 1;
                Source.Groups[GroupIndex].Actions.push_back(Action);
                PublishStructure();
            });
            GroupsLayout->addWidget(Card);
        }
        auto* AddGroup = BindEditorCaption(new QPushButton(tr("+ Следующий шаг"), Actions), QString::fromUtf8("+ Следующий шаг"));
        AddGroup->setObjectName("StoryEventAddGroup");
        GroupsLayout->addWidget(AddGroup);
        connect(AddGroup, &QPushButton::clicked, this, [this]() {
            if (bPending)
            {
                return;
            }
            Commit();
            StoryGraphAction Action;
            Action.Duration = 1;
            Source.Groups.push_back({{}, tr("Следующий шаг"), {Action}});
            PublishStructure();
        });
        auto* DescriptionBody = Section("description", tr("Описание"), false);
        auto* DescriptionLayout = new QVBoxLayout(DescriptionBody);
        DescriptionLayout->setContentsMargins(StoryEventStyle::GroupsMargins());
        Description = new QTextEdit(DescriptionBody);
        Description->setObjectName("StoryEventDescription");
        Description->setAcceptRichText(false);
        Description->setUndoRedoEnabled(false);
        Description->setPlainText(Source.Description);
        Description->setFixedHeight(StoryEventStyle::DescriptionHeight);
        DescriptionLayout->addWidget(Description);
        auto* Footer = new QWidget(this);
        auto* FooterLayout = new QVBoxLayout(Footer);
        FooterLayout->setContentsMargins(StoryEventStyle::FieldsMargins());
        FooterLayout->setSpacing(StoryEventStyle::HeadingGap);
        auto* Export = BindEditorCaption(new QPushButton(tr("Экспорт события…"), Footer), QString::fromUtf8("Экспорт события…"));
        FooterLayout->addWidget(Export);
        Export->setObjectName("StoryEventExport");
        auto* Import = BindEditorCaption(new QPushButton(tr("Импорт события…"), Footer), QString::fromUtf8("Импорт события…"));
        FooterLayout->addWidget(Import);
        Import->setObjectName("StoryEventImport");
        connect(Export, &QPushButton::clicked, this, [this]() {
            Commit();
            if (ExportRequested)
            {
                ExportRequested();
            }
        });
        connect(Import, &QPushButton::clicked, this, [this]() {
            Commit();
            if (ImportRequested)
            {
                ImportRequested();
            }
        });
        auto* Remove = BindEditorCaption(new QPushButton(tr("Убрать из реплики"), Footer), QString::fromUtf8("Убрать из реплики"));
        Remove->setObjectName("StoryEventRemove");
        FooterLayout->addWidget(Remove);
        connect(Remove, &QPushButton::clicked, this, [this]() {
            Commit();
            if (RemoveRequested)
            {
                RemoveRequested();
            }
        });
        Layout->addWidget(Footer);
    }
    Layout->addStretch(1);
    for (auto* Child : findChildren<QWidget*>())
    {
        Child->installEventFilter(this);
    }
    bUpdating = false;
}
void StoryEventInspectorView::Commit()
{
    if (bUpdating || bCollecting || !isEnabled())
    {
        return;
    }
    bCollecting = true;
    if (ActionFields)
    {
        ActionFields->Commit();
    }
    bool Changed = false;
    for (auto* Field : findChildren<QLineEdit*>("StoryEventGroupName"))
    {
        if (!Field->isVisibleTo(this) || !Field->isModified())
        {
            continue;
        }
        for (auto& Group : Source.Groups)
        {
            if (Group.Identity == Field->property("groupIdentity").toString())
            {
                Group.Name = Field->text();
                Changed = true;
            }
        }
        Field->setModified(false);
    }
    if (Name && Name->isModified())
    {
        Source.Name = Name->text();
        Name->setModified(false);
        Changed = true;
    }
    if (Description && Description->toPlainText() != Source.Description)
    {
        Source.Description = Description->toPlainText();
        Changed = true;
    }
    if (Changed && EventChanged)
    {
        EventChanged(Source);
    }
    bCollecting = false;
}
bool StoryEventInspectorView::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Event->type() == QEvent::ShortcutOverride || Event->type() == QEvent::KeyPress)
    {
        auto* Key = static_cast<QKeyEvent*>(Event);
        auto Command = Key->matches(QKeySequence::Undo)   ? UndoRequested
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
