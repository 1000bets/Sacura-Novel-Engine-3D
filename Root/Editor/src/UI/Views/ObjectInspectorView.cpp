#include "UI/Views/ObjectInspectorView.h"
#include "UI/Views/StudioControls.h"
#include "UI/Styles/StudioIcons.h"
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace EditorUi
{
QString ObjectInspectorView::ComponentCaption(const QString& Type)
{
    QString Caption = Type.section('.', -1);
    if (Caption.endsWith("Component")) { Caption.chop(9); }
    const QHash<QString, QString> Captions{{"MeshRenderer", tr("Меш и материалы")},
        {"Camera", tr("Камера")}, {"Light", tr("Освещение")},
        {"PhysicsBody", tr("Коллизия")}, {"CharacterController", tr("Контроллер персонажа")},
        {"NavigationAgent", tr("Навигация")}};
    return Captions.value(Caption, Caption);
}

void ObjectInspectorView::AddComponentSection(const QString& Identity, const QString& Type,
                                             QWidget& Fields, bool Expanded)
{
    auto* Card = new QWidget(this);
    Card->setObjectName("ComponentInspectorCard");
    Card->setProperty("componentType", Type);
    Card->setProperty("componentIdentity", Identity);
    auto* Layout = new QVBoxLayout(Card);
    Layout->setContentsMargins(0, 0, 0, 0);
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
    Layout->addWidget(&Fields);
    Fields.setVisible(Expanded);
    AnimateDisclosure(*Toggle, Fields);
    connect(Toggle, &QToolButton::toggled, Card, [Toggle](bool Open) {
        Toggle->setIcon(MakeStudioIcon(Open ? StudioIcon::ChevronDown : StudioIcon::ChevronRight));
    });
    Components->addWidget(Card);
}

ObjectInspectorView::ObjectInspectorView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("ObjectInspectorPage");
    auto* Outer = new QVBoxLayout(this);
    Outer->setContentsMargins(0, 0, 0, 0);
    auto* Scroll = new QScrollArea(this);
    Scroll->setWidgetResizable(true);
    Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* Body = new QWidget(Scroll);
    Body->setObjectName("ObjectInspectorBody");
    auto* Layout = new QVBoxLayout(Body);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    auto* Identity = new QWidget(Body);
    Identity->setObjectName("ObjectIdentity");
    auto* IdentityLayout = new QVBoxLayout(Identity);
    IdentityLayout->setContentsMargins(16, 12, 16, 12);
    Name = new QLineEdit(Identity);
    Name->setObjectName("ObjectName");
    Name->setPlaceholderText(tr("Имя объекта"));
    IdentityLayout->addWidget(Name);
    Layout->addWidget(Identity);
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
    TitleRow->addWidget(Icon);
    TitleRow->addWidget(Title, 1);
    Fields->addLayout(TitleRow);
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
        Caption->setFixedWidth(60);
        Caption->setProperty("uiRole", "transformCaption");
        RowLayout->addWidget(Caption);
        auto* Unit = new QLabel(Units[Row], VectorRow);
        Unit->setObjectName("TransformUnit");
        Unit->setFixedWidth(8);
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
    Layout->addStretch(1);
    Scroll->setWidget(Body);
    Outer->addWidget(Scroll);
}

void ObjectInspectorView::SetEditable(bool Editable)
{
    Name->setEnabled(Editable);
    Actions->setEnabled(Editable);
    ComponentFilter->setEnabled(Editable);
}
}
