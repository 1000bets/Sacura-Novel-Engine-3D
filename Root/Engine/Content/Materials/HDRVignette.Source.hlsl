float4 EvaluatePostProcess(SakuraPostProcessInput Input)
{
    float Radius = length(Input.TextureCoordinates - 0.5);
    float Factor = 1.0 - smoothstep(0.2, 0.71, Radius) * GetMaterialParameters().Strength;
    return float4(Input.SceneColor.rgb * Factor, Input.SceneColor.a);
}
