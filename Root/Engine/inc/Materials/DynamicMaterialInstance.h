#pragma once

#include "Materials/MaterialDefinition.h"
#include <memory>

class DynamicMaterialInstance
{
public:
    explicit DynamicMaterialInstance(ResolvedMaterial Material, bool bFollowAssetChanges = true);
    AssetDiagnostic SetParameter(const std::string& Identifier, const MaterialParameterValue& Value);
    bool GetParameter(const std::string& Identifier, MaterialParameterValue& OutValue) const;
    bool ResetParameter(const std::string& Identifier);
    const ResolvedMaterial& GetResolvedMaterial() const;
    uint64_t GetRevision() const { return Revision; }
    bool FollowsAssetChanges() const { return bFollowAssetChanges; }
    AssetDiagnostic Rebase(const ResolvedMaterial& Material);

private:
    ResolvedMaterial Parent;
    ResolvedMaterial Current;
    uint64_t Revision = 1;
    std::map<std::string, MaterialParameterValue> Overrides;
    bool bFollowAssetChanges = true;
};
