cbuffer FrameConstants
{
    float4x4 g_ViewProjection;
};

cbuffer ObjectConstants
{
    float4x4 g_World;
    float4 g_BaseColor;
    float g_UseTexture;
    float3 g_Padding;
};

Texture2D g_BaseColorTexture;
SamplerState g_BaseColorTexture_sampler;

struct VSInput
{
    float3 Pos      : ATTRIB0;
    float3 Normal   : ATTRIB1;
    float2 TexCoord : ATTRIB2;
    float4 Color    : ATTRIB3;
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
    float4 Color    : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn)
{
    float4 WorldPosition = mul(float4(VSIn.Pos, 1.0), g_World);
    PSIn.Pos = mul(WorldPosition, g_ViewProjection);
    PSIn.TexCoord = VSIn.TexCoord;
    PSIn.Color = VSIn.Color;
}

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut)
{
    float4 Sampled = float4(1.0, 1.0, 1.0, 1.0);
    if (g_UseTexture > 0.5)
    {
        Sampled = g_BaseColorTexture.Sample(g_BaseColorTexture_sampler, PSIn.TexCoord);
    }

    // Unlit path: vertex color * material base color * optional base-color texture.
    // LightComponent does not affect this material in v1.
    PSOut.Color = Sampled * g_BaseColor * PSIn.Color;
}
