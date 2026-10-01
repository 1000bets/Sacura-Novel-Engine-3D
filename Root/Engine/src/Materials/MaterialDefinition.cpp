#include "Materials/MaterialDefinition.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <set>
#include <sstream>

const char* MaterialDefinitionIO::ParameterTypeName(MaterialParameterType Type)
{
    const char* Names[] = {"Float", "Integer", "Boolean", "Vector2", "Vector3", "Vector4", "Color", "Texture2D", "StaticSwitch"};
    return Names[static_cast<size_t>(Type)];
}

bool MaterialDefinitionIO::ParseParameterType(const std::string& Name, MaterialParameterType& OutType)
{
    for (int Index = 0; Index <= static_cast<int>(MaterialParameterType::StaticSwitch); ++Index)
    {
        const auto Type = static_cast<MaterialParameterType>(Index);
        if (Name == ParameterTypeName(Type))
        {
            OutType = Type;
            return true;
        }
    }
    return false;
}

AssetDiagnostic MaterialDefinitionIO::Validate(const ResolvedMaterial& Material)
{
    size_t TextureCount = 0;
    size_t SwitchCount = 0;
    size_t ConstantCount = 0;
    std::set<std::string> Identifiers;
    for (const auto& Parameter : Material.Definition.Parameters)
    {
        const std::string& Identifier = Parameter.Identifier;
        if (Identifier.empty() || !std::isalpha(static_cast<unsigned char>(Identifier.front()))
            || !std::all_of(Identifier.begin(), Identifier.end(), [](unsigned char Character)
                { return std::isalnum(Character) || Character == '_'; })
            || !Identifiers.insert(Identifier).second || Identifier.rfind("Sakura", 0) == 0)
        {
            return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "Material", "Invalid or duplicate parameter identifier: " + Identifier);
        }
        const auto Found = Material.Values.find(Identifier);
        if (Found == Material.Values.end() || Found->second.Type != Parameter.DefaultValue.Type)
        {
            return AssetDiagnostic::Fail(AssetErrorCode::TypeMismatch, "Material", "Invalid parameter value: " + Identifier);
        }
        const auto& Value = Found->second;
        if (Value.Type == MaterialParameterType::Texture2D)
        {
            ++TextureCount;
        }
        else if (Value.Type == MaterialParameterType::StaticSwitch)
        {
            ++SwitchCount;
        }
        else
        {
            ++ConstantCount;
            const float Numbers[] = {Value.Numbers.x, Value.Numbers.y, Value.Numbers.z, Value.Numbers.w};
            for (float Number : Numbers)
            {
                if (!std::isfinite(Number))
                {
                    return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "Material", "Non-finite parameter: " + Identifier);
                }
            }
            if (Parameter.bHasRange)
            {
                double Number = Value.Numbers.x;
                if (Value.Type == MaterialParameterType::Integer)
                {
                    Number = Value.Integer;
                }
                if (!std::isfinite(Parameter.Minimum) || !std::isfinite(Parameter.Maximum) || Parameter.Minimum > Parameter.Maximum)
                {
                    return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "Material", "Invalid parameter range: " + Identifier);
                }
                size_t DimensionCount = 1;
                if (Value.Type == MaterialParameterType::Vector2)
                {
                    DimensionCount = 2;
                }
                else if (Value.Type == MaterialParameterType::Vector3)
                {
                    DimensionCount = 3;
                }
                else if (Value.Type == MaterialParameterType::Vector4 || Value.Type == MaterialParameterType::Color)
                {
                    DimensionCount = 4;
                }
                for (size_t Dimension = 0; Dimension < DimensionCount; ++Dimension)
                {
                    if (Value.Type != MaterialParameterType::Integer)
                    {
                        Number = Numbers[Dimension];
                    }
                    if (Number < Parameter.Minimum || Number > Parameter.Maximum)
                    {
                        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "Material", "Parameter outside declared range: " + Identifier);
                    }
                }
            }
        }
    }
    if (TextureCount > 8 || SwitchCount > 8 || ConstantCount > 256)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "Material", "Material exceeds texture, static switch or constant buffer limits");
    }
    if (!std::isfinite(Material.Definition.BoundsExpansion) || Material.Definition.BoundsExpansion < 0.f
        || !std::isfinite(Material.Definition.AlphaCutoff) || Material.Definition.AlphaCutoff < 0.f || Material.Definition.AlphaCutoff > 1.f)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "Material", "Invalid bounds expansion or alpha cutoff");
    }
    return AssetDiagnostic::Ok();
}

std::string MaterialDefinitionIO::BuildParameterDeclarations(const ResolvedMaterial& Material)
{
    std::ostringstream Source;
    Source << "cbuffer SakuraMaterialConstants { float4 SakuraParameterValues[256]; };\n";
    Source << "cbuffer SakuraMaterialTextureState { uint4 SakuraTexturePresence; };\n";
    Source << "struct MaterialParameters {\n";
    size_t ConstantCount = 0;
    for (const auto& Parameter : Material.Definition.Parameters)
    {
        const auto Type = Parameter.DefaultValue.Type;
        if (Type == MaterialParameterType::Texture2D || Type == MaterialParameterType::StaticSwitch)
        {
            continue;
        }
        const char* TypeName = "float4";
        if (Type == MaterialParameterType::Float)
        {
            TypeName = "float";
        }
        else if (Type == MaterialParameterType::Integer)
        {
            TypeName = "int";
        }
        else if (Type == MaterialParameterType::Boolean)
        {
            TypeName = "bool";
        }
        else if (Type == MaterialParameterType::Vector2)
        {
            TypeName = "float2";
        }
        else if (Type == MaterialParameterType::Vector3)
        {
            TypeName = "float3";
        }
        Source << TypeName << " " << Parameter.Identifier << ";\n";
        ++ConstantCount;
    }
    if (ConstantCount == 0)
    {
        Source << "float SakuraUnused;\n";
    }
    Source << "};\nMaterialParameters GetMaterialParameters() { MaterialParameters Parameters = (MaterialParameters)0;\n";
    size_t ConstantIndex = 0;
    for (const auto& Parameter : Material.Definition.Parameters)
    {
        const auto Type = Parameter.DefaultValue.Type;
        if (Type == MaterialParameterType::Texture2D || Type == MaterialParameterType::StaticSwitch)
        {
            continue;
        }
        Source << "Parameters." << Parameter.Identifier << " = ";
        if (Type == MaterialParameterType::Integer)
        {
            Source << "asint(SakuraParameterValues[" << ConstantIndex << "].x)";
        }
        else if (Type == MaterialParameterType::Boolean)
        {
            Source << "(SakuraParameterValues[" << ConstantIndex << "].x != 0.0)";
        }
        else
        {
            Source << "SakuraParameterValues[" << ConstantIndex << "]";
            if (Type == MaterialParameterType::Float)
        {
            Source << ".x";
        }
            else if (Type == MaterialParameterType::Vector2)
        {
            Source << ".xy";
        }
            else if (Type == MaterialParameterType::Vector3)
        {
            Source << ".xyz";
        }
        }
        Source << ";\n";
        ++ConstantIndex;
    }
    Source << "return Parameters; }\n";
    size_t TextureIndex = 0;
    for (const auto& Parameter : Material.Definition.Parameters)
    {
        const auto& Value = Material.Values.at(Parameter.Identifier);
        const auto& Identifier = Parameter.Identifier;
        if (Value.Type == MaterialParameterType::StaticSwitch)
        {
            int SwitchValue = 0;
            if (Value.bBoolean)
            {
                SwitchValue = 1;
            }
            Source << "#define MATERIAL_SWITCH_" << Identifier << " " << SwitchValue << "\n";
        }
        else if (Value.Type == MaterialParameterType::Texture2D)
        {
            Source << "bool Has" << Identifier << "() { return (SakuraTexturePresence.x & " << (1u << TextureIndex) << ") != 0; }\n";
            ++TextureIndex;
            Source << "Texture2D SakuraTexture_" << Identifier << "; SamplerState SakuraTexture_" << Identifier << "_sampler;\n";
            Source << "float4 Sample" << Identifier << "(float2 Coordinates) { return SakuraTexture_" << Identifier << ".Sample(SakuraTexture_" << Identifier << "_sampler, Coordinates); }\n";
            Source << "float4 Sample" << Identifier << "Level(float2 Coordinates, float Level) { return SakuraTexture_" << Identifier << ".SampleLevel(SakuraTexture_" << Identifier << "_sampler, Coordinates, Level); }\n";
        }
    }
    return Source.str();
}

std::vector<DirectX::SimpleMath::Vector4> MaterialDefinitionIO::PackParameters(const ResolvedMaterial& Material)
{
    std::vector<DirectX::SimpleMath::Vector4> Output(256, DirectX::SimpleMath::Vector4::Zero);
    size_t Index = 0;
    for (const auto& Parameter : Material.Definition.Parameters)
    {
        const auto& Value = Material.Values.at(Parameter.Identifier);
        if (Value.Type == MaterialParameterType::Texture2D || Value.Type == MaterialParameterType::StaticSwitch)
        {
            continue;
        }
        Output[Index] = Value.Numbers;
        if (Value.Type == MaterialParameterType::Boolean)
        {
            Output[Index].x = 0.f;
            if (Value.bBoolean)
            {
                Output[Index].x = 1.f;
            }
        }
        else if (Value.Type == MaterialParameterType::Integer)
        {
            std::memcpy(&Output[Index].x, &Value.Integer, sizeof(Value.Integer));
        }
        ++Index;
    }
    return Output;
}

std::string MaterialDefinitionIO::BuildVariantKey(const ResolvedMaterial& Material)
{
    std::string Key = Material.Root.Asset.ToString() + ":" + std::to_string(Material.Definition.ShaderRevision) + ":1";
    for (const auto& Parameter : Material.Definition.Parameters)
    {
        if (Parameter.DefaultValue.Type == MaterialParameterType::StaticSwitch)
        {
            Key += ":" + Parameter.Identifier + "=";
            if (Material.Values.at(Parameter.Identifier).bBoolean)
            {
                Key += "1";
            }
            else
            {
                Key += "0";
            }
        }
    }
    return Key;
}
