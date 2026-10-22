#include "UI/Views/WorkspaceView.h"
#include "UI/Adapters/WorkspaceLayoutAdapter.h"

#include <QSplitter>
#include <QVBoxLayout>
#include <QShowEvent>
#include <QResizeEvent>
#include <QCoreApplication>
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

    // Native viewport geometry changes immediately during a drag. Flush the
    // surrounding Qt panels at the same boundary, after all splitter children
    // have moved, so their old tab/header pixels cannot trail behind the GPU
    // surface while the next event-loop repaint is pending.
    // Deliver only pending paint requests: do not pump input or gameplay timers,
    // and let Qt keep its dirty-region clipping instead of repainting everything.
    for (QSplitter* Splitter : {Workspace, Center})
    {
        connect(Splitter, &QSplitter::splitterMoved, Splitter, [Splitter]()
        {
            QCoreApplication::sendPostedEvents(Splitter->window(), QEvent::UpdateRequest);
        });
    }
}

void WorkspaceView::resizeEvent(QResizeEvent* Event)
{
    QWidget::resizeEvent(Event);
    // QWidget has already activated this widget's layout before resizeEvent.
    // Cover outer-window resizing as well as dragging an individual divider.
    QCoreApplication::sendPostedEvents(window(), QEvent::UpdateRequest);
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
