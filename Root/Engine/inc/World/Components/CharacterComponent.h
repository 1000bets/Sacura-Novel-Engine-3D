#pragma once

#include "World/Components/Component.h"
#include "Reflection/ReflectionMacros.h"

#include <string>

// Authored identity for scene characters. The GameObject owns the name and pose;
// this component keeps character metadata in the scene document.
class CharacterComponent : public Component
{
    ENGINE_CLASS(CharacterComponent, Component, "engine.CharacterComponent", 1)

public:
    std::string Description;
    ENGINE_REFLECT_FIELD(CharacterComponent, Description, "description",
        RF_EDITOR_EDITABLE, RF_SERIALIZABLE, RF_SCRIPT_READ_WRITE,
        RF_DISPLAY_NAME("Description"))

protected:
    CharacterComponent() = default;
};

ENGINE_CLASS_END(CharacterComponent)
ENGINE_IMPLEMENT_FIELD(CharacterComponent, Description)
