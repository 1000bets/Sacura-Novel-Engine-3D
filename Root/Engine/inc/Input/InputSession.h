#pragma once

#include "Input/InputTypes.h"
#include "Core/Object/ObjectHandle.h"
#include <functional>
#include <map>
#include <set>

class InputSession
{
public:
    using Listener = std::function<void(const InputActionEvent&)>;
    bool Configure(InputConfiguration Configuration, std::string& Error);
    void Feed(const std::string& Source, const std::string& Device, const std::string& Control,
        InputValue Value, bool bTransient = false);
    void RemoveSource(const std::string& Source);
    void SetContexts(std::vector<InputContextActivation> Contexts);
    void SetPaused(bool bPaused);
    bool SetOverrides(std::vector<InputBindingOverride> Overrides, std::string& Error);
    void Cancel();
    void Close();
    void Tick(float DeltaTime);
    void SetListener(Listener Callback) { EventListener = std::move(Callback); }
    uint64_t BindAction(const std::string& Action, InputPhase Phase, Listener Callback,
        ObjectHandle Owner = {}, std::function<bool()> IsAlive = {});
    void Unbind(uint64_t Subscription);
    void UnbindOwner(ObjectHandle Owner);
    InputValue GetValue(const std::string& Action) const;
    const InputConfiguration& GetConfiguration() const { return Configuration; }
    const std::vector<InputContextActivation>& GetContexts() const { return ActiveContexts; }
    const std::vector<InputActionEvent>& GetEvents() const { return LastEvents; }
    static bool Validate(const InputConfiguration& Configuration, std::string& Error);

private:
    struct SourceValue
    {
        std::string Source;
        std::string Device;
        std::string Control;
        InputValue Value;
        bool bTransient = false;
    };
    struct ActionState
    {
        InputValue Value;
        float Elapsed = 0.f;
        bool bTriggered = false;
    };
    struct ActionDelegate
    {
        std::string Action;
        InputPhase Phase;
        Listener Callback;
        ObjectHandle Owner;
        std::function<bool()> IsAlive;
    };
    std::map<std::string, InputValue> Resolve() const;
    void UpdateSource(const SourceValue& Value);
    void Evaluate(float DeltaTime, bool bContinuous);
    void ApplyPendingSources();
    void CancelStates();
    void BlockHeld();
    void Emit(const std::string& Action, InputPhase Phase, InputValue Value = {});
    InputConfiguration Configuration;
    std::vector<InputContextActivation> ActiveContexts;
    std::vector<SourceValue> Sources;
    std::vector<SourceValue> PendingSources;
    std::map<std::string, ActionState> States;
    std::set<std::string> Blocked;
    std::vector<InputActionEvent> Events;
    std::vector<InputActionEvent> LastEvents;
    Listener EventListener;
    std::map<uint64_t, ActionDelegate> Delegates;
    bool bPaused = false;
};
