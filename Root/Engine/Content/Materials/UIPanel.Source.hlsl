float4 EvaluateUserInterface(SakuraUserInterfaceInput Input)
{
    MaterialParameters Parameters = GetMaterialParameters();
    float2 HalfSize = Input.Resolution * 0.5;
    float2 Position = abs((Input.TextureCoordinates - 0.5) * Input.Resolution) - HalfSize + Parameters.Radius;
    float Distance = length(max(Position, 0.0)) + min(max(Position.x, Position.y), 0.0) - Parameters.Radius;
    float Alpha = saturate(0.5 - Distance);
    return float4(Parameters.Tint.rgb * Input.Color.rgb, Alpha * Parameters.Tint.a * Input.Color.a);
}
