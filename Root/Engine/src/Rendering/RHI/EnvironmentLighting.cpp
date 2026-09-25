#include "Rendering/RHI/EnvironmentLighting.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include <SimpleMath.h>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace Diligent;
using namespace DirectX::SimpleMath;

static Vector2 SampleSequence(uint32_t Index, uint32_t Count)
{
    uint32_t Bits = (Index << 16u) | (Index >> 16u);
    Bits = ((Bits & 0x55555555u) << 1u) | ((Bits & 0xAAAAAAAAu) >> 1u);
    Bits = ((Bits & 0x33333333u) << 2u) | ((Bits & 0xCCCCCCCCu) >> 2u);
    Bits = ((Bits & 0x0F0F0F0Fu) << 4u) | ((Bits & 0xF0F0F0F0u) >> 4u);
    Bits = ((Bits & 0x00FF00FFu) << 8u) | ((Bits & 0xFF00FF00u) >> 8u);
    return Vector2(static_cast<float>(Index) / static_cast<float>(Count), static_cast<float>(Bits) * 2.3283064365386963e-10f);
}

static Vector3 SampleHemisphere(const Vector3& Normal, float Azimuth, float Cosine)
{
    Vector3 Auxiliary = Vector3::Up;
    if (std::abs(Normal.y) > 0.99f)
    {
        Auxiliary = Vector3::Right;
    }
    Vector3 Tangent = Auxiliary.Cross(Normal);
    Tangent.Normalize();
    const Vector3 Bitangent = Normal.Cross(Tangent);
    const float Sine = std::sqrt(std::max(0.f, 1.f - Cosine * Cosine));
    return Tangent * (std::cos(Azimuth) * Sine) + Bitangent * (std::sin(Azimuth) * Sine) + Normal * Cosine;
}

static Vector3 CubeDirection(uint32_t Face, float Horizontal, float Vertical)
{
    Vector3 Direction;
    switch (Face)
    {
    case 0: Direction = Vector3(1.f, -Vertical, -Horizontal); break;
    case 1: Direction = Vector3(-1.f, -Vertical, Horizontal); break;
    case 2: Direction = Vector3(Horizontal, 1.f, Vertical); break;
    case 3: Direction = Vector3(Horizontal, -1.f, -Vertical); break;
    case 4: Direction = Vector3(Horizontal, -Vertical, 1.f); break;
    default: Direction = Vector3(-Horizontal, -Vertical, -1.f); break;
    }
    Direction.Normalize();
    return Direction;
}

static Vector3 SkyRadiance(const Vector3& Direction)
{
    const float Blend = std::clamp(Direction.y * 0.5f + 0.5f, 0.f, 1.f);
    return Vector3::Lerp(Vector3(0.07f, 0.06f, 0.05f), Vector3(0.35f, 0.55f, 1.1f), Blend)
        + Vector3(0.4f, 0.3f, 0.2f) * std::pow(std::max(0.f, Direction.Dot(Vector3(0.57735f, 0.57735f, 0.57735f))), 32.f);
}

static bool BakeEnvironmentCube(IRenderDevice* Device, bool bDiffuse, RefCntAutoPtr<ITexture>& Texture)
{
    uint32_t Size = 32;
    uint32_t MipCount = 6;
    if (bDiffuse)
    {
        Size = 16;
        MipCount = 1;
    }
    std::vector<std::vector<Vector4>> Images(6 * MipCount);
    std::vector<TextureSubResData> Subresources(6 * MipCount);
    for (uint32_t Face = 0; Face < 6; ++Face)
    {
        for (uint32_t Mip = 0; Mip < MipCount; ++Mip)
        {
            const uint32_t Extent = std::max(1u, Size >> Mip);
            auto& Pixels = Images[Face * MipCount + Mip];
            Pixels.resize(Extent * Extent);
            const float Roughness = static_cast<float>(Mip) / 5.f;
            for (uint32_t Row = 0; Row < Extent; ++Row)
            {
                for (uint32_t Column = 0; Column < Extent; ++Column)
                {
                    const Vector3 Normal = CubeDirection(Face, (static_cast<float>(Column) + 0.5f) * 2.f / Extent - 1.f,
                        (static_cast<float>(Row) + 0.5f) * 2.f / Extent - 1.f);
                    Vector3 Sum = Vector3::Zero;
                    float Weight = 0.f;
                    for (uint32_t SampleIndex = 0; SampleIndex < 128; ++SampleIndex)
                    {
                        const Vector2 Sequence = SampleSequence(SampleIndex, 128);
                        Vector3 Direction;
                        float Contribution = 1.f;
                        if (bDiffuse)
                        {
                            Direction = SampleHemisphere(Normal, 6.2831853f * Sequence.x, std::sqrt(1.f - Sequence.y));
                        }
                        else
                        {
                            const float AlphaSquared = std::max(0.000001f, std::pow(Roughness, 4.f));
                            const Vector3 Halfway = SampleHemisphere(Normal, 6.2831853f * Sequence.x,
                                std::sqrt((1.f - Sequence.y) / (1.f + (AlphaSquared - 1.f) * Sequence.y)));
                            Direction = Halfway * (2.f * Normal.Dot(Halfway)) - Normal;
                            Contribution = std::max(0.f, Normal.Dot(Direction));
                        }
                        Sum += SkyRadiance(Direction) * Contribution;
                        Weight += Contribution;
                    }
                    Sum /= std::max(Weight, 0.00001f);
                    Pixels[Row * Extent + Column] = Vector4(Sum.x, Sum.y, Sum.z, 1.f);
                }
            }
            auto& Subresource = Subresources[Face * MipCount + Mip];
            Subresource.pData = Pixels.data();
            Subresource.Stride = Extent * sizeof(Vector4);
        }
    }
    TextureDesc Description;
    Description.Name = "Baked environment lighting";
    Description.Type = RESOURCE_DIM_TEX_CUBE;
    Description.Width = Size;
    Description.Height = Size;
    Description.ArraySize = 6;
    Description.MipLevels = MipCount;
    Description.Format = TEX_FORMAT_RGBA32_FLOAT;
    Description.BindFlags = BIND_SHADER_RESOURCE;
    Description.Usage = USAGE_IMMUTABLE;
    TextureData Data(Subresources.data(), static_cast<uint32_t>(Subresources.size()));
    Device->CreateTexture(Description, &Data, &Texture);
    return Texture != nullptr;
}

bool EnvironmentLighting::Initialize(IRenderDevice* Device)
{
    if (!BakeEnvironmentCube(Device, true, Irradiance) || !BakeEnvironmentCube(Device, false, Reflection))
    {
        return false;
    }
    constexpr uint32_t Size = 64;
    std::vector<Vector2> Pixels(Size * Size);
    for (uint32_t Row = 0; Row < Size; ++Row)
    {
        const float Roughness = (static_cast<float>(Row) + 0.5f) / Size;
        const float GeometryFactor = Roughness * Roughness * 0.5f;
        for (uint32_t Column = 0; Column < Size; ++Column)
        {
            const float NormalView = (static_cast<float>(Column) + 0.5f) / Size;
            const Vector3 View(std::sqrt(1.f - NormalView * NormalView), 0.f, NormalView);
            Vector2 Sum = Vector2::Zero;
            for (uint32_t SampleIndex = 0; SampleIndex < 256; ++SampleIndex)
            {
                const Vector2 Sequence = SampleSequence(SampleIndex, 256);
                const float Cosine = std::sqrt((1.f - Sequence.y) / (1.f + (std::pow(Roughness, 4.f) - 1.f) * Sequence.y));
                const float Sine = std::sqrt(std::max(0.f, 1.f - Cosine * Cosine));
                const Vector3 Halfway(std::cos(6.2831853f * Sequence.x) * Sine, std::sin(6.2831853f * Sequence.x) * Sine, Cosine);
                const float ViewHalf = std::max(0.f, View.Dot(Halfway));
                const Vector3 Light = Halfway * (2.f * ViewHalf) - View;
                if (Light.z > 0.f)
                {
                    const float Geometry = NormalView / (NormalView * (1.f - GeometryFactor) + GeometryFactor)
                        * Light.z / (Light.z * (1.f - GeometryFactor) + GeometryFactor);
                    const float Visibility = Geometry * ViewHalf / std::max(Cosine * NormalView, 0.00001f);
                    const float Fresnel = std::pow(1.f - ViewHalf, 5.f);
                    Sum += Vector2((1.f - Fresnel) * Visibility, Fresnel * Visibility);
                }
            }
            Pixels[Row * Size + Column] = Sum / 256.f;
        }
    }
    TextureDesc Description;
    Description.Name = "Integrated GGX BRDF";
    Description.Type = RESOURCE_DIM_TEX_2D;
    Description.Width = Size;
    Description.Height = Size;
    Description.Format = TEX_FORMAT_RG32_FLOAT;
    Description.BindFlags = BIND_SHADER_RESOURCE;
    Description.Usage = USAGE_IMMUTABLE;
    TextureSubResData Subresource(Pixels.data(), Size * sizeof(Vector2));
    TextureData Data(&Subresource, 1);
    Device->CreateTexture(Description, &Data, &IntegratedBrdf);
    return IntegratedBrdf != nullptr;
}
