#include "Authoring/CoreOperations.h"
#include "Authoring/OperationContext.h"

#include <algorithm>

namespace Authoring
{
namespace
{
class WaitTask final : public OperationTask
{
public:
    explicit WaitTask(double Duration) : RemainingSeconds(Duration) {}

    TaskResult Tick(OperationContext&, double DeltaSeconds) override
    {
        RemainingSeconds = std::max(0.0, RemainingSeconds - DeltaSeconds);
        return RemainingSeconds == 0.0 ? TaskResult::Complete() : TaskResult{};
    }

private:
    double RemainingSeconds;
};
}

Diagnostic RegisterCoreOperations(OperationRegistry& Registry)
{
    OperationDefinition Wait;
    Wait.Id = "sacura.core.wait";
    Wait.Label = "Wait";
    Wait.Contexts = {CallSite::Event, CallSite::Function, CallSite::Rule, CallSite::Mechanic, CallSite::Widget};
    Wait.Parameters = {{"seconds", "Duration", {ReflectedValue::Kind::Float64, 0.0, {}}, ReflectedValue::MakeFloat64(0.0)}};
    return Registry.Register(std::move(Wait), [](const Arguments& Values, OperationContext&)
    {
        return std::make_unique<WaitTask>(Values.at("seconds").Float64Value);
    });
}
}
