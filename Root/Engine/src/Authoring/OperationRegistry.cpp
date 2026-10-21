#include "Authoring/OperationRegistry.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>
#include <set>
#include <utility>

namespace Authoring
{
TaskResult TaskResult::Complete(ReflectedValue Value)
{
    return {RunStatus::Succeeded, std::move(Value), {}};
}

TaskResult TaskResult::Fail(std::string Message)
{
    return {RunStatus::Failed, {}, Diagnostic::Failure("execution", std::move(Message))};
}

Diagnostic OperationRegistry::Register(OperationDefinition Definition, TaskFactory Factory)
{
    AssertGameThread();
    if (!IsNamespacedId(Definition.Id) || Definition.Version == 0 || Definition.Label.empty() || !Factory)
    {
        return Diagnostic::Failure("operation", "Operation requires a namespaced ID, positive version, label and factory");
    }
    if (Entries.count({Definition.Id, Definition.Version}) != 0)
    {
        return Diagnostic::Failure("operation.id", "Operation ID and version are already installed");
    }
    std::set<CallSite> Sites;
    for (const CallSite Site : Definition.Contexts)
    {
        if (Site < CallSite::Event || Site > CallSite::Widget || !Sites.insert(Site).second)
        {
            return Diagnostic::Failure("operation.contexts", "Invalid or repeated calling context");
        }
    }
    if (Sites.empty())
    {
        return Diagnostic::Failure("operation.contexts", "At least one calling context is required");
    }
    const Diagnostic ResultSchema = ValidateSchema(Definition.Result, "operation.result");
    if (!ResultSchema.bOk)
    {
        return ResultSchema;
    }
    std::set<std::string> Names;
    for (const ParameterDefinition& Parameter : Definition.Parameters)
    {
        const std::string Path = "parameters." + Parameter.Id;
        if (Parameter.Id.empty() || Parameter.Label.empty() || !Names.insert(Parameter.Id).second
            || Parameter.Schema.Kind == ReflectedValue::Kind::Empty)
        {
            return Diagnostic::Failure(Path, "Parameter requires a unique name, label and non-void schema");
        }
        const Diagnostic Valid = Parameter.DefaultValue
            ? ValidateValue(Parameter.Schema, *Parameter.DefaultValue, Path)
            : ValidateSchema(Parameter.Schema, Path);
        if (!Valid.bOk)
        {
            return Valid;
        }
    }
    const Key Identity{Definition.Id, Definition.Version};
    Entries.emplace(Identity, std::make_shared<const RegisteredOperation>(
        RegisteredOperation{std::move(Definition), std::move(Factory)}));
    return {};
}

std::shared_ptr<const RegisteredOperation> OperationRegistry::Find(const std::string& Id, uint32_t Version) const
{
    AssertGameThread();
    const auto Found = Entries.find({Id, Version});
    return Found == Entries.end() ? nullptr : Found->second;
}

std::vector<OperationDefinition> OperationRegistry::GetDefinitions() const
{
    AssertGameThread();
    std::vector<OperationDefinition> Definitions;
    Definitions.reserve(Entries.size());
    for (const auto& Entry : Entries)
    {
        Definitions.push_back(Entry.second->Definition);
    }
    return Definitions;
}

Diagnostic OperationRegistry::Prepare(const OperationCall& Call, Arguments& OutParameters) const
{
    AssertGameThread();
    const auto Entry = Find(Call.Id, Call.Version);
    if (!Entry)
    {
        return Diagnostic::Failure("operation.id", "Operation or version is not installed: " + Call.Id);
    }
    const auto& Definition = Entry->Definition;
    if (std::find(Definition.Contexts.begin(), Definition.Contexts.end(), Call.Site) == Definition.Contexts.end())
    {
        return Diagnostic::Failure("operation.context", "Operation is unavailable in this calling context");
    }
    Arguments Candidate;
    for (const ParameterDefinition& Parameter : Definition.Parameters)
    {
        const auto Supplied = Call.Parameters.find(Parameter.Id);
        if (Supplied == Call.Parameters.end() && !Parameter.DefaultValue)
        {
            return Diagnostic::Failure("parameters." + Parameter.Id, "Required parameter is not assigned");
        }
        const ReflectedValue& Value = Supplied == Call.Parameters.end() ? *Parameter.DefaultValue : Supplied->second;
        const Diagnostic Valid = ValidateValue(Parameter.Schema, Value, "parameters." + Parameter.Id);
        if (!Valid.bOk)
        {
            return Valid;
        }
        Candidate.emplace(Parameter.Id, Value);
    }
    for (const auto& Parameter : Call.Parameters)
    {
        if (Candidate.count(Parameter.first) == 0)
        {
            return Diagnostic::Failure("parameters." + Parameter.first, "Unknown parameter");
        }
    }
    OutParameters = std::move(Candidate);
    return {};
}
}
