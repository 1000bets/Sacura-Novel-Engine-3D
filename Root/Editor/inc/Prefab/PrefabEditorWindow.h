#pragma once

#include "Assets/Guid.h"
#include "Commands/EditorCommandStack.h"
#include "Core/Object/ObjectHandle.h"
#include "Prefab/PrefabTypes.h"
#include "Prefab/PrefabUtility.h"
#include "Rendering/RenderSettings.h"
#include "Rendering/RenderView.h"

#include <QMainWindow>
#include <QString>

#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

class Engine;
class EditorAction;
class EditorViewportWidget;
class GameObject;
class PrefabOverridesPanel;
class QCloseEvent;
class QDoubleSpinBox;
class QEvent;
class QLabel;
class QLineEdit;
class QMenu;
class QPushButton;
class QTimer;
class QTreeWidget;
class QTreeWidgetItem;
class QVBoxLayout;
class ReflectionInspector;
class Scene;
struct EditorActionContext;

// Unreal-style asset editor for one prefab (Unity "Prefab Mode" in a separate window):
//  * an isolated preview Scene built from the prefab document (not the Engine's main world);
//  * its own native viewport (separate render surface), Hierarchy, Inspector, selection and
//    EditorCommandStack (Undo/Redo), so the main scene keeps its state and history;
//  * nested prefabs and a variant's base are live instances inside the preview scene with their
//    own override panel (Revert / Apply to the inner prefab / Unpack);
//  * Save writes the asset and notifies PrefabLibrary, which propagates the change to the main
//    scene and to other open prefab editors. Several windows can be open at once.
class PrefabEditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    PrefabEditorWindow(Engine& InEngine, const AssetId& InPrefab, QWidget* Parent = nullptr);
    ~PrefabEditorWindow() override;

    bool LoadPrefab(std::string& OutError);
    const AssetId& GetPrefab() const { return Prefab; }
    bool IsDirty() const { return bDirty; }
    Scene* GetPreviewScene() const { return PreviewScene; }

signals:
    void AssetFocusRequested(QString AssetIdentifier, QString SubAssetIdentifier);
    void OpenPrefabRequested(QString AssetId);
    void PrefabSaved(QString AssetId);

protected:
    void closeEvent(QCloseEvent* Event) override;
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private:
    void BuildUi();
    void BuildToolbar();
    void PopulateHierarchy();
    void AddHierarchyItem(QTreeWidgetItem* ParentItem, GameObject* Object);
    void PopulateCreateMenus(QMenu* ObjectsMenu, QMenu* ComponentsMenu);
    EditorActionContext MakeActionContext();

    void SelectObject(ObjectHandle Target);
    GameObject* GetSelectedObject() const;
    GameObject* GetPrefabRoot() const;
    void RefreshSelectionUi();
    void RebuildComponentInspectors();
    void UpdateTitle();

    bool Execute(std::unique_ptr<EditorCommand> Command);
    void AfterSceneChanged();
    bool ReparentObject(ObjectHandle Target, ObjectHandle NewParent);
    bool Save();
    bool SaveAs();
    bool PromptSaveIfDirty();
    void Undo();
    void Redo();
    void DeleteSelected();
    void CopySelected();
    void PasteClipboard();
    void OnNameEdited();
    void OnTransformEdited();
    void OnTick();
    void ConfigureRenderView();
    void OnViewportSelection(QPoint Position);
    void OnAssetDropped(QString AssetIdText, QString SubAssetIdentifier, QString AssetTypeIdentifier, QString VirtualPath);
    void OnHierarchyContextMenu(const QPoint& Position);
    void OnPrefabChanged(const AssetId& Changed);
    bool HandleGizmoHotkey(QEvent* Event);

    bool RevertInstance(ObjectHandle Root, const std::vector<PrefabOverride>& Selection);
    bool ApplyInstance(ObjectHandle Root, const std::vector<PrefabOverride>& Selection);
    bool UnpackInstance(ObjectHandle Root, PrefabUnpackMode Mode);

    Engine& BoundEngine;
    AssetId Prefab{};
    Scene* PreviewScene = nullptr;
    EditorCommandStack CommandStack;
    ObjectHandle Selected;
    RenderViewId ViewId{};
    RenderSettings RenderConfiguration;
    uint64_t LibraryListener = 0;
    bool bDirty = false;
    bool bSaving = false;
    bool bUpdatingUi = false;
    std::unordered_set<std::string> OverrideMarkers;

    QTimer* TickTimer = nullptr;
    QLabel* BreadcrumbLabel = nullptr;
    QTreeWidget* HierarchyTree = nullptr;
    EditorViewportWidget* Viewport = nullptr;
    PrefabOverridesPanel* PrefabPanel = nullptr;
    QLineEdit* NameEdit = nullptr;
    QDoubleSpinBox* PositionSpins[3] = {};
    QDoubleSpinBox* RotationSpins[3] = {};
    QDoubleSpinBox* ScaleSpins[3] = {};
    QPushButton* AddComponentButton = nullptr;
    QVBoxLayout* ComponentLayout = nullptr;
    std::vector<ReflectionInspector*> ComponentInspectors;
};
