cbuffer GroundGridConstants
{
    float4x4 GridViewProjection;
    float4 GridCameraPosition;
    float4 GridOptions;
};

struct GridOutput
{
    float4 Position : SV_POSITION;
    float2 WorldXZ : TEXCOORD0;
};

GridOutput GridVS(uint VertexIndex : SV_VertexID)
{
    const float2 Corners[6] = {
        float2(-1, -1), float2(-1, 1), float2(1, 1),
        float2(-1, -1), float2(1, 1), float2(1, -1)
    };
    GridOutput Output;
    // The patch follows the camera, but line coordinates stay anchored in world space.
    Output.WorldXZ = GridCameraPosition.xz + Corners[VertexIndex] * 200.0;
    Output.Position = mul(float4(Output.WorldXZ.x, 0.0, Output.WorldXZ.y, 1.0), GridViewProjection);
    if (GridOptions.x > 0.5)
    {
        Output.Position.z = Output.Position.z * 2.0 - Output.Position.w;
    }
    return Output;
}

float GridCoverage(float2 Coordinates)
{
    float2 Footprint = max(fwidth(Coordinates), 0.0001);
    float2 DistanceToLine = abs(frac(Coordinates + 0.5) - 0.5);
    float2 Lines = 1.0 - smoothstep(Footprint * 0.25, Footprint * 1.25, DistanceToLine);
    // Suppress unresolved lines at grazing angles instead of producing horizon moire.
    Lines *= 1.0 - smoothstep(0.25, 0.75, Footprint);
    return max(Lines.x, Lines.y);
}

float4 GridPS(GridOutput Input) : SV_TARGET
{
    float Minor = GridCoverage(Input.WorldXZ);
    float Major = GridCoverage(Input.WorldXZ / 10.0);
    float Distance = length(float3(Input.WorldXZ.x, 0.0, Input.WorldXZ.y) - GridCameraPosition.xyz);
    float Fade = 1.0 - smoothstep(40.0, 160.0, Distance);
    return float4(0.22, 0.24, 0.27, max(Minor * 0.35, Major * 0.5) * Fade);
}
