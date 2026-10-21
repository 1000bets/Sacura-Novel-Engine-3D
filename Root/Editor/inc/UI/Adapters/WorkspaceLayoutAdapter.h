#pragma once

class QSplitter;
class QWidget;

namespace EditorUi
{
class WorkspaceLayoutAdapter
{
public:
    static void Arrange(QSplitter& Workspace, QWidget& Hierarchy, QWidget& Center, QWidget& Inspector);
    static void Reset(QSplitter& Workspace, QSplitter& Center, int WindowHeight);
};
}
