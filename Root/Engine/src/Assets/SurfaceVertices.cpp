#include "Assets/Resources/StaticMeshResource.h"
#include <cmath>
#include <map>

using namespace DirectX::SimpleMath;

void PrepareSurfaceVertices(StaticMeshResource& Mesh)
{
    std::map<std::pair<uint32_t, int>, uint32_t> SplitVertices;
    for (size_t Triangle = 0; Triangle + 2 < Mesh.Indices.size(); Triangle += 3)
    {
        const auto& First = Mesh.Vertices[Mesh.Indices[Triangle]];
        const auto& Second = Mesh.Vertices[Mesh.Indices[Triangle + 1]];
        const auto& Third = Mesh.Vertices[Mesh.Indices[Triangle + 2]];
        const Vector2 FirstCoordinates = Second.TexCoord - First.TexCoord;
        const Vector2 SecondCoordinates = Third.TexCoord - First.TexCoord;
        const float Determinant = FirstCoordinates.x * SecondCoordinates.y - FirstCoordinates.y * SecondCoordinates.x;
        int Sign = 1;
        if (Determinant < 0.f)
        {
            Sign = -1;
        }
        for (size_t Corner = 0; Corner < 3; ++Corner)
        {
            const uint32_t Original = Mesh.Indices[Triangle + Corner];
            if (Mesh.Vertices[Original].bHasTangent)
            {
                continue;
            }
            const auto Key = std::make_pair(Original, Sign);
            if (SplitVertices.find(Key) == SplitVertices.end())
            {
                uint32_t Destination = Original;
                if (SplitVertices.find(std::make_pair(Original, -Sign)) != SplitVertices.end())
                {
                    Destination = static_cast<uint32_t>(Mesh.Vertices.size());
                    const StaticMeshVertex Copy = Mesh.Vertices[Original];
                    Mesh.Vertices.push_back(Copy);
                }
                SplitVertices.emplace(Key, Destination);
            }
            Mesh.Indices[Triangle + Corner] = SplitVertices.at(Key);
        }
    }
    std::vector<Vector3> Normals(Mesh.Vertices.size(), Vector3::Zero);
    std::vector<Vector3> Tangents(Mesh.Vertices.size(), Vector3::Zero);
    std::vector<Vector3> Bitangents(Mesh.Vertices.size(), Vector3::Zero);
    std::vector<bool> Degenerate(Mesh.Vertices.size(), false);
    for (size_t Triangle = 0; Triangle + 2 < Mesh.Indices.size(); Triangle += 3)
    {
        const uint32_t Indices[] = {Mesh.Indices[Triangle], Mesh.Indices[Triangle + 1], Mesh.Indices[Triangle + 2]};
        const auto& First = Mesh.Vertices[Indices[0]];
        const auto& Second = Mesh.Vertices[Indices[1]];
        const auto& Third = Mesh.Vertices[Indices[2]];
        const Vector3 FirstEdge = Second.Position - First.Position;
        const Vector3 SecondEdge = Third.Position - First.Position;
        const Vector3 Normal = FirstEdge.Cross(SecondEdge);
        const Vector2 FirstCoordinates = Second.TexCoord - First.TexCoord;
        const Vector2 SecondCoordinates = Third.TexCoord - First.TexCoord;
        const float Determinant = FirstCoordinates.x * SecondCoordinates.y - FirstCoordinates.y * SecondCoordinates.x;
        const bool bValidCoordinates = First.bHasTexCoord && Second.bHasTexCoord && Third.bHasTexCoord
            && std::isfinite(Determinant) && std::abs(Determinant) > 0.00000001f;
        for (uint32_t Index : Indices)
        {
            Normals[Index] += Normal;
            if (bValidCoordinates)
            {
                Tangents[Index] += (FirstEdge * SecondCoordinates.y - SecondEdge * FirstCoordinates.y) / Determinant;
                Bitangents[Index] += (SecondEdge * FirstCoordinates.x - FirstEdge * SecondCoordinates.x) / Determinant;
            }
            else
            {
                Degenerate[Index] = true;
            }
        }
    }
    for (size_t Index = 0; Index < Mesh.Vertices.size(); ++Index)
    {
        auto& Vertex = Mesh.Vertices[Index];
        if (!Vertex.bHasNormal)
        {
            Vertex.Normal = Normals[Index];
        }
        if (!std::isfinite(Vertex.Normal.LengthSquared()) || Vertex.Normal.LengthSquared() < 0.00000001f)
        {
            Vertex.Normal = Vector3::Up;
        }
        Vertex.Normal.Normalize();
        Vertex.bHasNormal = true;
        if (Vertex.bHasTangent)
        {
            Vector3 Imported = Vector3(Vertex.Tangent.x, Vertex.Tangent.y, Vertex.Tangent.z);
            Imported -= Vertex.Normal * Vertex.Normal.Dot(Imported);
            if (Degenerate[Index] || !std::isfinite(Imported.LengthSquared()) || Imported.LengthSquared() < 0.00000001f
                || !std::isfinite(Vertex.Tangent.w) || std::abs(Vertex.Tangent.w) < 0.5f)
            {
                Vertex.Tangent = Vector4::Zero;
                Vertex.bHasTangent = false;
            }
            else
            {
                Imported.Normalize();
                Vertex.Tangent = Vector4(Imported.x, Imported.y, Imported.z, std::copysign(1.f, Vertex.Tangent.w));
            }
        }
        if (!Vertex.bHasTangent)
        {
            Vector3 Tangent = Tangents[Index] - Vertex.Normal * Vertex.Normal.Dot(Tangents[Index]);
            if (!Degenerate[Index] && Tangent.LengthSquared() > 0.00000001f)
            {
                Tangent.Normalize();
                float Sign = 1.f;
                if (Vertex.Normal.Cross(Tangent).Dot(Bitangents[Index]) < 0.f)
                {
                    Sign = -1.f;
                }
                Vertex.Tangent = {Tangent.x, Tangent.y, Tangent.z, Sign};
                Vertex.bHasTangent = true;
            }
            else
            {
                Vertex.Tangent = Vector4::Zero;
            }
        }
    }
}
