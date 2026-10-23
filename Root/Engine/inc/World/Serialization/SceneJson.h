#pragma once

#include "Core/Transform.h"

#include <nlohmann/json.hpp>

#include <string>

// Small JSON helpers shared by SceneSerializer and the prefab system.
// All scene/prefab documents store vectors as [x, y, z] and quaternions as [x, y, z, w].
namespace SceneJson
{
nlohmann::json Vector3ToJson(const Vector3& Value);
nlohmann::json QuaternionToJson(const Quaternion& Value);
nlohmann::json TransformToJson(const Transform& Value);

bool TryReadVector3(const nlohmann::json& JsonValue, Vector3& OutValue, std::string& OutError);
bool TryReadQuaternion(const nlohmann::json& JsonValue, Quaternion& OutValue, std::string& OutError);
bool TryReadTransform(const nlohmann::json& JsonValue, Transform& OutValue, std::string& OutError);

// Tolerant structural comparison: numbers are compared with an absolute epsilon so that
// float -> double -> text round trips do not produce phantom prefab overrides.
bool NearlyEqual(const nlohmann::json& Left, const nlohmann::json& Right, double Epsilon = 1.0e-5);

bool NearlyEqual(const Vector3& Left, const Vector3& Right, float Epsilon = 1.0e-5f);
// q and -q describe the same rotation.
bool NearlyEqual(const Quaternion& Left, const Quaternion& Right, float Epsilon = 1.0e-5f);
}
