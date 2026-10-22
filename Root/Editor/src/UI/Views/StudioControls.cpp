#include "UI/Views/StudioControls.h"
#include "UI/Styles/ControlStyle.h"
#include "UI/Styles/EditorStyle.h"
#include <QApplication>
#include <QGraphicsOpacityEffect>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QLabel>
#include <QPersistentModelIndex>
#include <QPainter>
#include <QPropertyAnimation>
#include <QScreen>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QVBoxLayout>
#include <algorithm>

namespace EditorUi
{
QString StudioNumberEdit::textFromValue(double Value) const
{
    QString Text = QDoubleSpinBox::textFromValue(Value);
    const QString Decimal = locale().decimalPoint();
    if (Text.contains(Decimal))
    {
        const QString Zero = locale().zeroDigit();
        while (Text.endsWith(Zero)) { Text.chop(Zero.size()); }
        if (Text.endsWith(Decimal)) { Text.chop(Decimal.size()); }
    }
    return Text;
}

namespace
{
class OptionDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override
    { return {100, ControlStyle::OptionHeight}; }
    void paint(QPainter* Painter, const QStyleOptionViewItem& Style, const QModelIndex& Index) const override
    {
        ControlStyle::Option(*Painter, Style.rect, Index.data().toString(), Index.data(Qt::UserRole + 1).toBool(),
                             Style.state.testFlag(QStyle::State_Selected) || Style.state.testFlag(QStyle::State_MouseOver),
                             Style.state.testFlag(QStyle::State_Enabled), Style.font);
    }
};
}

StudioSelect::StudioSelect(QWidget* Parent) : QComboBox(Parent)
{
    setEditable(false);
    setMinimumWidth(0);
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    setFocusPolicy(Qt::StrongFocus);
    setProperty("uiRole", "studioSelect");
    connect(model(), &QAbstractItemModel::modelReset, this, &StudioSelect::ClosePopup);
    connect(model(), &QAbstractItemModel::rowsRemoved, this, &StudioSelect::ClosePopup);
}

StudioSelect::~StudioSelect() { ClosePopup(); }

void StudioSelect::SetPopupOpen(bool Open)
{
    if (property("popupOpen").toBool() == Open) { return; }
    setProperty("popupOpen", Open);
    style()->unpolish(this);
    style()->polish(this);
    update();
}

void StudioSelect::showPopup()
{
    if (!isEnabled() || !count()) { return; }
    if (Popup)
    {
        if (!bClosing) { hidePopup(); return; }
        // Reverse an in-flight close on the same surface; creating a second Qt::Popup
        // before the first one detaches lets Qt's focus restoration close the new one.
        bClosing = false;
        TransitionPopup(true);
        Search->setFocus(Qt::PopupFocusReason);
        return;
    }
    bClosing = false;
    SetPopupOpen(true);
    Popup = new QWidget(this, Qt::Popup | Qt::FramelessWindowHint);
    Popup->setObjectName("StudioSelectPopup");
    Popup->setAttribute(Qt::WA_DeleteOnClose);
    Popup->setAttribute(Qt::WA_StyledBackground);
    Popup->setFont(ControlStyle::Font(13));
    // A popup is a top-level surface; explicitly inherit the same resolved stylesheet.
    Popup->setStyleSheet(EditorStyle::LoadStylesheet());
    Popup->installEventFilter(this);
    auto* Layout = new QVBoxLayout(Popup);
    Layout->setContentsMargins(5, 5, 5, 5);
    Layout->setSpacing(4);
    Search = new QLineEdit(Popup);
    Search->setObjectName("StudioSelectSearch");
    Search->setPlaceholderText(tr("Поиск…"));
    Search->setClearButtonEnabled(true);
    Search->setFixedHeight(30);
    Search->installEventFilter(this);
    Layout->addWidget(Search);
    auto* Empty = new QLabel(tr("Ничего не найдено"), Popup);
    Empty->setObjectName("MutedLabel");
    Empty->setAlignment(Qt::AlignCenter);
    Empty->setFixedHeight(ControlStyle::OptionHeight);
    Empty->hide();
    Layout->addWidget(Empty);
    Options = new QListWidget(Popup);
    Options->setObjectName("StudioSelectOptions");
    Options->setMouseTracking(true);
    Options->setEditTriggers(QAbstractItemView::NoEditTriggers);
    Options->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    Options->setItemDelegate(new OptionDelegate(Options));
    Options->installEventFilter(this);
    for (int I = 0; I < count(); ++I)
    {
        auto* Item = new QListWidgetItem(itemText(I), Options);
        Item->setData(Qt::UserRole, I);
        Item->setData(Qt::UserRole + 1, I == currentIndex());
        Item->setData(Qt::UserRole + 2, QVariant::fromValue(QPersistentModelIndex(model()->index(I, modelColumn(), rootModelIndex()))));
        Item->setToolTip(itemText(I));
        Item->setFlags(model()->flags(model()->index(I, modelColumn(), rootModelIndex())) &
                       (Qt::ItemIsEnabled | Qt::ItemIsSelectable));
    }
    Options->setCurrentRow(currentIndex());
    Layout->addWidget(Options, 1);
    connect(Search, &QLineEdit::textChanged, Popup, [this, Empty](const QString& Query) {
        QListWidgetItem* First = nullptr;
        for (int I = 0; I < Options->count(); ++I)
        {
            auto* Item = Options->item(I);
            Item->setHidden(!Item->text().contains(Query, Qt::CaseInsensitive));
            if (!First && !Item->isHidden() && Item->flags().testFlag(Qt::ItemIsEnabled)) { First = Item; }
        }
        Options->setCurrentItem(First);
        Empty->setVisible(!First);
    });
    connect(Options, &QListWidget::itemClicked, this, [this]() { Choose(); });
    const QRect Available = screen()->availableGeometry().adjusted(8, 8, -8, -8);
    const int Width = std::min(std::max(width(), 280), Available.width());
    const int Height = std::min(10 + 34 + std::min(count(), 8) * ControlStyle::OptionHeight, Available.height());
    QPoint Position = mapToGlobal(QPoint(0, height() + 4));
    Position.setX(std::clamp(Position.x(), Available.left(), Available.right() - Width + 1));
    if (Position.y() + Height > Available.bottom()) { Position.setY(mapToGlobal(QPoint()).y() - Height - 4); }
    Position.setY(std::clamp(Position.y(), Available.top(), Available.bottom() - Height + 1));
    Popup->setGeometry(QRect(Position, QSize(Width, Height)));
    auto* Fade = new QGraphicsOpacityEffect(Popup);
    Fade->setOpacity(0);
    Popup->setGraphicsEffect(Fade);
    Popup->show();
    TransitionPopup(true);
    Search->setFocus(Qt::PopupFocusReason);
    Options->scrollToItem(Options->currentItem());
}

void StudioSelect::Choose()
{
    if (bClosing || !Options || !Options->currentItem()) { return; }
    auto* Item = Options->currentItem();
    if (Item->isHidden() || !Item->flags().testFlag(Qt::ItemIsEnabled)) { return; }
    const auto Source = Item->data(Qt::UserRole + 2).value<QPersistentModelIndex>();
    if (!Source.isValid() || Source.model() != model() || !model()->flags(Source).testFlag(Qt::ItemIsEnabled)) { return; }
    const int Index = Source.row();
    QPointer<StudioSelect> Guard(this);
    hidePopup();
    setFocus(Qt::PopupFocusReason);
    setCurrentIndex(Index);
    if (!Guard) { return; }
    emit activated(Index);
    if (Guard) { emit textActivated(itemText(Index)); }
}

void StudioSelect::ClosePopup()
{
    if (Popup)
    {
        auto* Closing = Popup.data();
        for (auto* Animation : Closing->findChildren<QPropertyAnimation*>())
        {
            Animation->stop();
            disconnect(Animation, nullptr, this, nullptr);
        }
        Popup = nullptr;
        Options = nullptr;
        Search = nullptr;
        Closing->removeEventFilter(this);
        Closing->close();
    }
    bClosing = false;
    SetPopupOpen(false);
    QComboBox::hidePopup();
}

void StudioSelect::hidePopup()
{
    if (!Popup || bClosing) { return; }
    bClosing = true;
    TransitionPopup(false);
}

void StudioSelect::TransitionPopup(bool Opening)
{
    auto* Fade = qobject_cast<QGraphicsOpacityEffect*>(Popup->graphicsEffect());
    const auto Animations = Popup->findChildren<QPropertyAnimation*>();
    for (auto* Animation : Animations)
    {
        Animation->stop();
        disconnect(Animation, nullptr, this, nullptr);
    }
    auto* Animation = new QPropertyAnimation(Fade, "opacity", Popup);
    Animation->setDuration(Opening ? ControlStyle::MotionDuration : 90);
    Animation->setStartValue(Fade->opacity());
    Animation->setEndValue(Opening ? 1.0 : 0.0);
    Animation->setEasingCurve(QEasingCurve::OutCubic);
    if (!Opening) { connect(Animation, &QPropertyAnimation::finished, this, &StudioSelect::ClosePopup); }
    Animation->start(QAbstractAnimation::DeleteWhenStopped);
}

bool StudioSelect::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Watched == Popup && Event->type() == QEvent::Hide)
    {
        // Qt closes a Popup on outside click or deactivation.
        ClosePopup();
    }
    if ((Watched == Search || Watched == Options) && Event->type() == QEvent::KeyPress)
    {
        auto* Key = static_cast<QKeyEvent*>(Event);
        if (Key->key() == Qt::Key_Escape) { hidePopup(); setFocus(); return true; }
        if (Key->key() == Qt::Key_Return || Key->key() == Qt::Key_Enter) { Choose(); return true; }
        if (Key->key() == Qt::Key_Tab) { ClosePopup(); focusNextPrevChild(!Key->modifiers().testFlag(Qt::ShiftModifier)); return true; }
        if (Watched == Search && (Key->key() == Qt::Key_Down || Key->key() == Qt::Key_Up ||
                                  Key->key() == Qt::Key_Home || Key->key() == Qt::Key_End))
        {
            QApplication::sendEvent(Options, Key);
            return true;
        }
    }
    return QComboBox::eventFilter(Watched, Event);
}

void StudioSelect::keyPressEvent(QKeyEvent* Event)
{
    if (Event->key() == Qt::Key_Space || Event->key() == Qt::Key_Return || Event->key() == Qt::Key_Enter ||
        Event->key() == Qt::Key_Down || Event->key() == Qt::Key_Up)
    { showPopup(); Event->accept(); return; }
    QComboBox::keyPressEvent(Event);
}

StudioCheckBox::StudioCheckBox(QWidget* Parent) : StudioCheckBox(QString{}, Parent) {}
StudioCheckBox::StudioCheckBox(const QString& Text, QWidget* Parent) : QCheckBox(Text, Parent)
{
    setMinimumHeight(24);
    Motion.setDuration(ControlStyle::MotionDuration);
    Motion.setEasingCurve(QEasingCurve::InOutCubic);
    connect(&Motion, &QVariantAnimation::valueChanged, this, [this](const QVariant& Value) {
        Progress = Value.toReal(); update();
    });
    connect(this, &QCheckBox::toggled, this, [this](bool Checked) {
        Motion.stop();
        if (!isVisible()) { Progress = Checked ? 1 : 0; update(); return; }
        Motion.setStartValue(Progress);
        Motion.setEndValue(Checked ? 1.0 : 0.0);
        Motion.start();
    });
}
bool StudioCheckBox::hitButton(const QPoint& Position) const { return rect().contains(Position); }
void StudioCheckBox::paintEvent(QPaintEvent*)
{
    QPainter Painter(this);
    const bool Rtl = layoutDirection() == Qt::RightToLeft;
    ControlStyle::Check(Painter, QRectF(Rtl ? width() - 15 : 1, (height() - 14) / 2.0, 14, 14),
                         Progress, isEnabled(), hasFocus());
    Painter.setFont(font());
    Painter.setPen(EditorStyle::Color(isEnabled() ? "--text-secondary" : "--text-disabled"));
    Painter.drawText(rect().adjusted(Rtl ? 0 : 23, 0, Rtl ? -23 : 0, 0),
                     Qt::AlignVCenter | (Rtl ? Qt::AlignRight : Qt::AlignLeft), text());
}

void AnimateDisclosure(QAbstractButton& Toggle, QWidget& Fields)
{
    auto* Motion = new QVariantAnimation(&Fields);
    Motion->setDuration(ControlStyle::MotionDuration);
    Motion->setEasingCurve(QEasingCurve::InOutCubic);
    QObject::connect(Motion, &QVariantAnimation::valueChanged, &Fields, [&Fields](const QVariant& Value) {
        Fields.setMaximumHeight(Value.toInt());
    });
    QObject::connect(Motion, &QVariantAnimation::finished, &Fields, [Toggle = QPointer<QAbstractButton>(&Toggle), &Fields]() {
        if (!Toggle) { return; }
        Fields.setVisible(Toggle->isChecked());
        Fields.setMaximumHeight(Toggle->isChecked() ? QWIDGETSIZE_MAX : 0);
    });
    QObject::connect(&Toggle, &QAbstractButton::toggled, &Fields, [Motion, &Fields](bool Expanded) {
        Motion->stop();
        const int Start = Fields.isHidden() ? 0 : Fields.height();
        Fields.setMaximumHeight(QWIDGETSIZE_MAX);
        Fields.ensurePolished();
        if (Fields.layout()) { Fields.layout()->activate(); }
        const int Target = Expanded ? Fields.sizeHint().height() : 0;
        Fields.setMaximumHeight(Start);
        Fields.setVisible(true);
        Motion->setStartValue(Start);
        Motion->setEndValue(Target);
        Motion->start();
    });
}
}
