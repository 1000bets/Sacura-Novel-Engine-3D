#ifndef SAKURA_MATERIAL_CONTRACT_INCLUDED
#define SAKURA_MATERIAL_CONTRACT_INCLUDED
struct SakuraSurfaceInput
{
    float3 WorldPosition;
    float3 Normal;
    float4 Tangent;
    float2 TextureCoordinates;
    float4 VertexColor;
    float Time;
};

struct SakuraSurface
{
    float3 Albedo;
    float Opacity;
    float3 Normal;
    float Metalness;
    float RoughnessValue;
    float Occlusion;
    float3 Emission;
    float ToonSteps;
    float ToonSmoothness;
    float ToonSpecular;
};

SakuraSurface MakeDefaultSurface(SakuraSurfaceInput Input)
{
    SakuraSurface Surface;
    Surface.Albedo = 1.0;
    Surface.Opacity = 1.0;
    Surface.Normal = Input.Normal;
    Surface.Metalness = 0.0;
    Surface.RoughnessValue = 0.5;
    Surface.Occlusion = 1.0;
    Surface.Emission = 0.0;
    Surface.ToonSteps = 3.0;
    Surface.ToonSmoothness = 0.05;
    Surface.ToonSpecular = 0.5;
    return Surface;
}

float3 TangentNormalToWorld(SakuraSurfaceInput Input, float3 TextureNormal)
{
    if (abs(Input.Tangent.w) < 0.5)
    {
        return Input.Normal;
    }
    float3 Tangent = Input.Tangent.xyz - Input.Normal * dot(Input.Normal, Input.Tangent.xyz);
    if (dot(Tangent, Tangent) < 0.00000001)
    {
        return Input.Normal;
    }
    Tangent = normalize(Tangent);
    float3 Bitangent = cross(Input.Normal, Tangent) * Input.Tangent.w;
    return normalize(Tangent * TextureNormal.x + Bitangent * TextureNormal.y + Input.Normal * TextureNormal.z);
}

struct SakuraVertex
{
    float3 Position;
    float3 Normal;
    float4 Tangent;
    float2 TextureCoordinates;
    float4 VertexColor;
    float Time;
};

struct SakuraPostProcessInput
{
    float4 SceneColor;
    float2 TextureCoordinates;
    float2 Resolution;
    float Time;
    float Depth;
    float3 WorldPosition;
};

struct SakuraUserInterfaceInput
{
    float2 TextureCoordinates;
    float2 Resolution;
    float Time;
    float4 Color;
};

#endif
