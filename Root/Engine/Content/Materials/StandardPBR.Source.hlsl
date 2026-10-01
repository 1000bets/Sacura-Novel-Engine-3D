SakuraSurface EvaluateSurface(SakuraSurfaceInput Input)
{
    MaterialParameters Parameters = GetMaterialParameters();
    SakuraSurface Surface = MakeDefaultSurface(Input);
    float4 Base = Parameters.BaseColor * Input.VertexColor;
    if (HasBaseColorTexture())
    {
        Base *= SampleBaseColorTexture(Input.TextureCoordinates);
    }
    Surface.Albedo = Base.rgb;
    Surface.Opacity = Base.a;
    Surface.Metalness = Parameters.Metallic;
    Surface.RoughnessValue = Parameters.Roughness;
    if (HasMetallicRoughnessTexture())
    {
        float4 Packed = SampleMetallicRoughnessTexture(Input.TextureCoordinates);
        Surface.Metalness *= Packed.b;
        Surface.RoughnessValue *= Packed.g;
    }
    if (HasNormalTexture())
    {
        float3 TangentNormal = SampleNormalTexture(Input.TextureCoordinates).rgb * 2.0 - 1.0;
        TangentNormal.xy *= Parameters.NormalStrength;
        Surface.Normal = TangentNormalToWorld(Input, normalize(TangentNormal));
    }
    if (HasOcclusionTexture())
    {
        Surface.Occlusion = lerp(1.0, SampleOcclusionTexture(Input.TextureCoordinates).r, Parameters.OcclusionStrength);
    }
    Surface.Emission = Parameters.Emissive.rgb;
    if (HasEmissiveTexture())
    {
        Surface.Emission *= SampleEmissiveTexture(Input.TextureCoordinates).rgb;
    }
    return Surface;
}
