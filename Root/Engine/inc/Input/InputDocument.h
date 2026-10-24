#pragma once

#include "Input/InputTypes.h"
#include <filesystem>
#include <nlohmann/json.hpp>

class AssetRegistry;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputValue, Horizontal, Vertical, Depth)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputModifiers, Scale, AxisOrder, DeadZone, bNormalize)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputAction, Identifier, Name, ValueType, Trigger, Accumulation, Modifiers, HoldSeconds, Threshold, ChordAction, bExecuteWhenPaused, bClampMagnitude, bRequireNeutral)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputBinding, Identifier, Action, Device, Control, Modifiers, bConsume)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputMappingContext, Identifier, Name, Bindings)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputContextActivation, Context, Priority)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputBindingOverride, Context, Binding, Device, Control, Sensitivity)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(InputStoryBinding, Action, Phase, Event)

class InputDocument
{
public:
    static bool Read(const std::filesystem::path& Path, nlohmann::json& Document, std::string& Error);
    static bool Write(const std::filesystem::path& Path, const nlohmann::json& Document, std::string& Error);
    static bool LoadConfiguration(AssetRegistry& Registry, InputConfiguration& Configuration, std::string& Error,
        const std::filesystem::path& ProjectFile = {}, bool bValidate = true);
    static bool ReadProjectInput(const std::filesystem::path& ProjectFile, nlohmann::json& Settings, std::string& Error);
    static bool SaveProjectInput(const std::filesystem::path& ProjectFile, const nlohmann::json& Settings, std::string& Error);
    static bool SaveStartupContexts(const std::filesystem::path& ProjectFile,
        const std::vector<InputContextActivation>& Contexts, std::string& Error);
    static bool SaveProfile(const std::filesystem::path& ContentRoot,
        const std::vector<InputBindingOverride>& Overrides, std::string& Error);
    static InputConfiguration CreateDefaults();
};
