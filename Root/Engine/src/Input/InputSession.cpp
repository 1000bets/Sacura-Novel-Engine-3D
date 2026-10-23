#include "Input/InputSession.h"
#include "Core/Threading/ThreadContext.h"
#include <algorithm>
#include <cmath>

uint64_t InputSession::BindAction(const std::string& Action, InputPhase Phase, Listener Callback,
    ObjectHandle Owner, std::function<bool()> IsAlive)
{
    AssertGameThread();
    const auto Found = std::find_if(Configuration.Actions.begin(), Configuration.Actions.end(),
        [&](const auto& Candidate) { return Candidate.Identifier == Action; });
    if (Found == Configuration.Actions.end() || !Callback)
    {
        return 0;
    }
    static uint64_t NextSubscription = 1;
    const uint64_t Subscription = NextSubscription++;
    Delegates.emplace(Subscription, ActionDelegate{Action, Phase, std::move(Callback), Owner, std::move(IsAlive)});
    return Subscription;
}

void InputSession::Unbind(uint64_t Subscription)
{
    AssertGameThread();
    Delegates.erase(Subscription);
}

void InputSession::UnbindOwner(ObjectHandle Owner)
{
    AssertGameThread();
    for (auto Iterator = Delegates.begin(); Iterator != Delegates.end();)
    {
        if (Iterator->second.Owner == Owner)
        {
            Iterator = Delegates.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
}

float InputValue::Magnitude() const
{
    return std::sqrt(Horizontal * Horizontal + Vertical * Vertical + Depth * Depth);
}

InputValue ApplyInputModifiers(InputValue Value, const InputModifiers& Modifiers)
{
    const std::array<float, 3> Components{Value.Horizontal, Value.Vertical, Value.Depth};
    Value = {Components[Modifiers.AxisOrder[0]] * Modifiers.Scale.Horizontal,
        Components[Modifiers.AxisOrder[1]] * Modifiers.Scale.Vertical,
        Components[Modifiers.AxisOrder[2]] * Modifiers.Scale.Depth};
    const float Magnitude = Value.Magnitude();
    if (Magnitude <= Modifiers.DeadZone)
    {
        return {};
    }
    float Factor = 1.f;
    if (Modifiers.DeadZone > 0.f)
    {
        Factor = (Magnitude - Modifiers.DeadZone) / ((1.f - Modifiers.DeadZone) * Magnitude);
    }
    if (Modifiers.bNormalize)
    {
        Factor = 1.f / Magnitude;
    }
    return {Value.Horizontal * Factor, Value.Vertical * Factor, Value.Depth * Factor};
}

const char* InputPhaseName(InputPhase Phase)
{
    switch (Phase)
    {
    case InputPhase::Started: return "Started";
    case InputPhase::Ongoing: return "Ongoing";
    case InputPhase::Triggered: return "Triggered";
    case InputPhase::Completed: return "Completed";
    default: return "Canceled";
    }
}

static bool ValidModifiers(const InputModifiers& Modifiers)
{
    return std::isfinite(Modifiers.Scale.Horizontal) && std::isfinite(Modifiers.Scale.Vertical)
        && std::isfinite(Modifiers.Scale.Depth) && std::isfinite(Modifiers.DeadZone)
        && Modifiers.DeadZone >= 0.f && Modifiers.DeadZone < 1.f
        && std::all_of(Modifiers.AxisOrder.begin(), Modifiers.AxisOrder.end(), [](int Axis) { return Axis >= 0 && Axis < 3; });
}

bool InputSession::Validate(const InputConfiguration& Configuration, std::string& Error)
{
    std::set<std::string> ActionIdentifiers;
    std::set<std::string> ContextIdentifiers;
    for (const auto& Action : Configuration.Actions)
    {
        if (Action.Identifier.empty() || !ActionIdentifiers.insert(Action.Identifier).second
            || !ValidModifiers(Action.Modifiers) || !std::isfinite(Action.HoldSeconds) || Action.HoldSeconds < 0.f
            || !std::isfinite(Action.Threshold) || Action.Threshold <= 0.f || Action.Threshold > 1.f
            || static_cast<int>(Action.ValueType) < 0 || static_cast<int>(Action.ValueType) > 3
            || static_cast<int>(Action.Trigger) < 0 || static_cast<int>(Action.Trigger) > 3
            || static_cast<int>(Action.Accumulation) < 0 || static_cast<int>(Action.Accumulation) > 1)
        {
            Error = "Invalid input action: " + Action.Name;
            return false;
        }
    }
    for (const auto& Action : Configuration.Actions)
    {
        std::set<std::string> Chain{Action.Identifier};
        std::string Chord = Action.ChordAction;
        while (!Chord.empty())
        {
            if (!ActionIdentifiers.count(Chord) || !Chain.insert(Chord).second)
            {
                Error = "Missing or cyclic chord action: " + Action.Name;
                return false;
            }
            const auto Found = std::find_if(Configuration.Actions.begin(), Configuration.Actions.end(),
                [&](const auto& Candidate) { return Candidate.Identifier == Chord; });
            Chord = Found->ChordAction;
        }
    }
    for (const auto& Context : Configuration.Contexts)
    {
        if (Context.Identifier.empty() || !ContextIdentifiers.insert(Context.Identifier).second)
        {
            Error = "Duplicate or empty input context identifier";
            return false;
        }
        std::set<std::string> BindingIdentifiers;
        for (const auto& Binding : Context.Bindings)
        {
            if (Binding.Identifier.empty() || !BindingIdentifiers.insert(Binding.Identifier).second
                || !ActionIdentifiers.count(Binding.Action) || Binding.Device.empty() || Binding.Control.empty()
                || !ValidModifiers(Binding.Modifiers))
            {
                Error = "Invalid binding in context: " + Context.Name;
                return false;
            }
        }
    }
    for (const auto& Activation : Configuration.StartupContexts)
    {
        if (!ContextIdentifiers.count(Activation.Context))
        {
            Error = "Missing startup input context";
            return false;
        }
    }
    for (const auto& Override : Configuration.Overrides)
    {
        if (Override.Device.empty() || Override.Control.empty() || !std::isfinite(Override.Sensitivity.Magnitude()))
        {
            Error = "Invalid input profile override";
            return false;
        }
    }
    Error.clear();
    return true;
}

bool InputSession::Configure(InputConfiguration NewConfiguration, std::string& Error)
{
    AssertGameThread();
    if (!Validate(NewConfiguration, Error))
    {
        return false;
    }
    Cancel();
    Configuration = std::move(NewConfiguration);
    ActiveContexts = Configuration.StartupContexts;
    bPaused = false;
    return true;
}

void InputSession::Feed(const std::string& Source, const std::string& Device, const std::string& Control,
    InputValue Value, bool bTransient)
{
    AssertGameThread();
    if (!std::isfinite(Value.Magnitude()))
    {
        Value = {};
    }
    PendingSources.push_back({Source, Device, Control, Value, bTransient});
}

void InputSession::UpdateSource(const SourceValue& Update)
{
    const auto& Source = Update.Source;
    const auto& Device = Update.Device;
    const auto& Control = Update.Control;
    const auto Value = Update.Value;
    const bool bTransient = Update.bTransient;
    auto Found = std::find_if(Sources.begin(), Sources.end(), [&](const SourceValue& Entry)
    {
        return Entry.Source == Source && Entry.Device == Device && Entry.Control == Control;
    });
    if (Found == Sources.end())
    {
        Sources.push_back({Source, Device, Control, Value, bTransient});
    }
    else if (bTransient)
    {
        Found->Value.Horizontal += Value.Horizontal;
        Found->Value.Vertical += Value.Vertical;
        Found->Value.Depth += Value.Depth;
    }
    else
    {
        Found->Value = Value;
    }
}

void InputSession::Emit(const std::string& Action, InputPhase Phase, InputValue Value)
{
    Events.push_back({Action, Phase, Value});
}

void InputSession::CancelStates()
{
    for (const auto& Entry : States)
    {
        Emit(Entry.first, InputPhase::Canceled);
    }
    States.clear();
}

void InputSession::BlockHeld()
{
    for (const auto& Entry : Resolve())
    {
        if (Entry.second.Magnitude() > 0.0001f)
        {
            Blocked.insert(Entry.first);
        }
    }
}

void InputSession::RemoveSource(const std::string& Source)
{
    AssertGameThread();
    ApplyPendingSources();
    std::set<std::string> Affected;
    for (const auto& Context : Configuration.Contexts)
    {
        for (auto Binding : Context.Bindings)
        {
            for (const auto& Override : Configuration.Overrides)
            {
                if (Override.Context == Context.Identifier && Override.Binding == Binding.Identifier)
                {
                    Binding.Device = Override.Device;
                    Binding.Control = Override.Control;
                }
            }
            for (const auto& Entry : Sources)
            {
                if (Entry.Source == Source && Entry.Device == Binding.Device && Entry.Control == Binding.Control)
                {
                    Affected.insert(Binding.Action);
                }
            }
        }
    }
    Sources.erase(std::remove_if(Sources.begin(), Sources.end(), [&](const auto& Entry)
    {
        return Entry.Source == Source;
    }), Sources.end());
    const auto Values = Resolve();
    for (const auto& Action : Affected)
    {
        if (States.erase(Action))
        {
            Emit(Action, InputPhase::Canceled);
        }
        if (Values.at(Action).Magnitude() > 0.0001f)
        {
            Blocked.insert(Action);
        }
    }
}

bool InputSession::SetOverrides(std::vector<InputBindingOverride> Overrides, std::string& Error)
{
    AssertGameThread();
    auto Candidate = Configuration;
    Candidate.Overrides = std::move(Overrides);
    if (!Validate(Candidate, Error))
    {
        return false;
    }
    ApplyPendingSources();
    CancelStates();
    Configuration.Overrides = std::move(Candidate.Overrides);
    BlockHeld();
    return true;
}

void InputSession::Cancel()
{
    AssertGameThread();
    CancelStates();
    Sources.clear();
    PendingSources.clear();
    Blocked.clear();
}

void InputSession::Close()
{
    AssertGameThread();
    Cancel();
    Delegates.clear();
    EventListener = {};
}

void InputSession::SetContexts(std::vector<InputContextActivation> Contexts)
{
    AssertGameThread();
    ApplyPendingSources();
    CancelStates();
    ActiveContexts = std::move(Contexts);
    Blocked.clear();
    BlockHeld();
}

void InputSession::SetPaused(bool bNewPaused)
{
    AssertGameThread();
    if (bPaused == bNewPaused)
    {
        return;
    }
    CancelStates();
    ApplyPendingSources();
    BlockHeld();
    bPaused = bNewPaused;
    BlockHeld();
}

std::map<std::string, InputValue> InputSession::Resolve() const
{
    std::map<std::string, InputValue> Values;
    std::set<std::string> Claimed;
    auto Sorted = ActiveContexts;
    std::stable_sort(Sorted.begin(), Sorted.end(), [](const auto& First, const auto& Second)
    {
        return First.Priority > Second.Priority;
    });
    std::set<std::string> Visited;
    for (const auto& Activation : Sorted)
    {
        const auto Context = std::find_if(Configuration.Contexts.begin(), Configuration.Contexts.end(),
            [&](const auto& Candidate) { return Candidate.Identifier == Activation.Context; });
        if (Context == Configuration.Contexts.end() || !Visited.insert(Activation.Context).second)
        {
            continue;
        }
        std::set<std::string> Claims;
        for (auto Binding : Context->Bindings)
        {
            const auto Action = std::find_if(Configuration.Actions.begin(), Configuration.Actions.end(),
                [&](const auto& Candidate) { return Candidate.Identifier == Binding.Action; });
            if (Action == Configuration.Actions.end() || (bPaused && !Action->bExecuteWhenPaused))
            {
                continue;
            }
            InputValue Sensitivity{1.f, 1.f, 1.f};
            for (const auto& Override : Configuration.Overrides)
            {
                if (Override.Context == Context->Identifier && Override.Binding == Binding.Identifier)
                {
                    Binding.Device = Override.Device;
                    Binding.Control = Override.Control;
                    Sensitivity = Override.Sensitivity;
                }
            }
            const std::string Channel = Binding.Device + "/" + Binding.Control;
            if (Claimed.count(Channel))
            {
                continue;
            }
            if (Binding.bConsume)
            {
                Claims.insert(Channel);
            }
            InputValue Raw;
            for (const auto& Source : Sources)
            {
                if (Source.Device == Binding.Device && Source.Control == Binding.Control
                    && Source.Value.Magnitude() > Raw.Magnitude())
                {
                    Raw = Source.Value;
                }
            }
            InputValue Modified = ApplyInputModifiers(Raw, Binding.Modifiers);
            Modified.Horizontal *= Sensitivity.Horizontal;
            Modified.Vertical *= Sensitivity.Vertical;
            Modified.Depth *= Sensitivity.Depth;
            auto& Value = Values[Binding.Action];
            if (Action->Accumulation == InputAccumulation::Strongest)
            {
                if (Modified.Magnitude() > Value.Magnitude())
                {
                    Value = Modified;
                }
            }
            else
            {
                Value.Horizontal += Modified.Horizontal;
                Value.Vertical += Modified.Vertical;
                Value.Depth += Modified.Depth;
            }
        }
        Claimed.insert(Claims.begin(), Claims.end());
    }
    for (const auto& Action : Configuration.Actions)
    {
        auto& Value = Values[Action.Identifier];
        Value = ApplyInputModifiers(Value, Action.Modifiers);
        if (Action.ValueType != InputValueType::Axis3D)
        {
            Value.Depth = 0.f;
        }
        if (Action.ValueType == InputValueType::Boolean || Action.ValueType == InputValueType::Axis1D)
        {
            Value.Vertical = 0.f;
        }
        if (Action.ValueType == InputValueType::Boolean)
        {
            if (std::abs(Value.Horizontal) >= Action.Threshold)
            {
                Value.Horizontal = 1.f;
            }
            else
            {
                Value.Horizontal = 0.f;
            }
        }
        const float Magnitude = Value.Magnitude();
        if (Magnitude > 1.f)
        {
            Value.Horizontal /= Magnitude;
            Value.Vertical /= Magnitude;
            Value.Depth /= Magnitude;
        }
    }
    return Values;
}

void InputSession::Evaluate(float DeltaTime, bool bContinuous)
{
    AssertGameThread();
    const auto Values = Resolve();
    for (const auto& Action : Configuration.Actions)
    {
        const auto Value = Values.at(Action.Identifier);
        bool bActive = Value.Magnitude() > 0.0001f;
        std::string Chord = Action.ChordAction;
        while (!Chord.empty())
        {
            bActive = bActive && Values.at(Chord).Magnitude() > 0.0001f && !Blocked.count(Chord);
            const auto Found = std::find_if(Configuration.Actions.begin(), Configuration.Actions.end(),
                [&](const auto& Candidate) { return Candidate.Identifier == Chord; });
            Chord = Found->ChordAction;
        }
        if (Blocked.count(Action.Identifier))
        {
            if (Value.Magnitude() <= 0.0001f)
            {
                Blocked.erase(Action.Identifier);
            }
            continue;
        }
        auto Found = States.find(Action.Identifier);
        if (bActive)
        {
            const bool bStarted = Found == States.end();
            if (bStarted)
            {
                Emit(Action.Identifier, InputPhase::Started, Value);
            }
            auto& State = States[Action.Identifier];
            State.Value = Value;
            if (std::isfinite(DeltaTime))
            {
                State.Elapsed += std::max(0.f, DeltaTime);
            }
            if ((Action.Trigger == InputTrigger::Pressed && bStarted) || (Action.Trigger == InputTrigger::Continuous && bContinuous)
                || (Action.Trigger == InputTrigger::Hold && !State.bTriggered && State.Elapsed >= Action.HoldSeconds))
            {
                State.bTriggered = true;
                Emit(Action.Identifier, InputPhase::Triggered, Value);
            }
            else if (!State.bTriggered && bContinuous)
            {
                Emit(Action.Identifier, InputPhase::Ongoing, Value);
            }
        }
        else if (Found != States.end())
        {
            if (Action.Trigger == InputTrigger::Released && Value.Magnitude() <= 0.0001f)
            {
                Found->second.bTriggered = true;
                Emit(Action.Identifier, InputPhase::Triggered, Found->second.Value);
            }
            InputPhase Phase = InputPhase::Canceled;
            if (Found->second.bTriggered)
            {
                Phase = InputPhase::Completed;
            }
            Emit(Action.Identifier, Phase);
            States.erase(Found);
        }
    }
}

void InputSession::ApplyPendingSources()
{
    for (const auto& Source : PendingSources)
    {
        UpdateSource(Source);
    }
    PendingSources.clear();
}

void InputSession::Tick(float DeltaTime)
{
    AssertGameThread();
    for (auto Iterator = Delegates.begin(); Iterator != Delegates.end();)
    {
        if (Iterator->second.IsAlive && !Iterator->second.IsAlive())
        {
            Iterator = Delegates.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
    for (const auto& Source : PendingSources)
    {
        UpdateSource(Source);
        if (!Source.bTransient)
        {
            Evaluate(0.f, false);
        }
    }
    PendingSources.clear();
    Evaluate(DeltaTime, true);
    for (auto& Source : Sources)
    {
        if (Source.bTransient)
        {
            Source.Value = {};
        }
    }
    const auto Pending = std::move(Events);
    Events.clear();
    LastEvents = Pending;
    const auto Callback = EventListener;
    for (const auto& Event : Pending)
    {
        const auto Definition = std::find_if(Configuration.Actions.begin(), Configuration.Actions.end(),
            [&](const auto& Action) { return Action.Identifier == Event.Action; });
        if (bPaused && Definition != Configuration.Actions.end() && !Definition->bExecuteWhenPaused
            && Event.Phase != InputPhase::Canceled)
        {
            continue;
        }
        if (Callback)
        {
            Callback(Event);
        }
        std::vector<uint64_t> Subscriptions;
        for (const auto& Entry : Delegates)
        {
            if (Entry.second.Action == Event.Action && Entry.second.Phase == Event.Phase)
            {
                Subscriptions.push_back(Entry.first);
            }
        }
        for (const auto Subscription : Subscriptions)
        {
            const auto Found = Delegates.find(Subscription);
            if (Found != Delegates.end() && (!Found->second.IsAlive || Found->second.IsAlive()))
            {
                const auto Invoke = Found->second.Callback;
                Invoke(Event);
            }
        }
    }
}

InputValue InputSession::GetValue(const std::string& Action) const
{
    AssertGameThread();
    const auto Found = States.find(Action);
    if (Found == States.end())
    {
        return {};
    }
    return Found->second.Value;
}
