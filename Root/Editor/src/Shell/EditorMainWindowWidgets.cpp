#include "Shell/EditorMainWindow.h"
#include "UI/Views/WidgetWorkspaceView.h"
#include "UI/Adapters/WorkspaceDockController.h"
#include "UI/Views/StoryWorkspaceView.h"
#include "UI/Views/StoryInspectorView.h"
#include "UI/Views/HierarchyPanelView.h"
#include "UI/StoryWidget.h"
#include "UI/WidgetLayout.h"
#include "Project/ProjectPaths.h"
#include "Panels/ContentBrowserWidget.h"
#include "Engine.h"
#include <QApplication>
#include <QTabWidget>
#include <QPushButton>
#include <QStatusBar>

void EditorMainWindow::InitializeWidgetAuthoring()
{
    WidgetWorkspace = new EditorUi::WidgetWorkspaceView(DockTabs);
    DockTabs->addTab(WidgetWorkspace, tr("Виджеты UI"));
    WidgetWorkspace->Open(QString::fromStdWString(ProjectPaths::Content().wstring()));
    WidgetWorkspace->Changed = [this]() { UpdateWindowTitleDirty(); };
    WidgetWorkspace->Saved = [this]()
    {
        auto Result = BoundEngine.GetAssetRegistry().ScanContent();
        if (Result.HasError())
            statusBar()->showMessage(QString::fromStdString(Result.Message), 12000);
        ContentBrowser->Refresh();
    };
    WidgetWorkspace->PreviewRequested = [this](QJsonObject Widget)
    {
        PendingWidgetPreview = std::move(Widget);
        PlayButton->setChecked(true);
        Docking->Open(*StoryDockPage);
    };
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget *, QWidget *Focus)
            {
                if (!Focus)
                    return;
                if (Focus == WidgetWorkspace || WidgetWorkspace->isAncestorOf(Focus))
                {
                    bWidgetEditingContext = true;
                    bStoryEditingContext = false;
                }
                else if (Focus == StoryGraph || StoryGraph->isAncestorOf(Focus) || Focus == StoryInspector ||
                         StoryInspector->isAncestorOf(Focus) || Focus == PrimaryViewport ||
                         PrimaryViewport->isAncestorOf(Focus) || Focus == ObjectInspectorContent ||
                         ObjectInspectorContent->isAncestorOf(Focus) || Focus == HierarchyContent ||
                         HierarchyContent->isAncestorOf(Focus))
                    bWidgetEditingContext = false;
            });
    // Only explicit tab activation changes the editing context. Removing a docked
    // page also changes currentIndex in the old area, without activating its fallback.
    for (auto* Tabs : findChildren<QTabWidget*>())
    connect(Tabs, &QTabWidget::tabBarClicked, this,
            [this,Tabs](int Index)
            {
                if (Tabs->widget(Index) == WidgetWorkspace)
                {
                    bWidgetEditingContext = true;
                    bStoryEditingContext = false;
                }
                else if (Tabs->widget(Index) == StoryGraph)
                {
                    bWidgetEditingContext = false;
                    bStoryEditingContext = true;
                }
            });
    RegisterAssetEditor(AssetType{"WidgetLibrary"},
                        [this](const AssetKey &, const AssetRegistryEntry &Entry, const QString &)
                        {
                            if (std::filesystem::path(Entry.AbsolutePath).lexically_normal() ==
                                (ProjectPaths::Content() / "UI/Project.widgets").lexically_normal())
                            {
                                Docking->Open(*WidgetWorkspace);
                                bWidgetEditingContext = true;
                                bStoryEditingContext = false;
                            }
                            else
                                statusBar()->showMessage(
                                    tr("Эта библиотека не назначена проекту. Для переноса отдельных виджетов "
                                       "используйте Импорт во вкладке «Виджеты UI»."),
                                    10000);
                        });
}
bool EditorMainWindow::IsWidgetEditingContext() const
{
    return WidgetWorkspace && WidgetWorkspace->GetDocument().IsOpen() && bWidgetEditingContext;
}
