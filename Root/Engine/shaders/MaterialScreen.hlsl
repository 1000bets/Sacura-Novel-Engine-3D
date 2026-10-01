cbuffer SakuraMaterialView
{
    float4x4 InverseViewProjection;
    float4 ResolutionTime;
    float4 WidgetColor;
    float4 ViewOptions;
};
Texture2D SceneImage;
SamplerState SceneImage_sampler;
Texture2D<float> SceneDepth;
SamplerState SceneDepth_sampler;

struct SakuraScreenOutput
{
    float4 Position : SV_POSITION;
    float2 TextureCoordinates : TEX_COORD;
};

SakuraScreenOutput MaterialFullscreenMain(uint VertexIndex : SV_VertexID)
{
    SakuraScreenOutput Output;
    float2 Coordinates = float2((VertexIndex << 1) & 2, VertexIndex & 2);
    Output.Position = float4(Coordinates * 2.0 - 1.0, 0.0, 1.0);
    Output.TextureCoordinates = Coordinates;
    if (ViewOptions.x < 0.5)
    {
        Output.TextureCoordinates.y = 1.0 - Output.TextureCoordinates.y;
    }
    return Output;
}

float3 ReconstructWorldPosition(float2 Coordinates, float Depth)
{
    float2 Ndc = Coordinates * 2.0 - 1.0;
    if (ViewOptions.x < 0.5)
    {
        Ndc.y = -Ndc.y;
    }
    float4 Position = mul(float4(Ndc, Depth, 1.0), InverseViewProjection);
    return Position.xyz / max(abs(Position.w), 0.000001) * sign(Position.w);
}

float4 MaterialPixelMain(SakuraScreenOutput Input) : SV_TARGET
{
#if SAKURA_MATERIAL_DOMAIN == 1
    SakuraPostProcessInput MaterialInput;
    MaterialInput.TextureCoordinates = Input.TextureCoordinates;
    MaterialInput.Resolution = ResolutionTime.xy;
    MaterialInput.Time = ResolutionTime.z;
    MaterialInput.SceneColor = SceneImage.Sample(SceneImage_sampler, Input.TextureCoordinates);
    MaterialInput.Depth = SceneDepth.Sample(SceneDepth_sampler, Input.TextureCoordinates);
    MaterialInput.WorldPosition = ReconstructWorldPosition(Input.TextureCoordinates, MaterialInput.Depth);
    float4 Result = EvaluatePostProcess(MaterialInput);
    return lerp(MaterialInput.SceneColor, Result, saturate(ResolutionTime.w));
#else
    SakuraUserInterfaceInput MaterialInput;
    MaterialInput.TextureCoordinates = Input.TextureCoordinates;
    MaterialInput.Resolution = ResolutionTime.xy;
    MaterialInput.Time = ResolutionTime.z;
    MaterialInput.Color = WidgetColor;
    float4 Result = EvaluateUserInterface(MaterialInput);
#if SAKURA_BLEND_MODE == 0
    Result.a = 1.0;
#elif SAKURA_BLEND_MODE == 1
    clip(Result.a - ViewOptions.y);
    Result.a = 1.0;
#endif
    Result.rgb = max(Result.rgb, 0.0);
    float3 SrgbLow = Result.rgb * 12.92;
    float3 SrgbHigh = 1.055 * pow(Result.rgb, 1.0 / 2.4) - 0.055;
    Result.rgb = lerp(SrgbHigh, SrgbLow, step(Result.rgb, 0.0031308));
    Result.a = saturate(Result.a);
    Result.rgb *= Result.a;
    return Result;
#endif
}
