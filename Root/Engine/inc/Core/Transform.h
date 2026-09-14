#pragma once

#include <SimpleMath.h>

using namespace DirectX::SimpleMath;

struct Transform
{
    Vector3 Position;
    Quaternion Rotation = Quaternion::Identity;
    Vector3 Scale = Vector3::One;

    Matrix GetMatrix() const
    {
        return Matrix::CreateScale(Scale)
             * Matrix::CreateFromQuaternion(Rotation)
             * Matrix::CreateTranslation(Position);
    }

    Vector3 GetForward() const { return Vector3::Transform(Vector3::Forward, Rotation); }
    Vector3 GetRight() const { return Vector3::Transform(Vector3::Right, Rotation); }
    Vector3 GetUp() const { return Vector3::Transform(Vector3::Up, Rotation); }
};
