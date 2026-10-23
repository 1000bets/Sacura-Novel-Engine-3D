#include "Engine.h"
#include "Core/Object/MemorySubsystem.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Components/ScriptComponent.h"
#include "Input/InputDocument.h"

#if defined(SAKURA_ENABLE_PYTHON)
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

static InputSession& RequireInputSession()
{
    AssertGameThread();
    auto* Runtime = ScriptingSubsystem::Get().GetMaterialEngine();
    if (!Runtime || !Runtime->GetInputSession())
    {
        throw std::runtime_error("Input is available only during a game session");
    }
    return *Runtime->GetInputSession();
}

static std::string ResolveInputAction(const std::string& Action)
{
    const auto& Configuration = RequireInputSession().GetConfiguration();
    for (const auto& Candidate : Configuration.Actions)
    {
        if (Candidate.Identifier == Action)
        {
            return Candidate.Identifier;
        }
    }
    auto* Runtime = ScriptingSubsystem::Get().GetMaterialEngine();
    AssetRegistryEntry Entry;
    if (Runtime->GetAssetRegistry().TryGetByPath(Action, Entry) && Entry.Metadata.Type == InputActionAssetType)
    {
        return Entry.Metadata.Guid.ToString();
    }
    throw std::runtime_error("Input action not found: " + Action);
}

void RegisterInputBindings(pybind11::module_& Module)
{
    using namespace pybind11;
    enum_<InputPhase>(Module, "InputPhase")
        .value("Started", InputPhase::Started).value("Ongoing", InputPhase::Ongoing)
        .value("Triggered", InputPhase::Triggered).value("Completed", InputPhase::Completed)
        .value("Canceled", InputPhase::Canceled);
    class_<InputValue>(Module, "InputValue")
        .def_readonly("Horizontal", &InputValue::Horizontal)
        .def_readonly("Vertical", &InputValue::Vertical)
        .def_readonly("Depth", &InputValue::Depth)
        .def("Magnitude", &InputValue::Magnitude);
    Module.def("BindAction", [](const object& Owner, const std::string& Action, InputPhase Phase, function Callback)
    {
        const ObjectHandle Identity{Owner.attr("_carrier_id").cast<uint64_t>(), Owner.attr("_carrier_generation").cast<uint32_t>()};
        auto* Component = MemorySubsystem::Get()->ResolveHandle<ScriptComponent>(Identity);
        if (!Component || !Component->IsPlaying())
        {
            throw std::runtime_error("Input delegate owner must be a playing component");
        }
        const auto Subscription = RequireInputSession().BindAction(ResolveInputAction(Action), Phase,
            [Callback = std::move(Callback)](const InputActionEvent& Event)
            {
                gil_scoped_acquire Gil;
                try
                {
                    Callback(Event.Value);
                }
                catch (const error_already_set& Error)
                {
                    PrintString(std::string("Input delegate failed: ") + Error.what());
                    PyErr_Clear();
                }
            }, Identity, [Identity]()
            {
                const auto* Component = MemorySubsystem::Get()->ResolveHandle<ScriptComponent>(Identity);
                return Component && Component->IsPlaying() && !Component->IsPendingRemoval();
            });
        return Subscription;
    }, arg("Owner"), arg("Action"), arg("Phase"), arg("Callback"));
    Module.def("Unbind", [](uint64_t Subscription)
    {
        AssertGameThread();
        auto* Runtime = ScriptingSubsystem::Get().GetMaterialEngine();
        if (Runtime && Runtime->GetInputSession())
        {
            Runtime->GetInputSession()->Unbind(Subscription);
        }
    });
    Module.def("GetInputValue", [](const std::string& Action)
    {
        return RequireInputSession().GetValue(ResolveInputAction(Action));
    });
    Module.def("SetInputContexts", [](const std::vector<std::pair<std::string, int>>& Contexts)
    {
        std::vector<InputContextActivation> Activations;
        auto& Session = RequireInputSession();
        for (const auto& Requested : Contexts)
        {
            std::string Identifier = Requested.first;
            AssetRegistryEntry Entry;
            if (ScriptingSubsystem::Get().GetMaterialEngine()->GetAssetRegistry().TryGetByPath(Identifier, Entry))
            {
                Identifier = Entry.Metadata.Guid.ToString();
            }
            const auto& Configuration = Session.GetConfiguration();
            if (std::none_of(Configuration.Contexts.begin(), Configuration.Contexts.end(),
                [&](const auto& Context) { return Context.Identifier == Identifier; }))
            {
                throw std::runtime_error("Input context not found: " + Requested.first);
            }
            Activations.push_back({Identifier, Requested.second});
        }
        Session.SetContexts(std::move(Activations));
    });
}
#endif
