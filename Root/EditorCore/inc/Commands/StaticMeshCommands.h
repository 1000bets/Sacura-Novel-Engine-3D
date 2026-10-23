#pragma once
#include "Assets/AssetRegistry.h"
#include "Commands/EditorCommands.h"

std::unique_ptr<EditorCommand> MakeCreateStaticMeshObjectCommand(Scene* World, const AssetRegistry& Registry,
    const AssetKey& Asset, const std::string& Name, ObjectHandle Parent, const Transform& InitialTransform,
    ObjectHandle* Created = nullptr);
