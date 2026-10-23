#include "World/Serialization/SceneJson.h"

#include <cmath>

namespace SceneJson
{
nlohmann::json Vector3ToJson(const Vector3& Value)
{
    return nlohmann::json::array({Value.x, Value.y, Value.z});
}

nlohmann::json QuaternionToJson(const Quaternion& Value)
{
    return nlohmann::json::array({Value.x, Value.y, Value.z, Value.w});
}

nlohmann::json TransformToJson(const Transform& Value)
{
    nlohmann::json Document = nlohmann::json::object();
    Document["position"] = Vector3ToJson(Value.Position);
    Document["rotation"] = QuaternionToJson(Value.Rotation);
    Document["scale"] = Vector3ToJson(Value.Scale);
    return Document;
}

bool TryReadVector3(const nlohmann::json& JsonValue, Vector3& OutValue, std::string& OutError)
{
    if (!JsonValue.is_array() || JsonValue.size() != 3)
    {
        OutError = "Vector3 must be an array of 3 numbers";
        return false;
    }
    if (!JsonValue[0].is_number() || !JsonValue[1].is_number() || !JsonValue[2].is_number())
    {
        OutError = "Vector3 elements must be numbers";
        return false;
    }
    OutValue = Vector3(
        JsonValue[0].get<float>(),
        JsonValue[1].get<float>(),
        JsonValue[2].get<float>());
    return true;
}

bool TryReadQuaternion(const nlohmann::json& JsonValue, Quaternion& OutValue, std::string& OutError)
{
    if (!JsonValue.is_array() || JsonValue.size() != 4)
    {
        OutError = "Quaternion must be an array of 4 numbers";
        return false;
    }
    if (!JsonValue[0].is_number() || !JsonValue[1].is_number()
        || !JsonValue[2].is_number() || !JsonValue[3].is_number())
    {
        OutError = "Quaternion elements must be numbers";
        return false;
    }
    OutValue = Quaternion(
        JsonValue[0].get<float>(),
        JsonValue[1].get<float>(),
        JsonValue[2].get<float>(),
        JsonValue[3].get<float>());
    return true;
}

bool TryReadTransform(const nlohmann::json& JsonValue, Transform& OutValue, std::string& OutError)
{
    if (!JsonValue.is_object())
    {
        OutError = "transform must be an object";
        return false;
    }
    if (JsonValue.contains("position") && !TryReadVector3(JsonValue.at("position"), OutValue.Position, OutError))
    {
        return false;
    }
    if (JsonValue.contains("rotation") && !TryReadQuaternion(JsonValue.at("rotation"), OutValue.Rotation, OutError))
    {
        return false;
    }
    if (JsonValue.contains("scale") && !TryReadVector3(JsonValue.at("scale"), OutValue.Scale, OutError))
    {
        return false;
    }
    return true;
}

bool NearlyEqual(const nlohmann::json& Left, const nlohmann::json& Right, double Epsilon)
{
    if (Left.is_number() && Right.is_number())
    {
        return std::abs(Left.get<double>() - Right.get<double>()) <= Epsilon;
    }
    if (Left.type() != Right.type())
    {
        return false;
    }
    if (Left.is_array())
    {
        if (Left.size() != Right.size())
        {
            return false;
        }
        for (size_t Index = 0; Index < Left.size(); ++Index)
        {
            if (!NearlyEqual(Left[Index], Right[Index], Epsilon))
            {
                return false;
            }
        }
        return true;
    }
    if (Left.is_object())
    {
        if (Left.size() != Right.size())
        {
            return false;
        }
        for (auto Iterator = Left.begin(); Iterator != Left.end(); ++Iterator)
        {
            const auto Found = Right.find(Iterator.key());
            if (Found == Right.end() || !NearlyEqual(Iterator.value(), *Found, Epsilon))
            {
                return false;
            }
        }
        return true;
    }
    return Left == Right;
}

bool NearlyEqual(const Vector3& Left, const Vector3& Right, float Epsilon)
{
    return std::abs(Left.x - Right.x) <= Epsilon
        && std::abs(Left.y - Right.y) <= Epsilon
        && std::abs(Left.z - Right.z) <= Epsilon;
}

bool NearlyEqual(const Quaternion& Left, const Quaternion& Right, float Epsilon)
{
    const bool bSame = std::abs(Left.x - Right.x) <= Epsilon
        && std::abs(Left.y - Right.y) <= Epsilon
        && std::abs(Left.z - Right.z) <= Epsilon
        && std::abs(Left.w - Right.w) <= Epsilon;
    if (bSame)
    {
        return true;
    }
    return std::abs(Left.x + Right.x) <= Epsilon
        && std::abs(Left.y + Right.y) <= Epsilon
        && std::abs(Left.z + Right.z) <= Epsilon
        && std::abs(Left.w + Right.w) <= Epsilon;
}
}
