#pragma once

#include "Assets/AssetTypes.h"
#include <SimpleMath.h>
#include <map>
#include <string>
#include <vector>

enum class MaterialDomain { Surface, PostProcess, UserInterface };
enum class MaterialShadingModel { Lit, Unlit, Toon };
enum class MaterialBlendMode { Opaque, Masked, Translucent, Additive };
enum class MaterialParameterType { Float, Integer, Boolean, Vector2, Vector3, Vector4, Color, Texture2D, StaticSwitch };
enum class MaterialPostProcessStage { BeforeToneMapping, AfterToneMapping };

struct MaterialParameterValue
{
    MaterialParameterType Type = MaterialParameterType::Float;
    DirectX::SimpleMath::Vector4 Numbers = DirectX::SimpleMath::Vector4::Zero;
    int32_t Integer = 0;
    bool bBoolean = false;
    AssetKey Texture;
};

struct MaterialParameterDefinition
{
    std::string Identifier;
    std::string DisplayName;
    std::string Group;
    MaterialParameterValue DefaultValue;
    bool bHasRange = false;
    double Minimum = 0.0;
    double Maximum = 1.0;
    bool bSrgb = false;
    bool bClamp = false;
    bool bPointFiltering = false;
};

struct MaterialDefinition
{
    MaterialDomain Domain = MaterialDomain::Surface;
    MaterialShadingModel ShadingModel = MaterialShadingModel::Lit;
    MaterialBlendMode BlendMode = MaterialBlendMode::Opaque;
    MaterialPostProcessStage PostProcessStage = MaterialPostProcessStage::BeforeToneMapping;
    bool bDoubleSided = false;
    bool bCastShadows = true;
    bool bReceiveShadows = true;
    bool bModifyVertex = false;
    bool bAnimated = false;
    float BoundsExpansion = 0.0f;
    float AlphaCutoff = 0.5f;
    AssetKey Source;
    std::vector<MaterialParameterDefinition> Parameters;
    std::string PreparedSource;
    std::vector<std::string> SourceFiles;
    std::vector<AssetKey> Dependencies;
    uint64_t Revision = 0;
    uint64_t ShaderRevision = 0;
};

struct ResolvedMaterial
{
    MaterialDefinition Definition;
    AssetKey Root;
    AssetKey Asset;
    AssetKey Parent;
    std::map<std::string, MaterialParameterValue> Values;
    std::map<std::string, AssetKey> ValueOrigins;
    std::vector<std::string> Warnings;
    std::string VariantKey;
};

class MaterialDefinitionIO
{
public:
    static const char* ParameterTypeName(MaterialParameterType Type);
    static bool ParseParameterType(const std::string& Name, MaterialParameterType& OutType);
    static AssetDiagnostic Validate(const ResolvedMaterial& Material);
    static std::string BuildParameterDeclarations(const ResolvedMaterial& Material);
    static std::vector<DirectX::SimpleMath::Vector4> PackParameters(const ResolvedMaterial& Material);
    static std::string BuildVariantKey(const ResolvedMaterial& Material);
};
