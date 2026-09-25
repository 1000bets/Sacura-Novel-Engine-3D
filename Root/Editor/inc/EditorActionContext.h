#pragma once

#include "EditorCommandStack.h"
#include "Gameplay/ObjectHandle.h"

#include <functional>
#include <memory>

class Scene;

struct EditorActionContext
{
    Scene* EditScene = nullptr;
    EditorCommandStack* CommandStack = nullptr;
    ObjectHandle SelectedObject{};
    std::function<bool(std::unique_ptr<EditorCommand>)> ExecuteCommand;
    std::function<void(ObjectHandle)> SelectObject;
};
