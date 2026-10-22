#pragma once
#include "UI/Presentation/StudioMenuItem.h"
#include <QMenu>
#include <QPointer>

class QLineEdit;
namespace EditorUi
{
// Native popup ownership, dismissal, submenu placement and scrolling are retained.
// The same data drives the unfiltered menu and recursive search results.
class StudioActionMenu final : public QMenu
{
public:
    StudioActionMenu(const QString& Title, QVector<StudioMenuItem> Items, QWidget* Parent = nullptr);
    void Open(const QPoint& Position);
protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;
    void keyPressEvent(QKeyEvent* Event) override;
private:
    StudioActionMenu(QVector<StudioMenuItem> Items, StudioActionMenu* Root, QWidget* Parent);
    void Populate(const QVector<StudioMenuItem>& Items);
    void Filter(const QString& Query);
    void ActivateFirst();
    void Activate(QAction* Action);
    QVector<StudioMenuItem> Items;
    QVector<QAction*> Content;
    QLineEdit* Search = nullptr;
    QPointer<StudioActionMenu> Root;
    QPoint Anchor;
};
}
