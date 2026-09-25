#include "EditorAction.h"

#include "EditorCommands.h"
#include "Game/Scene.h"
#include "Gameplay/GameObject.h"
#include "Reflection/ReflectedValue.h"
#include "Reflection/ReflectionSubsystem.h"

#include <algorithm>
#include <cstring>

const char* EditorAction::GetMenuCategory() const
{
    return ObjectsCategory;
}

const char* EditorAction::GetDisplayName() const
{
    return "Editor Action";
}

int EditorAction::GetSortOrder() const
{
    return 0;
}

bool EditorAction::RequiresSelection() const
{
    return false;
}

bool EditorAction::CanExecute(const EditorActionContext& Context) const
{
    if (Context.EditScene == nullptr || !Context.ExecuteCommand)
    {
        return false;
    }
    if (!RequiresSelection())
    {
        return true;
    }
    if (!Context.SelectedObject.IsValid())
    {
        return false;
    }
    return Context.EditScene->FindByHandle(Context.SelectedObject) != nullptr;
}

bool EditorAction::Execute(EditorActionContext& Context) const
{
    return false;
}

std::vector<EditorAction*> EditorAction::CollectPublishedActions()
{
    std::vector<EditorAction*> Result;
    if (!ReflectionSubsystem::Get().IsInitialized())
    {
        return Result;
    }

    const TypeId ActionBaseType{EditorAction::StaticReflectionTypeId()};
    for (Class* Entry : ReflectionSubsystem::Get().GetPublishedCatalog().Classes)
    {
        if (Entry == nullptr || !Entry->IsConcrete() || !Entry->IsA(ActionBaseType))
        {
            continue;
        }
        Object* DefaultObject = Entry->GetClassDefaultObject();
        if (DefaultObject == nullptr)
        {
            continue;
        }
        Result.push_back(static_cast<EditorAction*>(DefaultObject));
    }

    std::sort(Result.begin(), Result.end(), [](const EditorAction* Left, const EditorAction* Right)
    {
        const int CategoryCompare = std::strcmp(Left->GetMenuCategory(), Right->GetMenuCategory());
        if (CategoryCompare != 0)
        {
            return CategoryCompare < 0;
        }
        if (Left->GetSortOrder() != Right->GetSortOrder())
        {
            return Left->GetSortOrder() < Right->GetSortOrder();
        }
        return std::strcmp(Left->GetDisplayName(), Right->GetDisplayName()) < 0;
    });
    return Result;
}

bool EditorAction::CreateObjectWithComponent(
    EditorActionContext& Context,
    const std::string& ObjectName,
    const TypeId& ComponentType,
    bool bConfigureLightType,
    int64_t LightTypeValue) const
{
    if (!CanExecute(Context) || Context.CommandStack == nullptr)
    {
        return false;
    }

    ObjectHandle CreatedObject;
    ObjectHandle CreatedComponent;
    Context.CommandStack->BeginGroup(ObjectName);
    bool bCreated = Context.ExecuteCommand(MakeCreateObjectCommand(
        Context.EditScene,
        ObjectName,
        Context.SelectedObject,
        &CreatedObject));
    if (bCreated)
    {
        bCreated = Context.ExecuteCommand(MakeAddComponentCommand(
            Context.EditScene,
            CreatedObject,
            ComponentType,
            &CreatedComponent));
    }
    if (bCreated && bConfigureLightType)
    {
        bCreated = Context.ExecuteCommand(MakeSetPropertyCommand(
            Context.EditScene,
            CreatedComponent,
            PropertyId{"light_type"},
            ReflectedValue::MakeInt64(LightTypeValue)));
    }
    Context.CommandStack->EndGroup();
    if (bCreated && Context.SelectObject)
    {
        Context.SelectObject(CreatedObject);
    }
    return bCreated;
}

bool EditorAction::AddComponentToSelection(EditorActionContext& Context, const TypeId& ComponentType) const
{
    if (!CanExecute(Context))
    {
        return false;
    }
    return Context.ExecuteCommand(MakeAddComponentCommand(
        Context.EditScene,
        Context.SelectedObject,
        ComponentType));
}
