#pragma once

#include "Actions/EditorAction.h"
#include "World/Components/PhysicsBodyComponent.h"
#include "World/Components/NavMeshBoundsComponent.h"
#include "World/Components/NavigationModifierComponent.h"
#include "World/Components/NavigationLinkComponent.h"
#include "World/Components/NavigationAgentComponent.h"

class CreatePhysicsBodyObjectAction : public EditorAction
{
    ENGINE_CLASS(CreatePhysicsBodyObjectAction, EditorAction, "editor.CreatePhysicsBodyObjectAction")
public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Physics Body"; }
    int GetSortOrder() const override { return 120; }
    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(Context, "PhysicsBody", TypeId{PhysicsBodyComponent::StaticReflectionTypeId()});
    }
protected:
    CreatePhysicsBodyObjectAction() = default;
};
ENGINE_CLASS_END(CreatePhysicsBodyObjectAction)

class CreateCharacterObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateCharacterObjectAction, EditorAction, "editor.CreateCharacterObjectAction")
public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Character Controller"; }
    int GetSortOrder() const override { return 125; }
    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(Context, "Character", TypeId{CharacterControllerComponent::StaticReflectionTypeId()});
    }
protected:
    CreateCharacterObjectAction() = default;
};
ENGINE_CLASS_END(CreateCharacterObjectAction)

class CreateNavMeshBoundsObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateNavMeshBoundsObjectAction, EditorAction, "editor.CreateNavMeshBoundsObjectAction")
public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "NavMesh Bounds Volume"; }
    int GetSortOrder() const override { return 130; }
    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(Context, "NavMeshBounds", TypeId{NavMeshBoundsComponent::StaticReflectionTypeId()});
    }
protected:
    CreateNavMeshBoundsObjectAction() = default;
};
ENGINE_CLASS_END(CreateNavMeshBoundsObjectAction)

class CreateNavigationModifierObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateNavigationModifierObjectAction, EditorAction, "editor.CreateNavigationModifierObjectAction")
public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Navigation Modifier Volume"; }
    int GetSortOrder() const override { return 140; }
    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(Context, "NavigationModifier", TypeId{NavigationModifierComponent::StaticReflectionTypeId()});
    }
protected:
    CreateNavigationModifierObjectAction() = default;
};
ENGINE_CLASS_END(CreateNavigationModifierObjectAction)

class CreateNavigationLinkObjectAction : public EditorAction
{
    ENGINE_CLASS(CreateNavigationLinkObjectAction, EditorAction, "editor.CreateNavigationLinkObjectAction")
public:
    const char* GetMenuCategory() const override { return ObjectsCategory; }
    const char* GetDisplayName() const override { return "Navigation Link"; }
    int GetSortOrder() const override { return 150; }
    bool Execute(EditorActionContext& Context) const override
    {
        return CreateObjectWithComponent(Context, "NavigationLink", TypeId{NavigationLinkComponent::StaticReflectionTypeId()});
    }
protected:
    CreateNavigationLinkObjectAction() = default;
};
ENGINE_CLASS_END(CreateNavigationLinkObjectAction)

class AddPhysicsBodyComponentAction : public EditorAction
{
    ENGINE_CLASS(AddPhysicsBodyComponentAction, EditorAction, "editor.AddPhysicsBodyComponentAction")
public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Physics Body Component"; }
    int GetSortOrder() const override { return 30; }
    bool RequiresSelection() const override { return true; }
    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{PhysicsBodyComponent::StaticReflectionTypeId()});
    }
protected:
    AddPhysicsBodyComponentAction() = default;
};
ENGINE_CLASS_END(AddPhysicsBodyComponentAction)

class AddCharacterControllerComponentAction : public EditorAction
{
    ENGINE_CLASS(AddCharacterControllerComponentAction, EditorAction, "editor.AddCharacterControllerComponentAction")
public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Character Controller Component"; }
    int GetSortOrder() const override { return 35; }
    bool RequiresSelection() const override { return true; }
    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{CharacterControllerComponent::StaticReflectionTypeId()});
    }
protected:
    AddCharacterControllerComponentAction() = default;
};
ENGINE_CLASS_END(AddCharacterControllerComponentAction)

class AddNavMeshBoundsComponentAction : public EditorAction
{
    ENGINE_CLASS(AddNavMeshBoundsComponentAction, EditorAction, "editor.AddNavMeshBoundsComponentAction")
public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "NavMesh Bounds Component"; }
    int GetSortOrder() const override { return 40; }
    bool RequiresSelection() const override { return true; }
    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{NavMeshBoundsComponent::StaticReflectionTypeId()});
    }
protected:
    AddNavMeshBoundsComponentAction() = default;
};
ENGINE_CLASS_END(AddNavMeshBoundsComponentAction)

class AddNavigationModifierComponentAction : public EditorAction
{
    ENGINE_CLASS(AddNavigationModifierComponentAction, EditorAction, "editor.AddNavigationModifierComponentAction")
public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Navigation Modifier Component"; }
    int GetSortOrder() const override { return 45; }
    bool RequiresSelection() const override { return true; }
    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{NavigationModifierComponent::StaticReflectionTypeId()});
    }
protected:
    AddNavigationModifierComponentAction() = default;
};
ENGINE_CLASS_END(AddNavigationModifierComponentAction)

class AddNavigationLinkComponentAction : public EditorAction
{
    ENGINE_CLASS(AddNavigationLinkComponentAction, EditorAction, "editor.AddNavigationLinkComponentAction")
public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Navigation Link Component"; }
    int GetSortOrder() const override { return 50; }
    bool RequiresSelection() const override { return true; }
    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{NavigationLinkComponent::StaticReflectionTypeId()});
    }
protected:
    AddNavigationLinkComponentAction() = default;
};
ENGINE_CLASS_END(AddNavigationLinkComponentAction)

class AddNavigationAgentComponentAction : public EditorAction
{
    ENGINE_CLASS(AddNavigationAgentComponentAction, EditorAction, "editor.AddNavigationAgentComponentAction")
public:
    const char* GetMenuCategory() const override { return ComponentsCategory; }
    const char* GetDisplayName() const override { return "Navigation Agent Component"; }
    int GetSortOrder() const override { return 55; }
    bool RequiresSelection() const override { return true; }
    bool Execute(EditorActionContext& Context) const override
    {
        return AddComponentToSelection(Context, TypeId{NavigationAgentComponent::StaticReflectionTypeId()});
    }
protected:
    AddNavigationAgentComponentAction() = default;
};
ENGINE_CLASS_END(AddNavigationAgentComponentAction)


