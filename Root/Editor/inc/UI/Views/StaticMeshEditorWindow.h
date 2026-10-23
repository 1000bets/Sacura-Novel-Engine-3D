#pragma once
#include "Documents/StaticMeshDocument.h"
#include "Rendering/RenderView.h"
#include <QMainWindow>
#include "Core/Transform.h"
#include "Physics/PhysicsTypes.h"
#include <optional>

class Engine;
class Scene;
class MeshRendererComponent;
class PhysicsBodyComponent;
class EditorViewportWidget;
class QFormLayout;
class QLabel;
class QListWidget;
class QTabWidget;

class StaticMeshEditorWindow : public QMainWindow
{
public:
    StaticMeshEditorWindow(Engine& Runtime, const AssetKey& Asset, QWidget* Parent);
    ~StaticMeshEditorWindow() override;
    StaticMeshDocument& GetDocument() { return Document; }
    bool Run(const QString& Operation, const nlohmann::json& Value, std::string& Error);
protected:
    void closeEvent(QCloseEvent* Event) override;
private:
    void RebuildFields();
    void Change(const nlohmann::json& Value);
    void TickPreview();
    void SyncSelection();
    void PreviewTransform(const Transform& Value);
    void CommitTransform();
    void CancelTransform();
    void RebuildCollisionPreview();
    void PickCollision(QPoint Position);
    void FrameSelection(bool bMesh);
    const StaticMeshDefaults& GetPreviewDefaults() const;
    Engine& Runtime;
    StaticMeshDocument Document;
    Scene* PreviewScene = nullptr;
    MeshRendererComponent* PreviewMesh = nullptr;
    PhysicsBodyComponent* PreviewCollision = nullptr;
    EditorViewportWidget* Viewport = nullptr;
    QFormLayout* Fields = nullptr;
    QLabel* Diagnostic = nullptr;
    QListWidget* Elements = nullptr;
    QFormLayout* MaterialFields = nullptr;
    QTabWidget* Tabs = nullptr;
    std::optional<StaticMeshDefaults> TransformDraft;
    std::vector<PhysicsTriangleMesh> CollisionGeometry;
    nlohmann::json DisplayedCollision;
    size_t DisplayedSelection = static_cast<size_t>(-1);
    RenderViewId ViewId;
    bool bFramed = false;
    size_t SelectedCollisionElement = 0;
};
