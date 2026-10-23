cbuffer SkyConstants
{
    float4x4 InverseViewProjection;
    float4 CameraPosition;
    float4 Zenith;
    float4 Horizon;
    float4 Ground;
    float4 SunColor;
    float4 SunDirection;
};
struct SkyOutput
{
    float4 Position : SV_POSITION;
    float2 Clip : TEXCOORD0;
};
SkyOutput SkyVS(uint VertexIndex : SV_VertexID)
{
    float2 Corner = float2((VertexIndex << 1) & 2, VertexIndex & 2);
    SkyOutput Output;
    Output.Clip = Corner * 2.0 - 1.0;
    Output.Position = float4(Output.Clip, 1.0, 1.0);
    return Output;
}
float4 SkyPS(SkyOutput Input) : SV_TARGET
{
    float4 World = mul(float4(Input.Clip, 1.0, 1.0), InverseViewProjection);
    float3 Direction = normalize(World.xyz / World.w - CameraPosition.xyz);
    float3 Color = lerp(Horizon.rgb, Zenith.rgb, sqrt(max(0.0, Direction.y)));
    if (Direction.y < 0.0)
    {
        Color = lerp(Horizon.rgb, Ground.rgb, sqrt(-Direction.y));
    }
    Color += SunColor.rgb * pow(max(0.0, dot(Direction, SunDirection.xyz)), 512.0);
    return float4(Color * Zenith.a, 1.0);
}
