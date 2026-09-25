#pragma once

#include "Rendering/AxisAlignedBounds.h"

inline bool IntersectsFrustum(const AxisAlignedBounds& Bounds, const Matrix& ViewProjection)
{
    unsigned int CommonOutside = 63;
    for (unsigned int CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
    {
        Vector3 Corner = Bounds.Minimum;
        if ((CornerIndex & 1) != 0)
        {
            Corner.x = Bounds.Maximum.x;
        }
        if ((CornerIndex & 2) != 0)
        {
            Corner.y = Bounds.Maximum.y;
        }
        if ((CornerIndex & 4) != 0)
        {
            Corner.z = Bounds.Maximum.z;
        }
        const Vector4 Clip = Vector4::Transform(Vector4(Corner.x, Corner.y, Corner.z, 1.f), ViewProjection);
        unsigned int Outside = 0;
        if (Clip.x < -Clip.w)
        {
            Outside |= 1;
        }
        if (Clip.x > Clip.w)
        {
            Outside |= 2;
        }
        if (Clip.y < -Clip.w)
        {
            Outside |= 4;
        }
        if (Clip.y > Clip.w)
        {
            Outside |= 8;
        }
        if (Clip.z < 0.f)
        {
            Outside |= 16;
        }
        if (Clip.z > Clip.w)
        {
            Outside |= 32;
        }
        CommonOutside &= Outside;
    }
    return CommonOutside == 0;
}
