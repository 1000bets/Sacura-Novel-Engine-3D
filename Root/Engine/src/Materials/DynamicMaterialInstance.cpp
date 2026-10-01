#include "Materials/DynamicMaterialInstance.h"
#include "Core/Threading/ThreadContext.h"

DynamicMaterialInstance::DynamicMaterialInstance(ResolvedMaterial Material, bool bFollowChanges)
    : Parent(std::move(Material)), Current(Parent), bFollowAssetChanges(bFollowChanges)
{
    AssertGameThread();
}

AssetDiagnostic DynamicMaterialInstance::SetParameter(const std::string& Identifier, const MaterialParameterValue& Value)
{
    AssertGameThread();
    const auto Found = Current.Values.find(Identifier);
    if (Found == Current.Values.end() || Found->second.Type != Value.Type)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::TypeMismatch, "DynamicMaterialInstance", "Unknown parameter or incompatible type: " + Identifier);
    }
    if (Value.Type == MaterialParameterType::StaticSwitch)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "DynamicMaterialInstance", "Static switches are fixed at creation");
    }
    const MaterialParameterValue Previous = Found->second;
    Found->second = Value;
    const AssetDiagnostic Diagnostic = MaterialDefinitionIO::Validate(Current);
    if (Diagnostic.HasError())
    {
        Found->second = Previous;
        return Diagnostic;
    }
    Overrides[Identifier] = Value;
    ++Revision;
    return AssetDiagnostic::Ok();
}

bool DynamicMaterialInstance::GetParameter(const std::string& Identifier, MaterialParameterValue& OutValue) const
{
    AssertGameThread();
    const auto Found = Current.Values.find(Identifier);
    if (Found == Current.Values.end())
    {
        return false;
    }
    OutValue = Found->second;
    return true;
}

bool DynamicMaterialInstance::ResetParameter(const std::string& Identifier)
{
    AssertGameThread();
    const auto Found = Parent.Values.find(Identifier);
    if (Found == Parent.Values.end() || Found->second.Type == MaterialParameterType::StaticSwitch)
    {
        return false;
    }
    Current.Values[Identifier] = Found->second;
    Overrides.erase(Identifier);
    ++Revision;
    return true;
}

AssetDiagnostic DynamicMaterialInstance::Rebase(const ResolvedMaterial& Material)
{
    AssertGameThread();
    ResolvedMaterial Next = Material;
    for (const auto& Pair : Current.Values)
    {
        const auto Found = Next.Values.find(Pair.first);
        if (Pair.second.Type == MaterialParameterType::StaticSwitch && Found != Next.Values.end()
            && Found->second.Type == MaterialParameterType::StaticSwitch)
        {
            Found->second = Pair.second;
        }
    }
    for (const auto& Pair : Overrides)
    {
        const auto Found = Next.Values.find(Pair.first);
        if (Found == Next.Values.end() || Found->second.Type != Pair.second.Type)
        {
            Next.Warnings.push_back("Unused runtime override: " + Pair.first);
            continue;
        }
        Found->second = Pair.second;
    }
    const auto Diagnostic = MaterialDefinitionIO::Validate(Next);
    if (Diagnostic.HasError())
    {
        return Diagnostic;
    }
    Next.VariantKey = MaterialDefinitionIO::BuildVariantKey(Next);
    Parent = Material;
    Current = std::move(Next);
    ++Revision;
    return AssetDiagnostic::Ok();
}

const ResolvedMaterial& DynamicMaterialInstance::GetResolvedMaterial() const
{
    AssertGameThread();
    return Current;
}
