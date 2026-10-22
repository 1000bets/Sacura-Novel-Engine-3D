#pragma once

#include "Reflection/ReflectedValue.h"

namespace EditorUi
{
inline bool SameInspectorValue(const ReflectedValue& Left, const ReflectedValue& Right)
{
    if (Left.ValueKind != Right.ValueKind || Left.BoolValue != Right.BoolValue
        || Left.Int64Value != Right.Int64Value || Left.Float64Value != Right.Float64Value
        || Left.StringValue != Right.StringValue || Left.TypeIdValue != Right.TypeIdValue
        || Left.ObjectIdValue != Right.ObjectIdValue || Left.BytesValue != Right.BytesValue
        || Left.ObjectValue != Right.ObjectValue || !(Left.AssetValue == Right.AssetValue)
        || Left.FieldNames != Right.FieldNames || Left.Values.size() != Right.Values.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < Left.Values.size(); ++Index)
    {
        if (!SameInspectorValue(Left.Values[Index], Right.Values[Index])) { return false; }
    }
    return true;
}
}
