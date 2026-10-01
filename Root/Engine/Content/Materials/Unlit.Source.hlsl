SakuraSurface EvaluateSurface(SakuraSurfaceInput Input)
{
    MaterialParameters Parameters = GetMaterialParameters();
    SakuraSurface Surface = MakeDefaultSurface(Input);
    float4 Color = Parameters.BaseColor * Input.VertexColor * SampleBaseColorTexture(Input.TextureCoordinates);
    Surface.Albedo = Color.rgb;
    Surface.Opacity = Color.a;
    Surface.Emission = Parameters.Emissive.rgb;
    return Surface;
}
