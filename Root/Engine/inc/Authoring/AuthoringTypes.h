#pragma once

#include "Core/Object/ObjectHandle.h"
#include "Reflection/ReflectedValue.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Authoring
{
struct Diagnostic
{
    bool bOk = true;
    std::string Path;
    std::string Message;

    static Diagnostic Failure(std::string Path, std::string Message);
};

// Uses reflection's existing owning value carrier; no script or UI representation.
struct ValueSchema
{
    ReflectedValue::Kind Kind = ReflectedValue::Kind::Empty;
    std::optional<double> Minimum;
    std::optional<double> Maximum;
};

Diagnostic ValidateSchema(const ValueSchema& Schema, const std::string& Path);
Diagnostic ValidateValue(const ValueSchema& Schema, const ReflectedValue& Value, const std::string& Path);
bool IsNamespacedId(const std::string& Id);

enum class CallSite
{
    Event,
    Function,
    Rule,
    Mechanic,
    Widget
};

struct ParameterDefinition
{
    std::string Id;
    std::string Label;
    ValueSchema Schema;
    std::optional<ReflectedValue> DefaultValue;
};

struct OperationDefinition
{
    std::string Id;
    uint32_t Version = 1;
    std::string Label;
    std::vector<CallSite> Contexts;
    std::vector<ParameterDefinition> Parameters;
    ValueSchema Result;
};

using Arguments = std::map<std::string, ReflectedValue>;

struct OperationCall
{
    std::string Id;
    uint32_t Version = 1;
    CallSite Site = CallSite::Event;
    Arguments Parameters;
};

struct VariableDefinition
{
    std::string Id;
    ValueSchema Schema;
    ReflectedValue InitialValue;
};

struct SignalDefinition
{
    std::string Id;
    uint32_t Version = 1;
    ValueSchema Payload;
};

struct Signal
{
    std::string Id;
    uint32_t Version = 1;
    ReflectedValue Payload;
    ObjectHandle Source;
    double GameTime = 0.0;
};

enum class RunStatus
{
    Queued,
    Running,
    Succeeded,
    Failed,
    Cancelled
};

struct RunSnapshot
{
    uint64_t Id = 0;
    std::string OperationId;
    uint32_t OperationVersion = 1;
    RunStatus Status = RunStatus::Queued;
    ReflectedValue Result;
    Diagnostic Error;
};

struct Submission
{
    uint64_t RunId = 0;
    Diagnostic Error;
};
}
