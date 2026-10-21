#include "Authoring/AuthoringTypes.h"

#include <cmath>
#include <regex>
#include <utility>

namespace Authoring
{
Diagnostic Diagnostic::Failure(std::string Path, std::string Message)
{
    return {false, std::move(Path), std::move(Message)};
}

bool IsNamespacedId(const std::string& Id)
{
    static const std::regex Pattern("[a-z][a-z0-9-]*(\\.[a-z][a-z0-9-]*)+(\\.[a-z0-9-]+)*");
    return Id.size() <= 256 && std::regex_match(Id, Pattern);
}

Diagnostic ValidateSchema(const ValueSchema& Schema, const std::string& Path)
{
    using Kind = ReflectedValue::Kind;
    switch (Schema.Kind)
    {
    case Kind::Empty:
    case Kind::Bool:
    case Kind::Int64:
    case Kind::Float64:
    case Kind::String:
        break;
    default:
        return Diagnostic::Failure(Path, "Unsupported authoring value kind");
    }
    if ((Schema.Minimum && !std::isfinite(*Schema.Minimum))
        || (Schema.Maximum && !std::isfinite(*Schema.Maximum))
        || (Schema.Minimum && Schema.Maximum && *Schema.Minimum > *Schema.Maximum))
    {
        return Diagnostic::Failure(Path, "Invalid numeric range");
    }
    if ((Schema.Minimum || Schema.Maximum) && Schema.Kind != Kind::Float64 && Schema.Kind != Kind::Int64)
    {
        return Diagnostic::Failure(Path, "Only numeric values may declare a range");
    }
    return {};
}

Diagnostic ValidateValue(const ValueSchema& Schema, const ReflectedValue& Value, const std::string& Path)
{
    const Diagnostic Shape = ValidateSchema(Schema, Path);
    if (!Shape.bOk)
    {
        return Shape;
    }
    if (Value.ValueKind != Schema.Kind)
    {
        return Diagnostic::Failure(Path, "Value does not match its declared type");
    }
    if (Schema.Kind == ReflectedValue::Kind::Float64 || Schema.Kind == ReflectedValue::Kind::Int64)
    {
        const long double Number = Schema.Kind == ReflectedValue::Kind::Float64
            ? static_cast<long double>(Value.Float64Value) : static_cast<long double>(Value.Int64Value);
        if (!std::isfinite(Number) || (Schema.Minimum && Number < *Schema.Minimum)
            || (Schema.Maximum && Number > *Schema.Maximum))
        {
            return Diagnostic::Failure(Path, "Value is non-finite or outside its declared range");
        }
    }
    return {};
}
}
