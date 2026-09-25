#define MaximumLights 32
#define MaximumShadowViews 16

struct LightData
{
    float4 PositionType;
    float4 DirectionRange;
    float4 ColorIntensity;
    float4 ConeShadow;
};

cbuffer FrameConstants
{
    float4x4 ViewProjection;
    float4 CameraPosition;
    float4 FrameParameters;
    float4 OutputParameters;
    LightData Lights[MaximumLights];
    float4x4 ShadowMatrices[MaximumShadowViews];
};

cbuffer ObjectConstants
{
    float4x4 World;
    float4x4 NormalTransform;
    float4 BaseColor;
    float4 MaterialParameters;
    float4 EmissiveAlphaMode;
};

Texture2D BaseColorImage;
SamplerState BaseColorImage_sampler;
Texture2D<float> ShadowAtlas;
SamplerState ShadowAtlas_sampler;
TextureCube IrradianceImage;
SamplerState IrradianceImage_sampler;
TextureCube ReflectionImage;
SamplerState ReflectionImage_sampler;
Texture2D BrdfImage;
SamplerState BrdfImage_sampler;

struct VertexInput
{
    float3 Position : ATTRIB0;
    float3 Normal : ATTRIB1;
    float2 TextureCoordinates : ATTRIB2;
    float4 Color : ATTRIB3;
};

struct VertexOutput
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : WORLD_POSITION;
    float3 Normal : NORMAL;
    float2 TextureCoordinates : TEX_COORD;
    float4 Color : COLOR;
};

VertexOutput VSMain(VertexInput Input)
{
    VertexOutput Output;
    float4 WorldPosition = mul(float4(Input.Position, 1.0), World);
    Output.Position = mul(WorldPosition, ViewProjection);
    if (OutputParameters.x > 0.5)
    {
        Output.Position.z = Output.Position.z * 2.0 - Output.Position.w;
    }
    Output.WorldPosition = WorldPosition.xyz;
    Output.Normal = mul(float4(Input.Normal, 0.0), NormalTransform).xyz;
    Output.TextureCoordinates = Input.TextureCoordinates;
    Output.Color = Input.Color;
    return Output;
}

float4 ReadColor(VertexOutput Input)
{
    float4 Surface = BaseColor * Input.Color;
    if (MaterialParameters.w > 0.5)
    {
        Surface *= BaseColorImage.Sample(BaseColorImage_sampler, Input.TextureCoordinates);
    }
    if (EmissiveAlphaMode.w > 0.5 && EmissiveAlphaMode.w < 1.5)
    {
        clip(Surface.a - MaterialParameters.z);
    }
    return Surface;
}

void ShadowMain(VertexOutput Input)
{
    ReadColor(Input);
}

float ShadowVisibility(uint LightIndex, float3 Position, float3 Normal, float3 ToLight)
{
    LightData Light = Lights[LightIndex];
    if (Light.ConeShadow.z < 0.0)
    {
        return 1.0;
    }
    uint ShadowIndex = uint(Light.ConeShadow.z);
    if (Light.PositionType.w > 0.5 && Light.PositionType.w < 1.5)
    {
        float3 Direction = Position - Light.PositionType.xyz;
        float3 Magnitude = abs(Direction);
        uint Face = 0;
        if (Magnitude.x >= Magnitude.y && Magnitude.x >= Magnitude.z)
        {
            if (Direction.x < 0.0)
            {
                Face = 1;
            }
        }
        else if (Magnitude.y >= Magnitude.z)
        {
            Face = 2;
            if (Direction.y < 0.0)
            {
                Face = 3;
            }
        }
        else
        {
            Face = 4;
            if (Direction.z < 0.0)
            {
                Face = 5;
            }
        }
        ShadowIndex += Face;
    }
    float4 Projected = mul(float4(Position, 1.0), ShadowMatrices[ShadowIndex]);
    if (Projected.w <= 0.0)
    {
        return 1.0;
    }
    float3 Coordinates = Projected.xyz / Projected.w;
    float2 LocalCoordinates = Coordinates.xy * float2(0.5, OutputParameters.y * 0.5) + 0.5;
    if (any(LocalCoordinates < 0.0) || any(LocalCoordinates > 1.0) || Coordinates.z <= 0.0 || Coordinates.z >= 1.0)
    {
        return 1.0;
    }
    float2 Tile = float2(ShadowIndex % 4, ShadowIndex / 4);
    float Visibility = 0.0;
    float Bias = max(0.0003, 0.0015 * (1.0 - saturate(dot(Normal, ToLight))));
    if (Light.PositionType.w > 0.5)
    {
        Bias = max(0.00001, 0.00003 * (1.0 - saturate(dot(Normal, ToLight))));
    }
    [unroll]
    for (int Row = -1; Row <= 1; ++Row)
    {
        [unroll]
        for (int Column = -1; Column <= 1; ++Column)
        {
            float2 SampleCoordinates = clamp(LocalCoordinates + float2(Column, Row) / 1024.0, 0.5 / 1024.0, 1.0 - 0.5 / 1024.0);
            float Depth = ShadowAtlas.SampleLevel(ShadowAtlas_sampler, (SampleCoordinates + Tile) * 0.25, 0).r;
            Visibility += step(Coordinates.z - Bias, Depth);
        }
    }
    return Visibility / 9.0;
}

float GeometryTerm(float Cosine, float Roughness)
{
    float Factor = (Roughness + 1.0) * (Roughness + 1.0) * 0.125;
    return Cosine / max(Cosine * (1.0 - Factor) + Factor, 0.0001);
}

float3 Fresnel(float Cosine, float3 Reflectance)
{
    return Reflectance + (1.0 - Reflectance) * pow(1.0 - saturate(Cosine), 5.0);
}

float4 ShadeSurface(VertexOutput Input, bool FrontFace)
{
    float4 Surface = ReadColor(Input);
    float3 Normal = normalize(Input.Normal);
    if (!FrontFace)
    {
        Normal = -Normal;
    }
    float3 ViewDirection = normalize(CameraPosition.xyz - Input.WorldPosition);
    float Metallic = saturate(MaterialParameters.x);
    float Roughness = clamp(MaterialParameters.y, 0.045, 1.0);
    float3 Reflectance = lerp(0.04, Surface.rgb, Metallic);
    float NormalView = max(dot(Normal, ViewDirection), 0.001);
    float3 Result = EmissiveAlphaMode.xyz;
    for (uint LightIndex = 0; LightIndex < (uint)FrameParameters.x; ++LightIndex)
    {
        LightData Light = Lights[LightIndex];
        float3 ToLight = -Light.DirectionRange.xyz;
        float Attenuation = 1.0;
        if (Light.PositionType.w > 0.5)
        {
            float3 Offset = Light.PositionType.xyz - Input.WorldPosition;
            float DistanceSquared = max(dot(Offset, Offset), 0.0001);
            float Distance = sqrt(DistanceSquared);
            ToLight = Offset / Distance;
            float Fade = saturate(1.0 - pow(Distance / max(Light.DirectionRange.w, 0.001), 4.0));
            Attenuation = Fade * Fade / max(DistanceSquared, 0.01);
            if (Light.PositionType.w > 1.5)
            {
                Attenuation *= smoothstep(Light.ConeShadow.y, Light.ConeShadow.x, dot(-ToLight, Light.DirectionRange.xyz));
            }
        }
        float NormalLight = saturate(dot(Normal, ToLight));
        if (NormalLight <= 0.0 || Attenuation <= 0.0)
        {
            continue;
        }
        float3 HalfDirection = normalize(ViewDirection + ToLight);
        float NormalHalf = saturate(dot(Normal, HalfDirection));
        float AlphaSquared = pow(Roughness, 4.0);
        float Denominator = NormalHalf * NormalHalf * (AlphaSquared - 1.0) + 1.0;
        float Distribution = AlphaSquared / max(3.14159265 * Denominator * Denominator, 0.000001);
        float3 Reflection = Fresnel(dot(HalfDirection, ViewDirection), Reflectance);
        float Geometry = GeometryTerm(NormalView, Roughness) * GeometryTerm(NormalLight, Roughness);
        float3 Specular = Distribution * Geometry * Reflection / max(4.0 * NormalView * NormalLight, 0.0001);
        float3 Diffuse = (1.0 - Reflection) * (1.0 - Metallic) * Surface.rgb / 3.14159265;
        Result += (Diffuse + Specular) * Light.ColorIntensity.rgb * Light.ColorIntensity.w
            * NormalLight * Attenuation * ShadowVisibility(LightIndex, Input.WorldPosition, Normal, ToLight);
    }
    float3 EnvironmentFresnel = Reflectance + (max(1.0 - Roughness, Reflectance) - Reflectance) * pow(1.0 - NormalView, 5.0);
    float3 Irradiance = IrradianceImage.SampleLevel(IrradianceImage_sampler, Normal, 0).rgb;
    float3 Reflection = ReflectionImage.SampleLevel(ReflectionImage_sampler, reflect(-ViewDirection, Normal), Roughness * 5.0).rgb;
    float2 IntegratedBrdf = BrdfImage.SampleLevel(BrdfImage_sampler, float2(NormalView, Roughness), 0).rg;
    Result += FrameParameters.y * ((1.0 - EnvironmentFresnel) * (1.0 - Metallic) * Surface.rgb * Irradiance
        + Reflection * (Reflectance * IntegratedBrdf.x + IntegratedBrdf.y));
    return float4(max(Result, 0.0), saturate(Surface.a));
}

float4 PSMain(VertexOutput Input, bool FrontFace : SV_IsFrontFace) : SV_TARGET
{
    return float4(ShadeSurface(Input, FrontFace).rgb, 1.0);
}

struct TransparencyOutput
{
    float4 Accumulation : SV_TARGET0;
    float Revealage : SV_TARGET1;
};

TransparencyOutput TransparencyMain(VertexOutput Input, bool FrontFace : SV_IsFrontFace)
{
    float4 Surface = ShadeSurface(Input, FrontFace);
    float Weight = clamp(pow(Surface.a + 0.01, 3.0) * 8.0 * pow(1.0 - Input.Position.z * 0.9, 3.0), 0.01, 8.0);
    TransparencyOutput Output;
    Output.Accumulation = float4(Surface.rgb * Surface.a, Surface.a) * Weight;
    Output.Revealage = Surface.a;
    return Output;
}
