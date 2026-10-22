#include "UI/Views/StudioActionMenu.h"
#include "UI/Styles/ControlStyle.h"
#include "UI/Styles/ActionMenuStyle.h"
#include "UI/Styles/EditorStyle.h"
#include "UI/Styles/StudioIcons.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QPainter>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <algorithm>

namespace EditorUi
{
namespace
{
class MenuRow final : public QWidget
{
public:
    MenuRow(const StudioMenuItem& Item, int Width, QWidget* Parent)
        : QWidget(Parent), Text(Item.Label), Icon(Item.Icon), bSubmenu(!Item.Children.isEmpty())
    {
        setFixedSize(ActionMenuStyle::RowSize(Text, Width, bSubmenu));
        setAccessibleName(Text.simplified());
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
    }
    void SetActive(bool Active) { bActive = Active; update(); }
protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter Painter(this);
        ActionMenuStyle::PaintRow(Painter, rect(), Text, Icon, bSubmenu, bActive, isEnabled());
    }
private:
    QString Text;
    QIcon Icon;
    bool bSubmenu = false, bActive = false;
};

QWidgetAction* LabelAction(QMenu& Menu, const QString& Text, const char* Role)
{
    auto* Action = new QWidgetAction(&Menu);
    auto* Label = new QLabel(Text, &Menu);
    Label->setObjectName(QString::fromLatin1(Role));
    Label->setWordWrap(true);
    Action->setDefaultWidget(Label);
    Action->setEnabled(false);
    Menu.addAction(Action);
    return Action;
}

void FindItems(const QVector<StudioMenuItem>& Items, const QString& Query, const QString& Path,
               bool Enabled, QVector<StudioMenuItem>& Results)
{
    for (const auto& Item : Items)
    {
        const QString Group = Path.isEmpty() ? Item.Group : Path;
        if (!Item.Children.isEmpty())
        {
            FindItems(Item.Children, Query, Group + QLatin1Char(' ') + Item.Label,
                      Enabled && Item.bEnabled, Results);
        }
        else if ((Item.Label + QLatin1Char(' ') + Group + QLatin1Char(' ') + Item.SearchText)
                     .contains(Query, Qt::CaseInsensitive))
        {
            auto Result = Item;
            Result.Group = Group.trimmed();
            Result.bEnabled = Enabled && Item.bEnabled;
            Results.push_back(std::move(Result));
        }
    }
}
}

StudioActionMenu::StudioActionMenu(const QString& Title, QVector<StudioMenuItem> InItems, QWidget* Parent)
    : StudioActionMenu(std::move(InItems), nullptr, Parent)
{
    Root = this;
    setTitle(Title);
    setAccessibleName(Title);
    // Rebuild beneath a persistent header: typing must never replace the focused editor.
    clear();
    Content.clear();
    auto* HeaderAction = new QWidgetAction(this);
    auto* Header = new QWidget(this);
    Header->setFixedWidth(ActionMenuStyle::Width - 2 * ActionMenuStyle::Inset);
    auto* Layout = new QVBoxLayout(Header);
    Layout->setContentsMargins(0, 0, 0, 5);
    Layout->setSpacing(0);
    auto* Heading = new QLabel(Title, Header);
    Heading->setObjectName("StudioMenuTitle");
    Layout->addWidget(Heading);
    auto* SearchBox = new QWidget(Header);
    SearchBox->setObjectName("StudioMenuSearchBox");
    auto* SearchLayout = new QHBoxLayout(SearchBox);
    SearchLayout->setContentsMargins(7, 4, 7, 4);
    SearchLayout->setSpacing(7);
    auto* Icon = new QLabel(SearchBox);
    Icon->setPixmap(MakeStudioIcon(StudioIcon::Search).pixmap(14, 14));
    SearchLayout->addWidget(Icon);
    Search = new QLineEdit(SearchBox);
    Search->setObjectName("StudioMenuSearch");
    Search->setPlaceholderText(tr("Поиск…"));
    Search->setAccessibleName(tr("Поиск в контекстном меню"));
    Search->installEventFilter(this);
    SearchLayout->addWidget(Search, 1);
    Layout->addWidget(SearchBox);
    HeaderAction->setDefaultWidget(Header);
    addAction(HeaderAction);
    Populate(Items);
    connect(Search, &QLineEdit::textChanged, this, &StudioActionMenu::Filter);
    connect(this, &QMenu::aboutToHide, this, &QObject::deleteLater);
}

StudioActionMenu::StudioActionMenu(QVector<StudioMenuItem> InItems, StudioActionMenu* InRoot, QWidget* Parent)
    : QMenu(Parent), Items(std::move(InItems)), Root(InRoot)
{
    setObjectName("StudioActionMenu");
    setFont(ControlStyle::Font());
    setStyleSheet(EditorStyle::LoadStylesheet());
    ensurePolished();
    // QMenu caches SH_Menu_Scrollable before the object-name QSS selector is
    // polished. Refresh that cache now, otherwise Qt creates offscreen columns.
    QEvent StyleChanged(QEvent::StyleChange);
    QMenu::changeEvent(&StyleChanged);
    setToolTipsVisible(true);
    setFixedWidth(InRoot ? ActionMenuStyle::SubmenuWidth : ActionMenuStyle::Width);
    if (InRoot) { Populate(Items); }
}

void StudioActionMenu::Populate(const QVector<StudioMenuItem>& Entries)
{
    QString PreviousGroup;
    for (const auto& Item : Entries)
    {
        if (!Item.Group.isEmpty() && Item.Group != PreviousGroup)
        {
            Content.push_back(LabelAction(*this, Item.Group, "StudioMenuGroup"));
        }
        PreviousGroup = Item.Group;
        auto* Action = new QWidgetAction(this);
        auto* Row = new MenuRow(Item, width() - 2 * ActionMenuStyle::Inset, this);
        Action->setDefaultWidget(Row);
        Action->setText(Item.Label);
        Action->setProperty("studioCommand", true);
        Action->setObjectName(Item.Identity);
        Action->setEnabled(Item.bEnabled);
        Action->setToolTip(Item.Description);
        Row->setToolTip(Item.Description);
        addAction(Action);
        connect(this, &QMenu::hovered, Row, [Row, Action](QAction* Hovered) { Row->SetActive(Hovered == Action); });
        Content.push_back(Action);
        if (!Item.Children.isEmpty())
        {
            auto* Submenu = new StudioActionMenu(Item.Children, Root, this);
            Submenu->setTitle(Item.Label);
            Action->setMenu(Submenu);
        }
        else
        {
            connect(Action, &QAction::triggered, this, [Root = Root, Invoke = Item.Invoke, Enabled = Item.bEnabled]()
            {
                if (!Enabled || !Invoke) { return; }
                // A command may enter a modal loop which processes deferred menu deletion.
                const auto Command = Invoke;
                if (Root) { Root->close(); }
                Command();
            });
        }
    }
    if (Entries.isEmpty())
    {
        Content.push_back(LabelAction(*this, tr("Ничего не найдено"), "StudioMenuEmpty"));
    }
}

void StudioActionMenu::Filter(const QString& Query)
{
    setActiveAction(nullptr);
    for (auto* Action : Content)
    {
        auto* Submenu = Action->menu();
        if (Submenu) { Submenu->hide(); }
        removeAction(Action);
        if (Submenu) { delete Submenu; }
        delete Action;
    }
    Content.clear();
    QVector<StudioMenuItem> Results;
    const QString Trimmed = Query.trimmed();
    if (Trimmed.isEmpty()) { Results = Items; }
    else { FindItems(Items, Trimmed, {}, true, Results); }
    Populate(Results);
    if (isVisible())
    {
        adjustSize();
        auto* Screen = QGuiApplication::screenAt(Anchor);
        if (!Screen) { Screen = screen(); }
        const QRect Available = Screen->availableGeometry().adjusted(8, 8, -8, -8);
        move(std::clamp(Anchor.x(), Available.left(), std::max(Available.left(), Available.right() - width() + 1)),
             std::clamp(Anchor.y(), Available.top(), std::max(Available.top(), Available.bottom() - height() + 1)));
    }
}

void StudioActionMenu::Open(const QPoint& Position)
{
    Anchor = Position;
    popup(Position);
    if (Search) { Search->setFocus(Qt::PopupFocusReason); }
}

void StudioActionMenu::ActivateFirst()
{
    for (auto* Action : Content)
    {
        if (!Action->isEnabled() || !Action->property("studioCommand").toBool()) { continue; }
        Activate(Action);
        break;
    }
}

void StudioActionMenu::Activate(QAction* Action)
{
    if (!Action || !Action->isEnabled() || !Action->property("studioCommand").toBool()) { return; }
    if (Action->menu())
    {
        setActiveAction(Action);
        setFocus(Qt::PopupFocusReason);
        QKeyEvent Right(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier);
        QMenu::keyPressEvent(&Right);
    }
    else { Action->trigger(); }
}

bool StudioActionMenu::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Watched == Search && Event->type() == QEvent::ShortcutOverride)
    {
        // Editing search must not reach document shortcuts on the underlying canvas.
        Event->accept();
        return true;
    }
    if (Watched == Search && Event->type() == QEvent::KeyPress)
    {
        auto* Key = static_cast<QKeyEvent*>(Event);
        if (Key->key() == Qt::Key_Return || Key->key() == Qt::Key_Enter) { ActivateFirst(); return true; }
        if (Key->key() == Qt::Key_Escape) { close(); return true; }
        if (Key->key() == Qt::Key_Down || Key->key() == Qt::Key_Up)
        {
            setFocus(Qt::PopupFocusReason);
            keyPressEvent(Key);
            return true;
        }
        if (Key->key() == Qt::Key_Tab) { close(); return true; }
    }
    return QMenu::eventFilter(Watched, Event);
}

void StudioActionMenu::keyPressEvent(QKeyEvent* Event)
{
    const int Key = Event->key();
    if (Key == Qt::Key_Down || Key == Qt::Key_Up || Key == Qt::Key_Home || Key == Qt::Key_End)
    {
        // QMenu's default navigation includes the QWidgetAction search header.
        // Never send an activation key back to that header (it bubbles to QMenu).
        QVector<QAction*> Commands;
        for (auto* Action : Content)
        {
            if (Action->isVisible() && Action->isEnabled() && Action->property("studioCommand").toBool())
            { Commands.push_back(Action); }
        }
        if (Commands.isEmpty()) { Event->accept(); return; }
        int Index = Commands.indexOf(activeAction());
        if (Key == Qt::Key_Home) { Index = 0; }
        else if (Key == Qt::Key_End) { Index = Commands.size() - 1; }
        else if (Index < 0) { Index = Key == Qt::Key_Down ? 0 : Commands.size() - 1; }
        else { Index = (Index + (Key == Qt::Key_Down ? 1 : -1) + Commands.size()) % Commands.size(); }
        setActiveAction(Commands[Index]);
        Event->accept();
        return;
    }
    if (Key == Qt::Key_Return || Key == Qt::Key_Enter)
    {
        Activate(activeAction());
        Event->accept();
        return;
    }
    if (Key == Qt::Key_Tab)
    {
        if (Root) { Root->close(); }
        Event->accept();
        return;
    }
    if (Search && !Event->text().isEmpty() && Event->text().front().isPrint() &&
        !(Event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)))
    {
        Search->setFocus();
        QApplication::sendEvent(Search, Event);
        return;
    }
    QMenu::keyPressEvent(Event);
}
}
