#pragma once

#include <array>
#include <string>
#include <vector>

enum class InputValueType { Boolean, Axis1D, Axis2D, Axis3D };
enum class InputTrigger { Pressed, Released, Hold, Continuous };
enum class InputPhase { Started, Ongoing, Triggered, Completed, Canceled };
enum class InputAccumulation { Sum, Strongest };

struct InputValue
{
    float Horizontal = 0.f;
    float Vertical = 0.f;
    float Depth = 0.f;
    float Magnitude() const;
};

struct InputModifiers
{
    InputValue Scale{1.f, 1.f, 1.f};
    std::array<int, 3> AxisOrder{0, 1, 2};
    float DeadZone = 0.f;
    bool bNormalize = false;
};

struct InputAction
{
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
};

struct InputBinding
{
    std::string Identifier;
    std::string Action;
    std::string Device = "Keyboard";
    std::string Control = "KeyE";
    InputModifiers Modifiers;
    bool bConsume = true;
};

struct InputMappingContext
{
    std::string Identifier;
    std::string Name;
    std::vector<InputBinding> Bindings;
};

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
