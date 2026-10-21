#pragma once

#include "Authoring/OperationRegistry.h"

#include <memory>

namespace Authoring
{
class Runtime
{
public:
    explicit Runtime(std::shared_ptr<const OperationRegistry> Registry, ObjectHandle World = {});
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

    Diagnostic Start(const std::vector<VariableDefinition>& Variables = {},
        const std::vector<SignalDefinition>& Signals = {});
    void Stop();
    Submission Submit(const OperationCall& Call, ObjectHandle Owner = {});
    Diagnostic Tick(double DeltaSeconds);
    bool Cancel(uint64_t RunId);
    bool IsRunning() const;
    double GetGameTime() const;
    size_t GetActiveCount() const;
    std::optional<RunSnapshot> GetRun(uint64_t RunId) const;
    Diagnostic ReadVariable(const std::string& Id, ReflectedValue& OutValue) const;
    std::vector<Signal> DrainSignals();

private:
    struct Implementation;
    std::unique_ptr<Implementation> State;
};
}
