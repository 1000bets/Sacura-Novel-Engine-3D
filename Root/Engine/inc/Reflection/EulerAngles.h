#pragma once

#include <SimpleMath.h>

struct EulerAngles
{
    float Pitch = 0.f;
    float Yaw = 0.f;
    float Roll = 0.f;

    DirectX::SimpleMath::Quaternion ToQuaternion() const
    {
        return DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(
            Yaw * 0.017453292519943295f,
            Pitch * 0.017453292519943295f,
            Roll * 0.017453292519943295f);
    }

    static EulerAngles FromQuaternion(const DirectX::SimpleMath::Quaternion& Rotation)
    {
        const DirectX::SimpleMath::Vector3 Angles = Rotation.ToEuler();
        return {Angles.x * 57.29577951308232f, Angles.y * 57.29577951308232f, Angles.z * 57.29577951308232f};
    }

    bool operator==(const EulerAngles& Other) const
    {
        return Pitch == Other.Pitch && Yaw == Other.Yaw && Roll == Other.Roll;
    }
};
