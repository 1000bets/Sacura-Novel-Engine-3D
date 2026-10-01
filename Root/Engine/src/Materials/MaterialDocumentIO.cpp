#include "Materials/MaterialDocumentIO.h"
#include "Assets/AssetRegistry.h"
#include "Assets/ContentHash.h"
#include "Core/IO/AtomicFileWriter.h"
#include <fstream>
#include <functional>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

using namespace DirectX::SimpleMath;

AssetDiagnostic MaterialDocumentIO::Read(const std::string& Path, nlohmann::json& OutDocument)
{
    try
    {
        std::ifstream Input(Path);
        if (!Input)
        {
            throw std::runtime_error("Cannot open material document: " + Path);
        }
        Input >> OutDocument;
        if (!OutDocument.is_object())
        {
            throw std::runtime_error("Material document must be an object");
        }
        return AssetDiagnostic::Ok();
    }
    catch (const std::exception& Exception)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialDocumentIO", Exception.what(), {}, Path);
    }
}

AssetDiagnostic MaterialDocumentIO::Save(const std::string& Path, const nlohmann::json& Document)
{
    std::string Error;
    if (!AtomicFileWriter::WriteText(Path, Document.dump(2) + "\n", Error))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialDocumentIO", Error, {}, Path);
    }
    return AssetDiagnostic::Ok();
}

MaterialParameterValue MaterialDocumentIO::ReadValue(MaterialParameterType Type, const nlohmann::json& Value)
{
    MaterialParameterValue Output;
    Output.Type = Type;
    if (Type == MaterialParameterType::Texture2D)
    {
        if (!Value.is_null() && Value != "")
        {
            AssetDiagnostic Diagnostic;
            if (!AssetMetadataIO::TryAssetRefFromJson(Value, Output.Texture, Diagnostic))
            {
                throw std::runtime_error(Diagnostic.Message);
            }
        }
    }
    else if (Type == MaterialParameterType::Boolean || Type == MaterialParameterType::StaticSwitch)
    {
        Output.bBoolean = Value.get<bool>();
    }
    else if (Type == MaterialParameterType::Integer)
    {
        if (!Value.is_number_integer())
        {
            throw std::runtime_error("Integer parameter requires an integer");
        }
        const int64_t Integer = Value.get<int64_t>();
        if (Integer < INT32_MIN || Integer > INT32_MAX)
        {
            throw std::runtime_error("Integer parameter exceeds int32 range");
        }
        Output.Integer = static_cast<int32_t>(Integer);
    }
    else if (Type == MaterialParameterType::Float)
    {
        Output.Numbers.x = Value.get<float>();
    }
    else
    {
        size_t Count = 4;
        if (Type == MaterialParameterType::Vector2)
        {
            Count = 2;
        }
        else if (Type == MaterialParameterType::Vector3)
        {
            Count = 3;
        }
        if (!Value.is_array() || Value.size() != Count)
        {
            throw std::runtime_error("Invalid vector parameter dimensions");
        }
        float* Numbers = &Output.Numbers.x;
        for (size_t Index = 0; Index < Count; ++Index)
        {
            Numbers[Index] = Value.at(Index).get<float>();
        }
    }
    return Output;
}

nlohmann::json MaterialDocumentIO::WriteValue(const MaterialParameterValue& Value)
{
    if (Value.Type == MaterialParameterType::Texture2D)
    {
        if (!Value.Texture.IsValid())
        {
            return nullptr;
        }
        return AssetMetadataIO::AssetRefToJson(Value.Texture);
    }
    if (Value.Type == MaterialParameterType::Boolean || Value.Type == MaterialParameterType::StaticSwitch)
    {
        return Value.bBoolean;
    }
    if (Value.Type == MaterialParameterType::Integer)
    {
        return Value.Integer;
    }
    if (Value.Type == MaterialParameterType::Float)
    {
        return Value.Numbers.x;
    }
    nlohmann::json Output = {Value.Numbers.x, Value.Numbers.y};
    if (Value.Type != MaterialParameterType::Vector2)
    {
        Output.push_back(Value.Numbers.z);
    }
    if (Value.Type == MaterialParameterType::Vector4 || Value.Type == MaterialParameterType::Color)
    {
        Output.push_back(Value.Numbers.w);
    }
    return Output;
}

AssetDiagnostic MaterialDocumentIO::Resolve(const AssetRegistry& Registry, const AssetKey& Key, ResolvedMaterial& OutMaterial,
    const std::map<std::string, nlohmann::json>& Documents, const std::map<std::string, std::string>& Sources)
{
    ResolvedMaterial Result;
    std::string DocumentIdentity;
    std::set<std::string> Visiting;
    std::set<std::string> IncludeStack;
    std::function<void(const AssetKey&)> ResolveDocument;
    std::function<std::string(const AssetRegistryEntry&)> ReadSource;
    try
    {
        auto ReadKey = [](const nlohmann::json& Value)
        {
            AssetKey Parsed;
            AssetDiagnostic Diagnostic;
            if (!AssetMetadataIO::TryAssetRefFromJson(Value, Parsed, Diagnostic) || !Parsed.IsValid())
            {
                throw std::runtime_error("Invalid asset reference");
            }
            return Parsed;
        };
        ReadSource = [&](const AssetRegistryEntry& Entry) -> std::string
        {
            if (!IncludeStack.insert(Entry.AbsolutePath).second)
            {
                throw std::runtime_error("Shader include cycle: " + Entry.VirtualPath);
            }
            std::ifstream File(Entry.AbsolutePath);
            const auto EditedSource = Sources.find(Entry.Metadata.Guid.ToString());
            if (!File && EditedSource == Sources.end())
            {
                throw std::runtime_error("Shader source not found: " + Entry.VirtualPath);
            }
            Result.Definition.SourceFiles.push_back(Entry.AbsolutePath);
            Result.Definition.Dependencies.push_back(AssetKey{Entry.Metadata.Guid, {}});
            std::string Contents;
            if (EditedSource != Sources.end())
            {
                Contents = EditedSource->second;
            }
            else
            {
                Contents.assign(std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>());
            }
            if (Contents.compare(0, 3, "\xEF\xBB\xBF") == 0)
            {
                Contents.erase(0, 3);
            }
            std::istringstream SourceInput(Contents);
            std::ostringstream Expanded;
            Expanded << "\n#line 1 \"" << Entry.VirtualPath << "\"\n";
            const std::regex IncludePattern(R"(^\s*#\s*include\s*[\"<]([^\">]+)[\">]\s*(?://.*)?$)");
            std::string Line;
            size_t LineNumber = 0;
            while (std::getline(SourceInput, Line))
            {
                ++LineNumber;
                std::smatch Match;
                if (std::regex_match(Line, Match, IncludePattern))
                {
                    std::string IncludePath = Match[1].str();
                    if (IncludePath.rfind("Sakura/", 0) == 0)
                    {
                        Expanded << Line << '\n';
                        continue;
                    }
                    if (IncludePath.front() != '/')
                    {
                        IncludePath = (std::filesystem::path(Entry.VirtualPath).parent_path() / IncludePath).lexically_normal().generic_string();
                    }
                    AssetRegistryEntry Included;
                    if (!Registry.TryGetByPath(IncludePath, Included) || Included.Metadata.Type != ShaderSourceAssetType)
                    {
                        throw std::runtime_error("Shader include not found: " + IncludePath);
                    }
                    Expanded << ReadSource(Included) << "\n#line " << LineNumber + 1 << " \"" << Entry.VirtualPath << "\"\n";
                }
                else
                {
                    Expanded << Line << '\n';
                }
            }
            IncludeStack.erase(Entry.AbsolutePath);
            return Expanded.str();
        };
        ResolveDocument = [&](const AssetKey& Current)
        {
            if (!Visiting.insert(Current.Asset.ToString()).second)
            {
                throw std::runtime_error("Material inheritance cycle");
            }
            AssetRegistryEntry Entry;
            if (!Registry.TryResolveKey(Current, Entry) || Current.HasSubAsset())
            {
                throw std::runtime_error("Material parent or document not found: " + Current.Asset.ToString());
            }
            nlohmann::json Document;
            AssetDiagnostic ReadResult;
            const auto EditedDocument = Documents.find(Current.Asset.ToString());
            if (EditedDocument != Documents.end())
            {
                Document = EditedDocument->second;
            }
            else
            {
                ReadResult = Read(Entry.AbsolutePath, Document);
            }
            if (ReadResult.HasError())
            {
                throw std::runtime_error(ReadResult.Message);
            }
            DocumentIdentity += Current.Asset.ToString() + Document.dump();
            if (Entry.Metadata.Type == MaterialInstanceAssetType)
            {
                if (Document.at("schemaVersion") != 1)
                {
                    throw std::runtime_error("Unsupported Material Instance schemaVersion");
                }
                const AssetKey Parent = ReadKey(Document.at("parent"));
                ResolveDocument(Parent);
                Result.Parent = Parent;
                const nlohmann::json Overrides = Document.value("overrides", nlohmann::json::object());
                if (!Overrides.is_object())
                {
                    throw std::runtime_error("Overrides must be an object");
                }
                for (const auto& Override : Overrides.items())
                {
                    MaterialParameterType Type;
                    if (!MaterialDefinitionIO::ParseParameterType(Override.value().at("type").get<std::string>(), Type))
                    {
                        throw std::runtime_error("Unknown override parameter type");
                    }
                    const auto Found = Result.Values.find(Override.key());
                    if (Found == Result.Values.end())
                    {
                        Result.Warnings.push_back("Unused override: " + Override.key());
                        continue;
                    }
                    if (Found->second.Type != Type)
                    {
                        throw std::runtime_error("Override type mismatch: " + Override.key());
                    }
                    Result.Values[Override.key()] = ReadValue(Type, Override.value().at("value"));
                    Result.ValueOrigins[Override.key()] = Current;
                }
            }
            else if (Entry.Metadata.Type == MaterialAssetType)
            {
                Result.Root = Current;
                const int Version = Document.at("schemaVersion").get<int>();
                if (Version == 1)
                {
                    Document = {
                        {"schemaVersion", 2}, {"domain", "Surface"}, {"shadingModel", "Lit"},
                        {"blendMode", Document.value("alphaMode", "OPAQUE")},
                        {"doubleSided", Document.value("doubleSided", false)},
                        {"castShadows", Document.value("castShadows", true)},
                        {"alphaCutoff", Document.value("alphaCutoff", 0.5f)},
                        {"parameters", {
                            {{"id", "BaseColor"}, {"type", "Color"}, {"default", Document.value("baseColor", nlohmann::json{1.f, 1.f, 1.f, 1.f})}, {"range", {0.f, 1.f}}},
                            {{"id", "Metallic"}, {"type", "Float"}, {"default", Document.value("metallic", 0.f)}, {"range", {0.f, 1.f}}},
                            {{"id", "Roughness"}, {"type", "Float"}, {"default", Document.value("roughness", 1.f)}, {"range", {0.f, 1.f}}},
                            {{"id", "Emissive"}, {"type", "Vector3"}, {"default", Document.value("emissive", nlohmann::json{0.f, 0.f, 0.f})}},
                            {{"id", "BaseColorTexture"}, {"type", "Texture2D"}, {"default", Document.value("baseColorTexture", nlohmann::json{})}, {"srgb", true}}
                        }}
                    };
                    Result.Definition.PreparedSource = "SakuraSurface EvaluateSurface(SakuraSurfaceInput Input) { MaterialParameters Parameters = GetMaterialParameters(); SakuraSurface Output = MakeDefaultSurface(Input); Output.Albedo = Parameters.BaseColor.rgb * Input.VertexColor.rgb; Output.Opacity = Parameters.BaseColor.a * Input.VertexColor.a; float4 TextureColor = SampleBaseColorTexture(Input.TextureCoordinates); Output.Albedo *= TextureColor.rgb; Output.Opacity *= TextureColor.a; Output.Metalness = Parameters.Metallic; Output.RoughnessValue = Parameters.Roughness; Output.Emission = Parameters.Emissive; return Output; }\n";
                }
                else if (Version != 2)
                {
                    throw std::runtime_error("Unsupported Material schemaVersion");
                }
                const std::string Domain = Document.at("domain").get<std::string>();
                if (Domain == "PostProcess")
                {
                    Result.Definition.Domain = MaterialDomain::PostProcess;
                }
                else if (Domain == "UserInterface")
                {
                    Result.Definition.Domain = MaterialDomain::UserInterface;
                }
                else if (Domain != "Surface")
                {
                    throw std::runtime_error("Unknown Material domain");
                }
                const std::string Shading = Document.value("shadingModel", "Lit");
                if (Shading == "Unlit")
                {
                    Result.Definition.ShadingModel = MaterialShadingModel::Unlit;
                }
                else if (Shading == "Toon")
                {
                    Result.Definition.ShadingModel = MaterialShadingModel::Toon;
                }
                else if (Shading != "Lit")
                {
                    throw std::runtime_error("Unknown shading model");
                }
                const std::string Blend = Document.value("blendMode", "Opaque");
                if (Blend == "Masked" || Blend == "MASK")
                {
                    Result.Definition.BlendMode = MaterialBlendMode::Masked;
                }
                else if (Blend == "Translucent" || Blend == "BLEND")
                {
                    Result.Definition.BlendMode = MaterialBlendMode::Translucent;
                }
                else if (Blend == "Additive")
                {
                    Result.Definition.BlendMode = MaterialBlendMode::Additive;
                }
                else if (Blend != "Opaque" && Blend != "OPAQUE")
                {
                    throw std::runtime_error("Unknown blend mode");
                }
                const std::string Stage = Document.value("postProcessStage", "BeforeToneMapping");
                if (Stage == "AfterToneMapping")
                {
                    Result.Definition.PostProcessStage = MaterialPostProcessStage::AfterToneMapping;
                }
                else if (Stage != "BeforeToneMapping")
                {
                    throw std::runtime_error("Unknown postprocess stage");
                }
                Result.Definition.bDoubleSided = Document.value("doubleSided", false);
                Result.Definition.bCastShadows = Document.value("castShadows", true);
                Result.Definition.bReceiveShadows = Document.value("receiveShadows", true);
                Result.Definition.bModifyVertex = Document.value("modifyVertex", false);
                Result.Definition.bAnimated = Document.value("animated", false);
                Result.Definition.BoundsExpansion = Document.value("boundsExpansion", 0.f);
                Result.Definition.AlphaCutoff = Document.value("alphaCutoff", 0.5f);
                for (const auto& Parameter : Document.at("parameters"))
                {
                    MaterialParameterDefinition Definition;
                    Definition.Identifier = Parameter.at("id").get<std::string>();
                    Definition.DisplayName = Parameter.value("displayName", Definition.Identifier);
                    Definition.Group = Parameter.value("group", "Parameters");
                    MaterialParameterType Type;
                    if (!MaterialDefinitionIO::ParseParameterType(Parameter.at("type").get<std::string>(), Type))
                    {
                        throw std::runtime_error("Unknown parameter type");
                    }
                    Definition.DefaultValue = ReadValue(Type, Parameter.at("default"));
                    Definition.bSrgb = Parameter.value("srgb", false);
                    const auto Address = Parameter.value("address", "Wrap");
                    const auto Filter = Parameter.value("filter", "Linear");
                    if ((Address != "Wrap" && Address != "Clamp") || (Filter != "Linear" && Filter != "Point"))
                    {
                        throw std::runtime_error("Unknown texture address or filter mode");
                    }
                    Definition.bClamp = Address == "Clamp";
                    Definition.bPointFiltering = Filter == "Point";
                    if (Parameter.contains("range"))
                    {
                        Definition.bHasRange = true;
                        Definition.Minimum = Parameter.at("range").at(0).get<double>();
                        Definition.Maximum = Parameter.at("range").at(1).get<double>();
                    }
                    Result.Values[Definition.Identifier] = Definition.DefaultValue;
                    Result.ValueOrigins[Definition.Identifier] = Current;
                    Result.Definition.Parameters.push_back(Definition);
                }
                if (Document.contains("source"))
                {
                    Result.Definition.Source = ReadKey(Document.at("source"));
                    AssetRegistryEntry SourceEntry;
                    if (!Registry.TryResolveKey(Result.Definition.Source, SourceEntry) || SourceEntry.Metadata.Type != ShaderSourceAssetType)
                    {
                        throw std::runtime_error("Material shader source is missing or has the wrong type");
                    }
                    Result.Definition.PreparedSource = ReadSource(SourceEntry);
                }
                else if (Sources.find(Current.Asset.ToString()) != Sources.end())
                {
                    Result.Definition.PreparedSource = Sources.at(Current.Asset.ToString());
                }
                else if (Result.Definition.PreparedSource.empty())
                {
                    throw std::runtime_error("Material requires a shader source");
                }
                const std::string Identity = Document.dump() + Result.Definition.PreparedSource;
                uint64_t Hash = 14695981039346656037ull;
                for (unsigned char Character : Identity)
                {
                    Hash = (Hash ^ Character) * 1099511628211ull;
                }
                Result.Definition.Revision = Hash;
                nlohmann::json ShaderDocument = MakeMaterialDocument(Result);
                for (const char* Field : {"alphaCutoff", "boundsExpansion", "animated", "source"})
                {
                    ShaderDocument.erase(Field);
                }
                for (auto& Parameter : ShaderDocument["parameters"])
                {
                    for (const char* Field : {"default", "displayName", "group", "range"})
                    {
                        Parameter.erase(Field);
                    }
                }
                const std::string ShaderIdentity = ShaderDocument.dump() + Result.Definition.PreparedSource;
                Result.Definition.ShaderRevision = ContentHash::FromBytes(ShaderIdentity.data(), ShaderIdentity.size()).Value;
            }
            else
            {
                throw std::runtime_error("Material parent must be Material or MaterialInstance");
            }
            Result.Definition.SourceFiles.push_back(Entry.AbsolutePath);
            Result.Definition.Dependencies.push_back(Current);
            Visiting.erase(Current.Asset.ToString());
        };
        ResolveDocument(Key);
        const std::string ResolvedIdentity = DocumentIdentity + Result.Definition.PreparedSource;
        Result.Definition.Revision = ContentHash::FromBytes(ResolvedIdentity.data(), ResolvedIdentity.size()).Value;
        for (const auto& Pair : Result.Values)
        {
            if (Pair.second.Type == MaterialParameterType::Texture2D && Pair.second.Texture.IsValid())
            {
                AssetRegistryEntry TextureEntry;
                if (!Registry.TryResolveKey(Pair.second.Texture, TextureEntry) || TextureEntry.Metadata.Type != TextureAssetType)
                {
                    throw std::runtime_error("Texture parameter references a missing or incompatible asset: " + Pair.first);
                }
            }
        }
        const AssetDiagnostic Validation = MaterialDefinitionIO::Validate(Result);
        if (Validation.HasError())
        {
            return Validation;
        }
        Result.Asset = Key;
        Result.VariantKey = MaterialDefinitionIO::BuildVariantKey(Result);
        OutMaterial = std::move(Result);
        return AssetDiagnostic::Ok();
    }
    catch (const std::exception& Exception)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialDocumentIO", Exception.what(), Key);
    }
}

nlohmann::json MaterialDocumentIO::MakeMaterialDocument(const ResolvedMaterial& Material)
{
    const auto& Definition = Material.Definition;
    const char* Domains[] = {"Surface", "PostProcess", "UserInterface"};
    const char* Models[] = {"Lit", "Unlit", "Toon"};
    const char* Blends[] = {"Opaque", "Masked", "Translucent", "Additive"};
    nlohmann::json Document = {
        {"schemaVersion", 2}, {"domain", Domains[static_cast<size_t>(Definition.Domain)]},
        {"shadingModel", Models[static_cast<size_t>(Definition.ShadingModel)]},
        {"blendMode", Blends[static_cast<size_t>(Definition.BlendMode)]},
        {"doubleSided", Definition.bDoubleSided}, {"castShadows", Definition.bCastShadows},
        {"receiveShadows", Definition.bReceiveShadows}, {"animated", Definition.bAnimated},
        {"modifyVertex", Definition.bModifyVertex}, {"boundsExpansion", Definition.BoundsExpansion},
        {"alphaCutoff", Definition.AlphaCutoff}, {"parameters", nlohmann::json::array()}
    };
    if (Definition.Source.IsValid())
    {
        Document["source"] = AssetMetadataIO::AssetRefToJson(Definition.Source);
    }
    if (Definition.PostProcessStage == MaterialPostProcessStage::AfterToneMapping)
    {
        Document["postProcessStage"] = "AfterToneMapping";
    }
    for (const auto& Parameter : Definition.Parameters)
    {
        nlohmann::json Entry = {
            {"id", Parameter.Identifier}, {"type", MaterialDefinitionIO::ParameterTypeName(Parameter.DefaultValue.Type)},
            {"displayName", Parameter.DisplayName}, {"group", Parameter.Group}, {"default", WriteValue(Parameter.DefaultValue)},
            {"srgb", Parameter.bSrgb}
        };
        if (Parameter.bHasRange)
        {
            Entry["range"] = {Parameter.Minimum, Parameter.Maximum};
        }
        if (Parameter.bClamp)
        {
            Entry["address"] = "Clamp";
        }
        if (Parameter.bPointFiltering)
        {
            Entry["filter"] = "Point";
        }
        Document["parameters"].push_back(Entry);
    }
    return Document;
}
