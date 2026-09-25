cbuffer PostConstants
{
    float4 Parameters;
    float4 Resolution;
};
Texture2D SceneImage;
SamplerState SceneImage_sampler;
Texture2D AccumulationImage;
SamplerState AccumulationImage_sampler;
Texture2D RevealageImage;
SamplerState RevealageImage_sampler;

struct FullscreenOutput
{
    float4 Position : SV_POSITION;
    float2 Coordinates : TEX_COORD;
};

FullscreenOutput FullscreenMain(uint VertexIndex : SV_VertexID)
{
    FullscreenOutput Output;
    float2 Coordinates = float2((VertexIndex << 1) & 2, VertexIndex & 2);
    Output.Position = float4(Coordinates * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    Output.Coordinates = Coordinates;
    if (Resolution.z > 0.5)
    {
        Output.Position.y = -Output.Position.y;
    }
    return Output;
}

float3 Composite(float2 Coordinates)
{
    float3 Scene = SceneImage.SampleLevel(SceneImage_sampler, Coordinates, 0).rgb;
    float4 Accumulation = AccumulationImage.SampleLevel(AccumulationImage_sampler, Coordinates, 0);
    float Revealage = RevealageImage.SampleLevel(RevealageImage_sampler, Coordinates, 0).r;
    return Scene * Revealage + Accumulation.rgb / max(Accumulation.a, 0.0001) * (1.0 - Revealage);
}

float4 ToneMapMain(FullscreenOutput Input) : SV_TARGET
{
    float3 Radiance = Composite(Input.Coordinates);
    float3 Bloom = 0.0;
    for (int Row = -2; Row <= 2; ++Row)
    {
        for (int Column = -2; Column <= 2; ++Column)
        {
            Bloom += max(Composite(Input.Coordinates + float2(Column, Row) * Resolution.xy * 2.0) - Parameters.z, 0.0);
        }
    }
    Radiance = (Radiance + Bloom * Parameters.y / 25.0) * Parameters.x;
    float3 Mapped = saturate((Radiance * (2.51 * Radiance + 0.03)) / (Radiance * (2.43 * Radiance + 0.59) + 0.14));
    return float4(Mapped, 1.0);
}

float Luminance(float3 Color)
{
    return dot(Color, float3(0.299, 0.587, 0.114));
}

float4 AntialiasMain(FullscreenOutput Input) : SV_TARGET
{
    float3 Center = SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates, 0).rgb;
    if (Parameters.w < 0.5)
    {
        return float4(Center, 1.0);
    }
    float Northwest = Luminance(SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates + float2(-1.0, -1.0) * Resolution.xy, 0).rgb);
    float Northeast = Luminance(SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates + float2(1.0, -1.0) * Resolution.xy, 0).rgb);
    float Southwest = Luminance(SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates + float2(-1.0, 1.0) * Resolution.xy, 0).rgb);
    float Southeast = Luminance(SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates + float2(1.0, 1.0) * Resolution.xy, 0).rgb);
    float Middle = Luminance(Center);
    float Minimum = min(Middle, min(min(Northwest, Northeast), min(Southwest, Southeast)));
    float Maximum = max(Middle, max(max(Northwest, Northeast), max(Southwest, Southeast)));
    if (Maximum - Minimum < max(0.0312, Maximum * 0.125))
    {
        return float4(Center, 1.0);
    }
    float2 Direction = float2(-((Northwest + Northeast) - (Southwest + Southeast)), (Northwest + Southwest) - (Northeast + Southeast));
    float Reduction = max((Northwest + Northeast + Southwest + Southeast) * 0.03125, 0.0078125);
    Direction = clamp(Direction / (min(abs(Direction.x), abs(Direction.y)) + Reduction), -8.0, 8.0) * Resolution.xy;
    float3 Inner = 0.5 * (SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates + Direction * (-1.0 / 6.0), 0).rgb
        + SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates + Direction * (1.0 / 6.0), 0).rgb);
    float3 Outer = Inner * 0.5 + 0.25 * (SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates - Direction * 0.5, 0).rgb
        + SceneImage.SampleLevel(SceneImage_sampler, Input.Coordinates + Direction * 0.5, 0).rgb);
    float OuterLuminance = Luminance(Outer);
    if (OuterLuminance < Minimum || OuterLuminance > Maximum)
    {
        return float4(Inner, 1.0);
    }
    return float4(Outer, 1.0);
}
