float4 EvaluateUserInterface(SakuraUserInterfaceInput Input)
{
    return SampleImage(Input.TextureCoordinates) * GetMaterialParameters().Tint * Input.Color;
}
