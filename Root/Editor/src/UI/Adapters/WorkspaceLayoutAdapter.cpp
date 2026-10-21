#include "UI/Adapters/WorkspaceLayoutAdapter.h"
#include "UI/Presentation/WorkspaceLayout.h"

#include <QSplitter>
#include <algorithm>

namespace EditorUi
{
void WorkspaceLayoutAdapter::Arrange(QSplitter& Workspace, QWidget& Hierarchy, QWidget& Center, QWidget& Inspector)
{
    Workspace.insertWidget(0, &Hierarchy);
    Workspace.insertWidget(1, &Center);
    Workspace.insertWidget(2, &Inspector);
    Workspace.setStretchFactor(0, 0);
    Workspace.setStretchFactor(1, 1);
    Workspace.setStretchFactor(2, 0);
}

void WorkspaceLayoutAdapter::Reset(QSplitter& Workspace, QSplitter& Center, int WindowHeight)
{
    const WorkspaceLayout Layout = WorkspaceLayout::Initial(WindowHeight);
    const int HandlesWidth = Workspace.handleWidth() * std::max(0, Workspace.count() - 1);
    Workspace.setSizes({Layout.HierarchyWidth, std::max(320, Workspace.width() - Layout.HierarchyWidth - Layout.InspectorWidth - HandlesWidth), Layout.InspectorWidth});
    Center.setSizes({Layout.ViewportHeight, Layout.ToolsHeight});
}
}
