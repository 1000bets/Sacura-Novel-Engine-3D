#include "UI/Presentation/WorkspaceLayout.h"

#include <algorithm>
#include <cmath>

namespace EditorUi
{
WorkspaceLayout WorkspaceLayout::Initial(int WindowHeight)
{
    WorkspaceLayout Layout;
    Layout.ViewportHeight = std::max(260, static_cast<int>(std::round(WindowHeight * 0.54)));
    Layout.ToolsHeight = std::max(160, WindowHeight - Layout.ViewportHeight);
    return Layout;
}
}
