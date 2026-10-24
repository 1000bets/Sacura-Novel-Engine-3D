#pragma once
#include "Assets/AssetRegistry.h"

class PythonScriptDocument
{
public:
    explicit PythonScriptDocument(AssetRegistry& InRegistry) : Registry(InRegistry) {}
    bool Open(const AssetKey& Asset, std::string& Error);
    bool Edit(const std::string& Source, std::string& Error);
    void Undo();
    void Redo();
    bool Save(std::string& Error);
    bool IsDirty() const { return GetSource() != Saved; }
    const std::string& GetSource() const { return History[Position]; }
    const AssetRegistryEntry& GetEntry() const { return Entry; }
private:
    AssetRegistry& Registry;
    AssetRegistryEntry Entry;
    std::string Saved;
    std::vector<std::string> History{std::string{}};
    size_t Position = 0;
};
