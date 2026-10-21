#pragma once

namespace EditorUi
{
struct WorkspaceLayout
{
    int HierarchyWidth = 210;
    int InspectorWidth = 280;
    int ViewportHeight = 486;
    int ToolsHeight = 280;

    static WorkspaceLayout Initial(int WindowHeight);
};
}
