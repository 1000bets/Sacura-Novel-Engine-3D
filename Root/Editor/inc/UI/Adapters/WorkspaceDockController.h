#pragma once
#include <QObject>
#include <QPointer>
#include <QMap>
#include <QPoint>
#include <QRect>
#include <memory>

class QAction;
class QMenu;
class QSettings;
class QRubberBand;
class QSplitter;
class QTabWidget;
class QTimer;
class QWidget;

namespace EditorUi
{
// Local editor preferences only; moving a page never recreates its document or viewport.
class WorkspaceDockController final : public QObject
{
public:
    explicit WorkspaceDockController(QWidget& Owner, const QString& SettingsFile = {});
    ~WorkspaceDockController() override;
    void AddArea(const QString& Id, const QString& Title, QTabWidget& Tabs, QWidget& Region);
    void AddPage(const QString& Id, QWidget& Page, const QString& Area, QAction& Visibility,
                 bool bPreviouslyFixed = false);
    void SetSplitters(QSplitter& Workspace, QSplitter& Center);
    void Open(QWidget& Page);
    void SetOpen(QWidget& Page, bool Open);
    void Dock(QWidget& Page, const QString& Area, int Index = -1);
    void RestoreDefault(QWidget& Page);
    void Float(QWidget& Page, QRect Geometry = {});
    void SetWorkspacePosition(const QString& Position);
    QString WorkspacePosition() const { return Position; }
    bool Restore();
    void Save();
    void Reset();

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private:
    struct AreaState { QString Title; QPointer<QTabWidget> Tabs; QPointer<QWidget> Region; };
    struct PageState
    {
        QPointer<QWidget> Page, Floating;
        QPointer<QAction> Visibility;
        QString Title, Area, DefaultArea;
        int DefaultIndex = 0;
        bool Open = true, DefaultOpen = true;
        bool bPreviouslyFixed = false;
    };
    QString PageId(QWidget* Page) const;
    QTabWidget* Host(QWidget* Page) const;
    void UpdateArea(QTabWidget& Tabs);
    void Detach(PageState& State);
    void ShowMenu(const QString& Id, QPoint Position);
    void ScheduleSave();
    QRect VisibleGeometry(QRect Geometry) const;
    QWidget& Owner;
    QMap<QString, AreaState> Areas;
    QMap<QString, PageState> Pages;
    QPointer<QSplitter> Workspace, Center;
    QPointer<QRubberBand> DropIndicator;
    std::unique_ptr<QSettings> Settings;
    QTimer* SaveTimer = nullptr;
    QString DragId, DragToken;
    QString Position = "bottom";
    QPoint DragStart;
    bool bRestoring = true;
};
}
