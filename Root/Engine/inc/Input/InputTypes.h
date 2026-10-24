#pragma once

#include "Reflection/ReflectionMacros.h"

#include <array>
#include <string>
#include <vector>

enum class InputValueType { Boolean, Axis1D, Axis2D, Axis3D };
ENGINE_REFLECT_ENUM(InputValueType, "engine.InputValueType",
    ENGINE_ENUM_VALUE(InputValueType, Boolean),
    ENGINE_ENUM_VALUE(InputValueType, Axis1D),
    ENGINE_ENUM_VALUE(InputValueType, Axis2D),
    ENGINE_ENUM_VALUE(InputValueType, Axis3D));
enum class InputTrigger { Pressed, Released, Hold, Continuous };
ENGINE_REFLECT_ENUM(InputTrigger, "engine.InputTrigger",
    ENGINE_ENUM_VALUE(InputTrigger, Pressed),
    ENGINE_ENUM_VALUE(InputTrigger, Released),
    ENGINE_ENUM_VALUE(InputTrigger, Hold),
    ENGINE_ENUM_VALUE(InputTrigger, Continuous));
enum class InputPhase { Started, Ongoing, Triggered, Completed, Canceled };
enum class InputAccumulation { Sum, Strongest };
ENGINE_REFLECT_ENUM(InputAccumulation, "engine.InputAccumulation",
    ENGINE_ENUM_VALUE(InputAccumulation, Sum),
    ENGINE_ENUM_VALUE(InputAccumulation, Strongest));

struct InputValue
{
    ENGINE_STRUCT(InputValue, "engine.InputValue")
public:
    float Horizontal = 0.f;
    float Vertical = 0.f;
    float Depth = 0.f;
    float Magnitude() const;
    ENGINE_REFLECT_FIELD(InputValue, Horizontal, "Horizontal", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Horizontal"))
    ENGINE_REFLECT_FIELD(InputValue, Vertical, "Vertical", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Vertical"))
    ENGINE_REFLECT_FIELD(InputValue, Depth, "Depth", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Depth"))
};
ENGINE_STRUCT_END(InputValue)
ENGINE_IMPLEMENT_FIELD(InputValue, Horizontal)
ENGINE_IMPLEMENT_FIELD(InputValue, Vertical)
ENGINE_IMPLEMENT_FIELD(InputValue, Depth)


struct InputModifiers
{
    ENGINE_STRUCT(InputModifiers, "engine.InputModifiers")
public:
    InputValue Scale{1.f, 1.f, 1.f};
    std::array<int, 3> AxisOrder{0, 1, 2};
    float DeadZone = 0.f;
    bool bNormalize = false;
    ENGINE_REFLECT_FIELD(InputModifiers, Scale, "Scale", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Scale"))
    ENGINE_REFLECT_FIELD(InputModifiers, AxisOrder, "AxisOrder", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Axis order"))
    ENGINE_REFLECT_FIELD(InputModifiers, DeadZone, "DeadZone", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Dead zone"))
    ENGINE_REFLECT_FIELD(InputModifiers, bNormalize, "bNormalize", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Normalize"))
};
ENGINE_STRUCT_END(InputModifiers)
ENGINE_IMPLEMENT_FIELD(InputModifiers, Scale)
ENGINE_IMPLEMENT_FIELD(InputModifiers, AxisOrder)
ENGINE_IMPLEMENT_FIELD(InputModifiers, DeadZone)
ENGINE_IMPLEMENT_FIELD(InputModifiers, bNormalize)


struct InputAction
{
    ENGINE_STRUCT(InputAction, "engine.InputAction")
public:
    std::string Identifier;
    std::string Name;
    InputValueType ValueType = InputValueType::Boolean;
    InputTrigger Trigger = InputTrigger::Pressed;
    InputAccumulation Accumulation = InputAccumulation::Sum;
    InputModifiers Modifiers;
    float HoldSeconds = 0.4f;
    float Threshold = 0.5f;
    std::string ChordAction;
    bool bExecuteWhenPaused = false;
    bool bClampMagnitude = true;
    bool bRequireNeutral = true;
    ENGINE_REFLECT_FIELD(InputAction, Identifier, "Identifier", RF_SERIALIZABLE, RF_DISPLAY_NAME("Identifier"))
    ENGINE_REFLECT_FIELD(InputAction, Name, "Name", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Name"))
    ENGINE_REFLECT_FIELD(InputAction, ValueType, "ValueType", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Value type"))
    ENGINE_REFLECT_FIELD(InputAction, Trigger, "Trigger", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Trigger"))
    ENGINE_REFLECT_FIELD(InputAction, Accumulation, "Accumulation", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Accumulation"))
    ENGINE_REFLECT_FIELD(InputAction, Modifiers, "Modifiers", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Modifiers"))
    ENGINE_REFLECT_FIELD(InputAction, HoldSeconds, "HoldSeconds", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Hold duration"))
    ENGINE_REFLECT_FIELD(InputAction, Threshold, "Threshold", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Threshold"))
    ENGINE_REFLECT_FIELD(InputAction, ChordAction, "ChordAction", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Chord action"), RF_ASSET_TYPE("InputAction"))
    ENGINE_REFLECT_FIELD(InputAction, bExecuteWhenPaused, "bExecuteWhenPaused", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Execute when paused"))
    ENGINE_REFLECT_FIELD(InputAction, bClampMagnitude, "bClampMagnitude", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Clamp magnitude"))
    ENGINE_REFLECT_FIELD(InputAction, bRequireNeutral, "bRequireNeutral", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Require neutral"))
};
ENGINE_STRUCT_END(InputAction)
ENGINE_IMPLEMENT_FIELD(InputAction, Identifier)
ENGINE_IMPLEMENT_FIELD(InputAction, Name)
ENGINE_IMPLEMENT_FIELD(InputAction, ValueType)
ENGINE_IMPLEMENT_FIELD(InputAction, Trigger)
ENGINE_IMPLEMENT_FIELD(InputAction, Accumulation)
ENGINE_IMPLEMENT_FIELD(InputAction, Modifiers)
ENGINE_IMPLEMENT_FIELD(InputAction, HoldSeconds)
ENGINE_IMPLEMENT_FIELD(InputAction, Threshold)
ENGINE_IMPLEMENT_FIELD(InputAction, ChordAction)
ENGINE_IMPLEMENT_FIELD(InputAction, bExecuteWhenPaused)
ENGINE_IMPLEMENT_FIELD(InputAction, bClampMagnitude)
ENGINE_IMPLEMENT_FIELD(InputAction, bRequireNeutral)


struct InputBinding
{
    ENGINE_STRUCT(InputBinding, "engine.InputBinding")
public:
    std::string Identifier;
    std::string Action;
    std::string Device = "Keyboard";
    std::string Control = "KeyE";
    InputModifiers Modifiers;
    bool bConsume = true;
    ENGINE_REFLECT_FIELD(InputBinding, Identifier, "Identifier", RF_SERIALIZABLE, RF_DISPLAY_NAME("Identifier"))
    ENGINE_REFLECT_FIELD(InputBinding, Action, "Action", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Action"), RF_ASSET_TYPE("InputAction"))
    ENGINE_REFLECT_FIELD(InputBinding, Device, "Device", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Device"))
    ENGINE_REFLECT_FIELD(InputBinding, Control, "Control", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Control"))
    ENGINE_REFLECT_FIELD(InputBinding, Modifiers, "Modifiers", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Modifiers"))
    ENGINE_REFLECT_FIELD(InputBinding, bConsume, "bConsume", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Consume input"))
};
ENGINE_STRUCT_END(InputBinding)
ENGINE_IMPLEMENT_FIELD(InputBinding, Identifier)
ENGINE_IMPLEMENT_FIELD(InputBinding, Action)
ENGINE_IMPLEMENT_FIELD(InputBinding, Device)
ENGINE_IMPLEMENT_FIELD(InputBinding, Control)
ENGINE_IMPLEMENT_FIELD(InputBinding, Modifiers)
ENGINE_IMPLEMENT_FIELD(InputBinding, bConsume)


struct InputMappingContext
{
    ENGINE_STRUCT(InputMappingContext, "engine.InputMappingContext")
public:
    std::string Identifier;
    std::string Name;
    std::vector<InputBinding> Bindings;
    ENGINE_REFLECT_FIELD(InputMappingContext, Identifier, "Identifier", RF_SERIALIZABLE, RF_DISPLAY_NAME("Identifier"))
    ENGINE_REFLECT_FIELD(InputMappingContext, Name, "Name", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Name"))
    ENGINE_REFLECT_FIELD(InputMappingContext, Bindings, "Bindings", RF_SERIALIZABLE | RF_EDITOR_EDITABLE, RF_DISPLAY_NAME("Bindings"))
};
ENGINE_STRUCT_END(InputMappingContext)
ENGINE_IMPLEMENT_FIELD(InputMappingContext, Identifier)
ENGINE_IMPLEMENT_FIELD(InputMappingContext, Name)
ENGINE_IMPLEMENT_FIELD(InputMappingContext, Bindings)


struct InputContextActivation
{
    std::string Context;
    int Priority = 0;
};

struct InputActionEvent
{
    std::string Action;
    InputPhase Phase = InputPhase::Canceled;
    InputValue Value;
};

struct InputBindingOverride
{
    std::string Context;
    std::string Binding;
    std::string Device;
    std::string Control;
    InputValue Sensitivity{1.f, 1.f, 1.f};
};

struct InputConfiguration
{
    std::vector<InputAction> Actions;
    std::vector<InputMappingContext> Contexts;
    std::vector<InputContextActivation> StartupContexts;
    std::vector<InputBindingOverride> Overrides;
};

struct InputStoryBinding
{
    std::string Action;
    InputPhase Phase = InputPhase::Triggered;
    std::string Event;
};

InputValue ApplyInputModifiers(InputValue Value, const InputModifiers& Modifiers);
const char* InputPhaseName(InputPhase Phase);
