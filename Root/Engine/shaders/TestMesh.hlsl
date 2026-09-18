cbuffer FrameConstants
{
    float4x4 g_ViewProjection;
};

cbuffer ObjectConstants
{
    float4x4 g_World;
};

struct VSInput
{
    float3 Pos   : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
};

void VSMain(in VSInput VSIn, out PSInput PSIn)
{
    float4 WorldPosition = mul(float4(VSIn.Pos, 1.0), g_World);
    PSIn.Pos = mul(WorldPosition, g_ViewProjection);
    PSIn.Color = VSIn.Color;
}

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void PSMain(in PSInput PSIn, out PSOutput PSOut)
{
    PSOut.Color = PSIn.Color;
}
