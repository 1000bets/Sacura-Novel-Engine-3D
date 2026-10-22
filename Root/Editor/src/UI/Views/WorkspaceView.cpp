#include "UI/Views/WorkspaceView.h"
#include "UI/Adapters/WorkspaceLayoutAdapter.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QTimer>

namespace EditorUi
{
namespace
{
QVBoxLayout* MakeRegionLayout(QWidget& Region)
{
    auto* Layout = new QVBoxLayout(&Region);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    return Layout;
}
}

WorkspaceView::WorkspaceView(QWidget* Parent) : QWidget(Parent)
{
    setObjectName("EditorRoot");
    auto* RootLayout = MakeRegionLayout(*this);
    Workspace = new QSplitter(Qt::Horizontal, this);
    Workspace->setHandleWidth(6);

    HierarchyPanel = new QWidget(Workspace);
    HierarchyPanel->setObjectName("HierarchyPanel");
    HierarchyPanel->setMinimumWidth(180);
    HierarchyLayout = MakeRegionLayout(*HierarchyPanel);

    auto* CenterPanel = new QWidget(Workspace);
    auto* CenterLayout = MakeRegionLayout(*CenterPanel);
    Center = new QSplitter(Qt::Vertical, CenterPanel);
    Center->setHandleWidth(6);
    ViewportPanel = new QWidget(Center);
    ViewportPanel->setObjectName("ViewportPanel");
    ViewportLayout = MakeRegionLayout(*ViewportPanel);
    ToolsPanel = new QWidget(Center);
    ToolsPanel->setObjectName("GraphDock");
    ToolsLayout = MakeRegionLayout(*ToolsPanel);
    Center->setStretchFactor(0, 3);
    Center->setStretchFactor(1, 2);
    CenterLayout->addWidget(Center);

    InspectorPanel = new QWidget(Workspace);
    InspectorPanel->setObjectName("InspectorPanel");
    InspectorPanel->setMinimumWidth(240);
    InspectorLayout = MakeRegionLayout(*InspectorPanel);

    WorkspaceLayoutAdapter::Arrange(*Workspace, *HierarchyPanel, *CenterPanel, *InspectorPanel);
    RootLayout->addWidget(Workspace);
}

void WorkspaceView::showEvent(QShowEvent* Event)
{
    QWidget::showEvent(Event);
    if (!bInitialLayoutApplied)
    {
        QTimer::singleShot(0, this, [this]()
        {
            if (!property("layoutRestored").toBool())
            { WorkspaceLayoutAdapter::Reset(*Workspace, *Center, window()->height()); }
        });
        bInitialLayoutApplied = true;
    }
}
}
