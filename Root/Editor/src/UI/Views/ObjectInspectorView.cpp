#include "UI/Views/ObjectInspectorView.h"
#include "UI/Views/StudioControls.h"
#include "UI/Styles/StudioIcons.h"
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QGroupBox>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStringList>
#include <QToolButton>
#include <QVBoxLayout>

namespace EditorUi
{
QString ObjectInspectorView::ComponentCaption(const QString& Type)
{
    QString Caption = Type.section('.', -1);
    if (Caption.endsWith("Component")) { Caption.chop(9); }
    const QHash<QString, QString> Captions{{"MeshRenderer", tr("Статический меш")},
        {"Camera", tr("Камера")}, {"Light", tr("Освещение")},
        {"PhysicsBody", tr("Коллизия")}, {"CharacterController", tr("Контроллер персонажа")},
        {"NavigationAgent", tr("Навигация")}};
    return Captions.value(Caption, Caption);
}

void ObjectInspectorView::AddComponentSection(const QString& Identity, const QString& Type,
                                             QWidget& Fields, bool Expanded)
{
    const bool Mesh = Type == "engine.MeshRendererComponent";
    auto* Card = Mesh ? new QGroupBox(ComponentCaption(Type), this) : new QWidget(this);
    Card->setObjectName(Mesh ? "MeshInspectorCard" : "ComponentInspectorCard");
    Card->setProperty("componentType", Type);
    Card->setProperty("componentIdentity", Identity);
    auto* Layout = new QVBoxLayout(Card);
    Layout->setContentsMargins(0, Mesh ? 12 : 0, 0, 0);
    Layout->setSpacing(0);
    auto* Toggle = new QToolButton(Card);
    Toggle->setProperty("uiRole", "objectSection");
    Toggle->setText(ComponentCaption(Type));
    Toggle->setToolTip(Type);
    Toggle->setCheckable(true);
    Toggle->setChecked(Expanded);
    Toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    Toggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    Toggle->setIcon(MakeStudioIcon(Expanded ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    Layout->addWidget(Toggle);
    Toggle->setVisible(!Mesh);
    Layout->addWidget(&Fields);
    Fields.setVisible(Expanded);
    if (Mesh)
    {
        auto* Remove = new QPushButton(tr("Удалить компонент"), Card);
        Remove->setObjectName("RemoveStaticMeshComponent");
        connect(Remove, &QPushButton::clicked, this, [this, Identity]() {
            if (RemoveComponentRequested) { RemoveComponentRequested(Identity); }
        });
        Layout->addWidget(Remove);
    }
    AnimateDisclosure(*Toggle, Fields);
    connect(Toggle, &QToolButton::toggled, Card, [Toggle](bool Open) {
        Toggle->setIcon(MakeStudioIcon(Open ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    });
    Components->addWidget(Card);
    auto* Row = new QPushButton(MakeFrontendIcon("Pin", "--text-secondary", "--accent"), ComponentCaption(Type), this);
    Row->setCheckable(true);
    Row->setIconSize(QSize(16, 16));
    Row->setAccessibleName(tr("Фильтр компонента: %1").arg(ComponentCaption(Type)));
    Row->setProperty("uiRole", "componentInstance");
    Row->setProperty("componentIdentity", Identity);
    Row->setProperty("searchText", ComponentCaption(Type) + " " + Type);
    Row->setToolTip(tr("Показать только отмеченные компоненты. Снимите все отметки, чтобы показать все."));
    Row->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    Row->setMinimumWidth(0);
    Row->setVisible(Row->property("searchText").toString().contains(ComponentSearch->text(), Qt::CaseInsensitive));
    connect(Row, &QPushButton::toggled, Card, [this, Toggle](bool bSelected) {
        if (bSelected)
        {
            Toggle->setChecked(true);
        }
        ApplyComponentFilter();
    });
    ComponentRows->addWidget(Row);
    ComponentCount->setText(QString::number(ComponentRows->count()));
    EmptyComponents->hide();
}

void ObjectInspectorView::ApplyComponentFilter()
{
    QStringList SelectedComponents;
    for (int Index = 0; Index < ComponentRows->count(); ++Index)
    {
        auto* Row = qobject_cast<QPushButton*>(ComponentRows->itemAt(Index)->widget());
        if (Row && Row->isChecked())
        {
            SelectedComponents.push_back(Row->property("componentIdentity").toString());
        }
    }
    for (int Index = 0; Index < Components->count(); ++Index)
    {
        auto* Card = Components->itemAt(Index)->widget();
        if (Card)
        {
            Card->setVisible(SelectedComponents.isEmpty()
                || SelectedComponents.contains(Card->property("componentIdentity").toString()));
        }
    }
}

void ObjectInspectorView::ClearComponentOverview()
{
    while (auto* Item = ComponentRows->takeAt(0))
    {
        delete Item->widget();
        delete Item;
    }
    ComponentCount->setText("0");
    EmptyComponents->show();
}

void ObjectInspectorView::RemoveComponentSection(const QString& Identity)
{
    for (auto* Layout : {Components, ComponentRows})
    {
        for (int Index = Layout->count() - 1; Index >= 0; --Index)
        {
            auto* Widget = Layout->itemAt(Index)->widget();
            if (Widget && Widget->property("componentIdentity").toString() == Identity)
            {
                delete Layout->takeAt(Index);
                Widget->blockSignals(true);
                for (auto* Child : Widget->findChildren<QWidget*>()) { Child->blockSignals(true); }
                Widget->hide();
                Widget->deleteLater();
            }
        }
    }
    ComponentCount->setText(QString::number(ComponentRows->count()));
    EmptyComponents->setVisible(ComponentRows->count() == 0);
}

void ObjectInspectorView::MoveComponentSection(const QString& Identity, int Index)
{
    for (auto* Layout : {Components, ComponentRows})
    {
        for (int Current = 0; Current < Layout->count(); ++Current)
        {
            auto* Widget = Layout->itemAt(Current)->widget();
            if (Widget && Widget->property("componentIdentity").toString() == Identity)
            {
                if (Current != Index) { Layout->insertWidget(Index, Widget); }
                break;
            }
        }
    }
}

void ObjectInspectorView::SetIdentity(const QString& Caption, const QString& Kind, bool Enabled)
{
    Name->setText(Caption);
    ObjectKind->setText(Kind);
    const QSignalBlocker Blocker(Active);
    Active->setChecked(Enabled);
    Selection->setText(tr("Выделено: %1").arg(Caption));
    Selection->setToolTip(Caption);
}

ObjectInspectorView::ObjectInspectorView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("ObjectInspectorPage");
    auto* Outer = new QVBoxLayout(this);
    Outer->setContentsMargins(0, 0, 0, 0);
    Outer->setSpacing(0);
    Scroll = new QScrollArea(this);
    Scroll->setFrameShape(QFrame::NoFrame);
    Scroll->setWidgetResizable(true);
    Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* Body = new QWidget(Scroll);
    Body->setObjectName("ObjectInspectorBody");
    auto* Layout = new QVBoxLayout(Body);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    auto* Identity = new QWidget(Body);
    Identity->setObjectName("ObjectIdentity");
    auto* IdentityLayout = new QHBoxLayout(Identity);
    IdentityLayout->setContentsMargins(16, 12, 16, 12);
    auto* IdentityText = new QVBoxLayout();
    IdentityText->setSpacing(4);
    Name = new QLineEdit(Identity);
    Name->setObjectName("ObjectName");
    Name->setPlaceholderText(tr("Имя объекта"));
    Name->setAccessibleName(tr("Имя объекта"));
    Name->setMinimumWidth(0);
    IdentityText->addWidget(Name);
    ObjectKind = new QLabel(tr("Объект сцены"), Identity);
    ObjectKind->setObjectName("ObjectKind");
    IdentityText->addWidget(ObjectKind);
    IdentityLayout->addLayout(IdentityText, 1);
    Active = new StudioCheckBox(Identity);
    Active->setObjectName("ObjectActive");
    Active->setAccessibleName(tr("Объект активен"));
    Active->setToolTip(tr("Объект активен"));
    Active->setFixedWidth(16);
    Active->setChecked(true);
    IdentityLayout->addWidget(Active);
    Layout->addWidget(Identity);
    ComponentTools = new QWidget(Body);
    ComponentTools->setObjectName("ObjectComponents");
    auto* ComponentLayout = new QVBoxLayout(ComponentTools);
    ComponentLayout->setContentsMargins(16, 12, 16, 12);
    ComponentLayout->setSpacing(6);
    auto* Heading = new QHBoxLayout();
    auto* HeadingText = new QLabel(tr("Компоненты"), ComponentTools);
    HeadingText->setProperty("uiRole", "objectSectionTitle");
    Heading->addWidget(HeadingText);
    ComponentCount = new QLabel("0", ComponentTools);
    ComponentCount->setObjectName("ObjectComponentCount");
    Heading->addWidget(ComponentCount);
    Heading->addStretch();
    auto* SearchButton = new QToolButton(ComponentTools);
    SearchButton->setIcon(MakeStudioIcon(StudioIcon::Search));
    SearchButton->setAccessibleName(tr("Поиск компонентов"));
    SearchButton->setCheckable(true);
    Heading->addWidget(SearchButton);
    ComponentLayout->addLayout(Heading);
    ComponentSearch = new QLineEdit(ComponentTools);
    ComponentSearch->setPlaceholderText(tr("Название или тип…"));
    ComponentSearch->setAccessibleName(tr("Поиск компонентов"));
    ComponentSearch->hide();
    ComponentLayout->addWidget(ComponentSearch);
    connect(SearchButton, &QToolButton::toggled, this, [this](bool Visible) {
        ComponentSearch->setVisible(Visible);
        if (Visible) { ComponentSearch->setFocus(); } else { ComponentSearch->clear(); }
    });
    ComponentRows = new QVBoxLayout();
    ComponentRows->setSpacing(3);
    ComponentLayout->addLayout(ComponentRows);
    connect(ComponentSearch, &QLineEdit::textChanged, this, [this](const QString& Query) {
        bool Found = false;
        for (int Index = 0; Index < ComponentRows->count(); ++Index)
        {
            auto* Row = ComponentRows->itemAt(Index)->widget();
            const bool Match = Row->property("searchText").toString().contains(Query, Qt::CaseInsensitive);
            Row->setVisible(Match);
            Found |= Match;
        }
        EmptyComponents->setText(ComponentRows->count() ? tr("Ничего не найдено") : tr("Нет компонентов"));
        EmptyComponents->setVisible(!Found);
    });
    EmptyComponents = new QLabel(tr("Нет компонентов"), ComponentTools);
    EmptyComponents->setObjectName("ObjectComponentEmpty");
    ComponentLayout->addWidget(EmptyComponents);
    auto* AddToggle = new QPushButton(MakeStudioIcon(StudioIcon::Plus, "--accent"), tr("Добавить компонент"), ComponentTools);
    AddToggle->setObjectName("ObjectAddComponent");
    AddToggle->setCheckable(true);
    ComponentLayout->addWidget(AddToggle);
    auto* AddForm = new QWidget(ComponentTools);
    auto* AddLayout = new QVBoxLayout(AddForm);
    AddLayout->setContentsMargins(0, 4, 0, 0);
    AddComponentType = new StudioSelect(AddForm);
    AddComponentType->setAccessibleName(tr("Компонент"));
    AddLayout->addWidget(AddComponentType);
    AddComponent = new QPushButton(tr("Добавить"), AddForm);
    AddLayout->addWidget(AddComponent);
    ComponentLayout->addWidget(AddForm);
    AddForm->hide();
    AnimateDisclosure(*AddToggle, *AddForm);
    connect(AddComponent, &QPushButton::clicked, this, [this, AddToggle]() {
        const int Index = AddComponentType->currentIndex();
        AddToggle->setChecked(false);
        if (AddComponentRequested) { AddComponentRequested(Index); }
    });
    Layout->addWidget(ComponentTools);
    auto* Transform = new QWidget(Body);
    Transform->setObjectName("ObjectTransform");
    auto* Fields = new QVBoxLayout(Transform);
    Fields->setContentsMargins(16, 12, 16, 14);
    Fields->setSpacing(10);
    auto* Title = new QLabel(tr("Трансформация"), Transform);
    Title->setProperty("uiRole", "objectSectionTitle");
    auto* TitleRow = new QHBoxLayout();
    auto* Icon = new QLabel(Transform);
    Icon->setPixmap(MakeStudioIcon(StudioIcon::Move).pixmap(14, 14));
    Icon->setFixedWidth(14);
    TitleRow->setSpacing(7);
    TitleRow->addWidget(Icon);
    TitleRow->addWidget(Title, 1);
    Fields->addLayout(TitleRow);
    Fields->addSpacing(6);
    const QStringList Captions{tr("Положение"), tr("Поворот"), tr("Масштаб")};
    const QStringList Units{tr("м"), QString::fromUtf8("°"), QString::fromUtf8("×")};
    for (int Row = 0; Row < 3; ++Row)
    {
        auto* VectorRow = new QWidget(Transform);
        VectorRow->setObjectName("TransformVector");
        auto* RowLayout = new QHBoxLayout(VectorRow);
        RowLayout->setContentsMargins(0, 0, 0, 0);
        RowLayout->setSpacing(5);
        auto* Caption = new QLabel(Captions[Row], VectorRow);
        Caption->setFixedWidth(58);
        Caption->setProperty("uiRole", "transformCaption");
        RowLayout->addWidget(Caption);
        auto* Unit = new QLabel(Units[Row], VectorRow);
        Unit->setObjectName("TransformUnit");
        Unit->setFixedWidth(7);
        RowLayout->addWidget(Unit);
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            const QString AxisName = QString(QChar('X' + Axis));
            auto* Group = new QWidget(VectorRow);
            auto* AxisLayout = new QHBoxLayout(Group);
            AxisLayout->setContentsMargins(0, 0, 0, 0);
            AxisLayout->setSpacing(4);
            auto* Label = new QLabel(AxisName, Group);
            Label->setObjectName("TransformAxis" + AxisName);
            AxisLayout->addWidget(Label);
            auto* Spin = new StudioNumberEdit(Group);
            Spin->setObjectName("Transform" + QString::number(Row) + AxisName);
            Spin->setAccessibleName(Captions[Row] + " " + AxisName);
            Spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
            Spin->setMinimumWidth(0);
            Spin->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            Spin->setKeyboardTracking(false);
            Spin->setDecimals(3);
            Spin->setRange(-1.0e6, 1.0e6);
            Spin->setSingleStep(Row == 1 ? 1 : .1);
            Spin->SetScrubStep(Row == 1 ? 1.0 : Row == 2 ? 0.01 : 0.1);
            Spin->setToolTip(tr("Тяните влево/вправо для изменения. Shift — точнее, Ctrl — быстрее.\nЩелчок — ввод числа. Esc — отмена перетягивания."));
            AxisLayout->addWidget(Spin, 1);
            Label->setBuddy(Spin);
            TransformFields[Row * 3 + Axis] = Spin;
            RowLayout->addWidget(Group, 1);
        }
        Fields->addWidget(VectorRow);
    }
    Actions = new QWidget(Transform);
    auto* Buttons = new QGridLayout(Actions);
    Buttons->setContentsMargins(0, 2, 0, 0);
    Buttons->setSpacing(5);
    const QStringList Labels{tr("В кадр"), tr("Сбросить"), tr("Дубликат"), tr("Удалить")};
    const StudioIcon Icons[]{StudioIcon::Fit, StudioIcon::Rotate, StudioIcon::Copy, StudioIcon::Trash};
    for (int I = 0; I < 4; ++I)
    {
        auto* Button = new QPushButton(MakeStudioIcon(Icons[I]), Labels[I], Actions);
        if (I == 1) { Button->setIcon(MakeFrontendIcon("RotateCcw")); }
        Button->setObjectName("ObjectAction" + QString::number(I));
        Buttons->addWidget(Button, I / 2, I % 2);
        connect(Button, &QPushButton::clicked, this, [this, I]() {
            const auto Callback = I == 0 ? FrameRequested : I == 1 ? ResetRequested :
                                  I == 2 ? DuplicateRequested : DeleteRequested;
            if (Callback) { Callback(); }
        });
    }
    Fields->addWidget(Actions);
    Layout->addWidget(Transform);
    ComponentFilter = new StudioSelect(Body);
    ComponentFilter->setObjectName("ObjectComponentFilter");
    ComponentFilter->setAccessibleName(tr("Компоненты объекта"));
    Layout->addWidget(ComponentFilter);
    auto* Container = new QWidget(Body);
    Container->setObjectName("ComponentInspectorList");
    Components = new QVBoxLayout(Container);
    Components->setContentsMargins(0, 0, 0, 0);
    Components->setSpacing(0);
    Layout->addWidget(Container);
    auto* SceneToggle = new QToolButton(Body);
    SceneToggle->setProperty("uiRole", "objectSection");
    SceneToggle->setText(tr("Объект сцены"));
    SceneToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    SceneToggle->setIcon(MakeStudioIcon(StudioIcon::ChevronRight));
    SceneToggle->setCheckable(true);
    SceneToggle->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    Layout->addWidget(SceneToggle);
    auto* SceneFields = new QWidget(Body);
    auto* SceneLayout = new QGridLayout(SceneFields);
    SceneLayout->setContentsMargins(16, 8, 16, 14);
    SceneLayout->setHorizontalSpacing(8);
    SceneLayout->setVerticalSpacing(10);
    SceneLayout->setColumnStretch(0, 9);
    SceneLayout->setColumnStretch(1, 13);
    ParentObject = new StudioSelect(SceneFields);
    ParentObject->setObjectName("ObjectParent");
    AnchorObject = new StudioSelect(SceneFields);
    AnchorObject->setObjectName("ObjectAnchor");
    const QStringList SceneLabels{tr("Группа в иерархии"), tr("Опорная точка")};
    const std::array<QComboBox*, 2> SceneControls{ParentObject, AnchorObject};
    for (int Row = 0; Row < 2; ++Row)
    {
        auto* Label = new QLabel(SceneLabels[Row], SceneFields);
        Label->setWordWrap(true);
        Label->setMinimumWidth(0);
        Label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        Label->setBuddy(SceneControls[Row]);
        SceneControls[Row]->setAccessibleName(SceneLabels[Row]);
        SceneLayout->addWidget(Label, Row, 0);
        SceneLayout->addWidget(SceneControls[Row], Row, 1);
    }
    Layout->addWidget(SceneFields);
    SceneFields->hide();
    AnimateDisclosure(*SceneToggle, *SceneFields);
    connect(SceneToggle, &QToolButton::toggled, this, [SceneToggle](bool Open) {
        SceneToggle->setIcon(MakeStudioIcon(Open ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    });
    Layout->addStretch(1);
    Scroll->setWidget(Body);
    Outer->addWidget(Scroll);
    Selection = new QLabel(this);
    Selection->setObjectName("ObjectSelectionFooter");
    Selection->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    Selection->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    Outer->addWidget(Selection);
}

void ObjectInspectorView::SetEditable(bool Editable)
{
    Name->setEnabled(Editable);
    Active->setEnabled(Editable);
    ComponentTools->findChild<QPushButton*>("ObjectAddComponent")->setEnabled(Editable);
    AddComponentType->setEnabled(Editable);
    AddComponent->setEnabled(Editable);
    ParentObject->setEnabled(Editable);
    AnchorObject->setEnabled(Editable);
    Actions->setEnabled(Editable);
    ComponentFilter->setEnabled(Editable);
    for (auto* Remove : findChildren<QPushButton*>("RemoveStaticMeshComponent"))
    { Remove->setEnabled(Editable); }
}
}
