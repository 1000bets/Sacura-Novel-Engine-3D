#pragma once

#include "EditorActionContext.h"
#include "Gameplay/Object.h"
#include "Reflection/ReflectionMacros.h"
#include "Reflection/TypeId.h"

#include <cstdint>
#include <string>
#include <vector>

class EditorAction : public Object
{
    ENGINE_ABSTRACT_CLASS(EditorAction, Object, "editor.EditorAction", 1)

public:
    static constexpr const char* ObjectsCategory = "Objects";
    static constexpr const char* ComponentsCategory = "Components";

    virtual const char* GetMenuCategory() const;
    virtual const char* GetDisplayName() const;
    virtual int GetSortOrder() const;
    virtual bool RequiresSelection() const;
    virtual bool CanExecute(const EditorActionContext& Context) const;
    virtual bool Execute(EditorActionContext& Context) const;

    static std::vector<EditorAction*> CollectPublishedActions();

protected:
    EditorAction() = default;

    bool CreateObjectWithComponent(
        EditorActionContext& Context,
        const std::string& ObjectName,
        const TypeId& ComponentType,
        bool bConfigureLightType = false,
        int64_t LightTypeValue = 0) const;

    bool AddComponentToSelection(EditorActionContext& Context, const TypeId& ComponentType) const;
};

ENGINE_CLASS_END(EditorAction)
