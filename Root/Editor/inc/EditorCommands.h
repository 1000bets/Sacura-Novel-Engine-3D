#pragma once

#include "EditorCommandStack.h"
#include "Core/Transform.h"

#include <string>

class GameObject;
class Scene;

std::unique_ptr<EditorCommand> MakeRenameObjectCommand(Scene* TargetScene, ObjectHandle Target, std::string NewName);
std::unique_ptr<EditorCommand> MakeSetTransformCommand(Scene* TargetScene, ObjectHandle Target, Transform NewTransform);
std::unique_ptr<EditorCommand> MakeCreateObjectCommand(
    Scene* TargetScene,
    std::string Name,
    ObjectHandle Parent,
    ObjectHandle* OutCreated = nullptr);
std::unique_ptr<EditorCommand> MakePasteSubtreeCommand(
    Scene* TargetScene,
    std::string SerializedSubtree,
    ObjectHandle Parent,
    ObjectHandle* OutCreated = nullptr);
std::unique_ptr<EditorCommand> MakeDeleteObjectCommand(Scene* TargetScene, ObjectHandle Target);
std::unique_ptr<EditorCommand> MakeReparentObjectCommand(Scene* TargetScene, ObjectHandle Target, ObjectHandle NewParent);
std::unique_ptr<EditorCommand> MakeAddComponentCommand(
    Scene* TargetScene,
    ObjectHandle Owner,
    TypeId ComponentType,
    ObjectHandle* OutCreatedComponent = nullptr);
std::unique_ptr<EditorCommand> MakeRemoveComponentCommand(Scene* TargetScene, ObjectHandle Owner, ObjectHandle Component);
std::unique_ptr<EditorCommand> MakeSetPropertyCommand(
    Scene* TargetScene,
    ObjectHandle Target,
    PropertyId Property,
    ReflectedValue NewValue);
std::unique_ptr<EditorCommand> MakeResetPropertyCommand(
    Scene* TargetScene,
    ObjectHandle Target,
    PropertyId Property);
