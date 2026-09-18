#pragma once

#include <SimpleMath.h>

#include <cfloat>

using namespace DirectX::SimpleMath;

struct AxisAlignedBounds
{
    Vector3 Minimum = Vector3::Zero;
    Vector3 Maximum = Vector3::Zero;

    static AxisAlignedBounds FromCenterExtents(const Vector3& Center, const Vector3& Extents)
    {
        AxisAlignedBounds Bounds;
        Bounds.Minimum = Center - Extents;
        Bounds.Maximum = Center + Extents;
        return Bounds;
    }

    AxisAlignedBounds TransformedBy(const Matrix& WorldMatrix) const
    {
        const Vector3 Corners[8] = {
            Vector3(Minimum.x, Minimum.y, Minimum.z),
            Vector3(Maximum.x, Minimum.y, Minimum.z),
            Vector3(Minimum.x, Maximum.y, Minimum.z),
            Vector3(Maximum.x, Maximum.y, Minimum.z),
            Vector3(Minimum.x, Minimum.y, Maximum.z),
            Vector3(Maximum.x, Minimum.y, Maximum.z),
            Vector3(Minimum.x, Maximum.y, Maximum.z),
            Vector3(Maximum.x, Maximum.y, Maximum.z),
        };

        AxisAlignedBounds Result;
        Result.Minimum = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
        Result.Maximum = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        for (const Vector3& Corner : Corners)
        {
            const Vector3 Transformed = Vector3::Transform(Corner, WorldMatrix);
            Result.Minimum = Vector3::Min(Result.Minimum, Transformed);
            Result.Maximum = Vector3::Max(Result.Maximum, Transformed);
        }

        return Result;
    }
};
