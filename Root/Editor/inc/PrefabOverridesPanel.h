#pragma once

#include "Assets/Guid.h"
#include "Gameplay/ObjectHandle.h"
#include "Prefab/PrefabTypes.h"
#include "Prefab/PrefabUtility.h"

#include <QWidget>

#include <functional>
#include <vector>

class GameObject;
class QLabel;
class QPushButton;
class QTreeWidgetItem;
class Scene;

// Inspector header shown for objects that belong to a prefab instance (Unity "Prefab" bar):
// prefab name / relation, Open, Overrides (N) dialog with per-override Revert/Apply,
// Revert All, Apply All and Unpack. The host window performs the operations through callbacks so
// that it can wrap them into undoable commands and refresh its own UI.
class PrefabOverridesPanel : public QWidget
{
    Q_OBJECT

public:
    struct Callbacks
    {
        std::function<void(const AssetId& Prefab)> OpenPrefab;
        // Empty selection == all overrides.
        std::function<bool(ObjectHandle InstanceRoot, const std::vector<PrefabOverride>& Selection)> Revert;
        std::function<bool(ObjectHandle InstanceRoot, const std::vector<PrefabOverride>& Selection)> Apply;
        std::function<bool(ObjectHandle InstanceRoot, PrefabUnpackMode Mode)> Unpack;
        std::function<void(ObjectHandle Object)> Select;
    };

    explicit PrefabOverridesPanel(QWidget* Parent = nullptr);

    // Unity-style Hierarchy colors: instance roots blue/bold, prefab-owned objects light blue,
    // nested prefab roots italic, added objects green, missing prefabs red.
    static void DecorateHierarchyItem(QTreeWidgetItem* Item, GameObject* Object);

    void SetCallbacks(Callbacks InCallbacks);
    // Hides itself when Selected is not related to a prefab instance.
    void SetTarget(Scene* InScene, GameObject* Selected, bool bEditable);
    void Refresh();

private:
    void ShowOverridesDialog();
    GameObject* ResolveRoot() const;

    Callbacks Handlers;
    Scene* TargetScene = nullptr;
    ObjectHandle Selection;
    ObjectHandle InstanceRoot;
    AssetId Prefab{};
    AssetId NestedPrefab{};
    bool bEditable = true;

    QLabel* TitleLabel = nullptr;
    QLabel* DetailLabel = nullptr;
    QPushButton* OpenButton = nullptr;
    QPushButton* OpenNestedButton = nullptr;
    QPushButton* SelectRootButton = nullptr;
    QPushButton* OverridesButton = nullptr;
    QPushButton* RevertAllButton = nullptr;
    QPushButton* ApplyAllButton = nullptr;
    QPushButton* UnpackButton = nullptr;
};
