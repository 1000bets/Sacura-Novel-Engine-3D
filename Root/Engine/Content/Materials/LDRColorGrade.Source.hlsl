float4 EvaluatePostProcess(SakuraPostProcessInput Input)
{
    MaterialParameters Parameters = GetMaterialParameters();
    return float4(saturate(Input.SceneColor.rgb * Parameters.Tint.rgb), Input.SceneColor.a);
}
