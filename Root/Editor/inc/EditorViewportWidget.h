#pragma once

#include "Core/Transform.h"
#include "UI/RenderViewportWidget.h"

#include <functional>

class EditorGizmoOverlay;

enum class EditorGizmoOperation
{
    Translate,
    Rotate,
    Scale
};

class EditorViewportWidget : public RenderViewportWidget
{
    Q_OBJECT

public:
    using TransformPreviewCallback = std::function<void(const Transform& Value)>;
    using TransformCommitCallback = std::function<void(const Transform& OldValue, const Transform& NewValue)>;

    explicit EditorViewportWidget(QWidget* Parent = nullptr);

    void SetSelectedTransform(const Transform& LocalTransform, const Transform& WorldTransform);
    void ClearSelectedTransform();
    void SetTransformCallbacks(TransformPreviewCallback Preview, TransformCommitCallback Commit);
    void SetGizmoOperation(EditorGizmoOperation Operation);
    void SetEditorToolsEnabled(bool bEnabled);
    void SyncGizmoOverlay();

signals:
    void AssetDropped(QString AssetId, QString SubAssetId, int AssetTypeValue, QString VirtualPath, QPoint Position);

protected:
    void resizeEvent(QResizeEvent* Event) override;
    void showEvent(QShowEvent* Event) override;
    void hideEvent(QHideEvent* Event) override;

private:
    friend class EditorGizmoOverlay;

    EditorGizmoOverlay* Gizmo = nullptr;
    TransformPreviewCallback PreviewCallback;
    TransformCommitCallback CommitCallback;
    bool bEditorToolsEnabled = false;
};
