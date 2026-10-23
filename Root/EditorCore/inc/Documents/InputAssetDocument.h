#pragma once

#include "Input/InputDocument.h"
#include "Assets/AssetRegistry.h"

class InputAssetDocument
{
public:
    bool Open(AssetRegistry& Registry, const AssetRegistryEntry& Entry, std::string& Error);
    bool Apply(nlohmann::json Candidate, std::string& Error);
    bool Save(std::string& Error);
    void Undo();
    void Redo();
    bool IsDirty() const { return Data != Saved; }
    bool IsReadOnly() const { return Entry.Mount == AssetMount::Engine; }
    const nlohmann::json& GetData() const { return Data; }
    const AssetRegistryEntry& GetEntry() const { return Entry; }
    static bool Create(AssetRegistry& Registry, const std::string& Name, bool bContext,
        AssetRegistryEntry& Entry, std::string& Error);

private:
    AssetRegistry* Registry = nullptr;
    AssetRegistryEntry Entry;
    nlohmann::json Data;
    nlohmann::json Saved;
    std::vector<nlohmann::json> Past;
    std::vector<nlohmann::json> Future;
};
