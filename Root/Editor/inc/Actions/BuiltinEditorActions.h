#pragma once

#include "EditorAction.h"
#include "Gameplay/CameraComponent.h"
#include "Gameplay/LightComponent.h"
#include "Gameplay/MeshRendererComponent.h"
#include "Rendering/RenderLight.h"

#include "EditorCommands.h"

class CreateEmptyObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateEmptyObjectAction, EditorAction, "editor.CreateEmptyObjectAction", 1)

public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Empty Object"; }
    int GetSortOrder() const override { return 0; }

    bool Execute(EditorActionContext& Context) const override
    {
        if (!CanExecute(Context))
        {
            return false;
        }
        ObjectHandle Created;
        if (!Context.ExecuteCommand(MakeCreateObjectCommand(
                Context.EditScene,
                "GameObject",
                Context.SelectedObject,
                &Created)))
        {
            return false;
        }
        if (Context.SelectObject)
        {
            Context.SelectObject(Created);
        }
        return true;
    }

protected:
    CreateEmptyObjectAction() = default;
};

ENGINE_CLASS_END(CreateEmptyObjectAction)

class CreateCameraObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateCameraObjectAction, EditorAction, "editor.CreateCameraObjectAction", 1)

public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Camera Object"; }
    int GetSortOrder() const override { return 10; }

    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(
            Context,
            "Camera",
            TypeId{CameraComponent::StaticReflectionTypeId()});
    }

protected:
    CreateCameraObjectAction() = default;
};

ENGINE_CLASS_END(CreateCameraObjectAction)

class CreateStaticMeshObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateStaticMeshObjectAction, EditorAction, "editor.CreateStaticMeshObjectAction", 1)

public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Static Mesh Object"; }
    int GetSortOrder() const override { return 20; }

    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(
            Context,
            "StaticMesh",
            TypeId{MeshRendererComponent::StaticReflectionTypeId()});
    }

protected:
    CreateStaticMeshObjectAction() = default;
};

ENGINE_CLASS_END(CreateStaticMeshObjectAction)

class CreateDirectionalLightObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateDirectionalLightObjectAction, EditorAction, "editor.CreateDirectionalLightObjectAction", 1)

public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Directional Light Object"; }
    int GetSortOrder() const override { return 100; }

    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(
            Context,
            "DirectionalLight",
            TypeId{LightComponent::StaticReflectionTypeId()},
            true,
            static_cast<int64_t>(RenderLightType::Directional));
    }

protected:
    CreateDirectionalLightObjectAction() = default;
};

ENGINE_CLASS_END(CreateDirectionalLightObjectAction)

class CreatePointLightObjectAction : public EditorAction
{
    ENGINE_CLASS(CreatePointLightObjectAction, EditorAction, "editor.CreatePointLightObjectAction", 1)

public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Point Light Object"; }
    int GetSortOrder() const override { return 110; }

    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(
            Context,
            "PointLight",
            TypeId{LightComponent::StaticReflectionTypeId()},
            true,
            static_cast<int64_t>(RenderLightType::Point));
    }

protected:
    CreatePointLightObjectAction() = default;
};

ENGINE_CLASS_END(CreatePointLightObjectAction)

class CreateSpotLightObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateSpotLightObjectAction, EditorAction, "editor.CreateSpotLightObjectAction", 1)

public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Spot Light Object"; }
    int GetSortOrder() const override { return 120; }

    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(
            Context,
            "SpotLight",
            TypeId{LightComponent::StaticReflectionTypeId()},
            true,
            static_cast<int64_t>(RenderLightType::Spot));
    }

protected:
    CreateSpotLightObjectAction() = default;
};

ENGINE_CLASS_END(CreateSpotLightObjectAction)

class AddCameraComponentAction : public EditorAction
{
    ENGINE_CLASS(AddCameraComponentAction, EditorAction, "editor.AddCameraComponentAction", 1)

public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Camera Component"; }
    int GetSortOrder() const override { return 0; }
    bool RequiresSelection() const override { return true; }

    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{CameraComponent::StaticReflectionTypeId()});
    }

protected:
    AddCameraComponentAction() = default;
};

ENGINE_CLASS_END(AddCameraComponentAction)

class AddLightComponentAction : public EditorAction
{
    ENGINE_CLASS(AddLightComponentAction, EditorAction, "editor.AddLightComponentAction", 1)

public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Light Component"; }
    int GetSortOrder() const override { return 10; }
    bool RequiresSelection() const override { return true; }

    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{LightComponent::StaticReflectionTypeId()});
    }

protected:
    AddLightComponentAction() = default;
};

ENGINE_CLASS_END(AddLightComponentAction)

class AddMeshRendererComponentAction : public EditorAction
{
    ENGINE_CLASS(AddMeshRendererComponentAction, EditorAction, "editor.AddMeshRendererComponentAction", 1)

public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Mesh Renderer Component"; }
    int GetSortOrder() const override { return 20; }
    bool RequiresSelection() const override { return true; }

    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{MeshRendererComponent::StaticReflectionTypeId()});
    }

protected:
    AddMeshRendererComponentAction() = default;
};

ENGINE_CLASS_END(AddMeshRendererComponentAction)
