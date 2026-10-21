#pragma once

#include "Authoring/AuthoringTypes.h"

namespace Authoring
{
class OperationContext;

struct TaskResult
{
    RunStatus Status = RunStatus::Running;
    ReflectedValue Value;
    Diagnostic Error;

    static TaskResult Complete(ReflectedValue Value = {});
    static TaskResult Fail(std::string Message);
};

class OperationTask
{
public:
    virtual ~OperationTask() = default;
    virtual TaskResult Tick(OperationContext& Context, double DeltaSeconds) = 0;
    virtual void Cancel(OperationContext& Context) noexcept {}
};
}
