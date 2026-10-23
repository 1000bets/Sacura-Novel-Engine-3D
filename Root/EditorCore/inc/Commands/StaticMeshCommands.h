#pragma once
#include "Assets/AssetRegistry.h"
#include "Commands/EditorCommands.h"

std::unique_ptr<EditorCommand> MakeCreateStaticMeshObjectCommand(Scene* World, const AssetRegistry& Registry,
    const AssetKey& Asset, const std::string& Name, ObjectHandle Parent, const Transform& InitialTransform,
    ObjectHandle* Created = nullptr);

std::unique_ptr<EditorCommand> MakeAssignMeshMaterialCommand(Scene* World, const AssetRegistry& Registry,
    ObjectHandle Mesh, int32_t Slot, const AssetKey& Material, std::string& Error);
std::unique_ptr<EditorCommand> MakeSetMeshPhysicsCommand(Scene* World, const AssetRegistry& Registry,
    ObjectHandle Mesh, bool bEnabled, bool bSimulate, std::string& Error);
