#pragma once

#include <QWidget>

class QSplitter;
class QVBoxLayout;
class QShowEvent;

namespace EditorUi
{
// Owns the shell's visual regions. Controllers supply their content and commands.
class WorkspaceView final : public QWidget
{
public:
    explicit WorkspaceView(QWidget* Parent = nullptr);

    QSplitter& GetWorkspaceSplitter() const { return *Workspace; }
    QSplitter& GetCenterSplitter() const { return *Center; }
    QWidget& GetHierarchyPanel() const { return *HierarchyPanel; }
    QVBoxLayout& GetHierarchyLayout() const { return *HierarchyLayout; }
    QWidget& GetViewportPanel() const { return *ViewportPanel; }
    QVBoxLayout& GetViewportLayout() const { return *ViewportLayout; }
    QWidget& GetToolsPanel() const { return *ToolsPanel; }
    QVBoxLayout& GetToolsLayout() const { return *ToolsLayout; }
    QWidget& GetInspectorPanel() const { return *InspectorPanel; }
    QVBoxLayout& GetInspectorLayout() const { return *InspectorLayout; }

protected:
    void showEvent(QShowEvent* Event) override;

private:
    QSplitter* Workspace = nullptr;
    QSplitter* Center = nullptr;
    QWidget* HierarchyPanel = nullptr;
    QVBoxLayout* HierarchyLayout = nullptr;
    QWidget* ViewportPanel = nullptr;
    QVBoxLayout* ViewportLayout = nullptr;
    QWidget* ToolsPanel = nullptr;
    QVBoxLayout* ToolsLayout = nullptr;
    QWidget* InspectorPanel = nullptr;
    QVBoxLayout* InspectorLayout = nullptr;
    bool bInitialLayoutApplied = false;
};
}
