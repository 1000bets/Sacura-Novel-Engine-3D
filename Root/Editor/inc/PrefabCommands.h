#pragma once

#include "EditorCommandStack.h"
#include "Assets/Guid.h"
#include "Core/Transform.h"

#include <functional>
#include <memory>
#include <string>

class GameObject;
class Scene;

// Undoable editor commands for prefab operations (EditorCore, no Qt dependency).

// Instantiates a prefab asset (outermost instance). Undo removes it; redo restores the exact
// same instance (same record id => same object ids) from a captured subtree document.
std::unique_ptr<EditorCommand> MakeInstantiatePrefabCommand(
    Scene* TargetScene,
    AssetId Prefab,
    ObjectHandle Parent,
    const Transform* InitialTransform = nullptr,
    ObjectHandle* OutCreated = nullptr);

// Generic snapshot command: captures the subtree of Root as a document before and after an
// arbitrary prefab mutation (revert, unpack, create/connect prefab...). Undo/redo swap the
// snapshots; persistent ids are stable, so the subtree is located by id rather than by handle.
using PrefabSubtreeMutation = std::function<bool(Scene& TargetScene, GameObject& Root, std::string& OutError)>;
std::unique_ptr<EditorCommand> MakePrefabSubtreeEditCommand(
    Scene* TargetScene,
    ObjectHandle Root,
    std::string DisplayName,
    PrefabSubtreeMutation Mutation,
    std::string* OutError = nullptr,
    ObjectHandle* OutRoot = nullptr);
