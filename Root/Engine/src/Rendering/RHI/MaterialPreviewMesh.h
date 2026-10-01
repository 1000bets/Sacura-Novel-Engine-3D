#pragma once
#include "Assets/Resources/StaticMeshResource.h"
#include <cmath>

inline StaticMeshResource BuildMaterialPreviewMesh(int32_t Shape)
{
    StaticMeshResource Mesh;
    if (Shape == 0)
    {
        constexpr uint32_t LongitudeCount = 32;
        constexpr uint32_t LatitudeCount = 16;
        for (uint32_t Latitude = 0; Latitude <= LatitudeCount; ++Latitude)
        {
            const float Vertical = static_cast<float>(Latitude) / LatitudeCount;
            for (uint32_t Longitude = 0; Longitude <= LongitudeCount; ++Longitude)
            {
                const float Horizontal = static_cast<float>(Longitude) / LongitudeCount;
                StaticMeshVertex Vertex;
                Vertex.Position = Vector3(std::sin(Vertical * 3.14159265f) * std::cos(Horizontal * 6.2831853f),
                    std::cos(Vertical * 3.14159265f), std::sin(Vertical * 3.14159265f) * std::sin(Horizontal * 6.2831853f));
                Vertex.Normal = Vertex.Position;
                Vertex.TexCoord = Vector2(Horizontal, Vertical);
                Vertex.bHasNormal = true;
                Vertex.bHasTexCoord = true;
                Mesh.Vertices.push_back(Vertex);
                if (Longitude < LongitudeCount && Latitude < LatitudeCount)
                {
                    const uint32_t First = Latitude * (LongitudeCount + 1) + Longitude;
                    Mesh.Indices.insert(Mesh.Indices.end(), {First, First + 1, First + LongitudeCount + 1,
                        First + 1, First + LongitudeCount + 2, First + LongitudeCount + 1});
                }
            }
        }
    }
    else
    {
        const Vector3 Normals[] = {Vector3::Backward, Vector3::Forward, Vector3::Right,
            Vector3::Left, Vector3::Up, Vector3::Down};
        uint32_t FaceCount = 6;
        if (Shape == 2)
        {
            FaceCount = 1;
        }
        for (uint32_t Face = 0; Face < FaceCount; ++Face)
        {
            const Vector3 Normal = Normals[Face];
            Vector3 Horizontal = Normal.Cross(Vector3::Up);
            if (Face >= 4)
            {
                Horizontal = Vector3::Right;
            }
            Horizontal.Normalize();
            const Vector3 Vertical = Normal.Cross(Horizontal);
            const uint32_t First = static_cast<uint32_t>(Mesh.Vertices.size());
            const Vector2 Coordinates[] = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}};
            for (const auto& CoordinatesValue : Coordinates)
            {
                StaticMeshVertex Vertex;
                Vertex.Position = Horizontal * (CoordinatesValue.x * 2.f - 1.f)
                    + Vertical * (CoordinatesValue.y * 2.f - 1.f);
                if (Shape != 2)
                {
                    Vertex.Position += Normal;
                }
                Vertex.Normal = Normal;
                Vertex.TexCoord = CoordinatesValue;
                Vertex.bHasNormal = true;
                Vertex.bHasTexCoord = true;
                Mesh.Vertices.push_back(Vertex);
            }
            Mesh.Indices.insert(Mesh.Indices.end(), {First, First + 1, First + 2, First, First + 2, First + 3});
        }
    }
    Mesh.Bounds = AxisAlignedBounds::FromCenterExtents(Vector3::Zero, Vector3::One);
    PrepareSurfaceVertices(Mesh);
    return Mesh;
}
