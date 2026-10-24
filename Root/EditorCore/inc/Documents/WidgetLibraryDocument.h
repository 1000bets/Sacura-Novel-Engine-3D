#pragma once
#include "Assets/AssetRegistry.h"
#include <nlohmann/json.hpp>

class WidgetLibraryDocument
{
public:
    explicit WidgetLibraryDocument(AssetRegistry& InRegistry) : Registry(InRegistry) {}
    static bool Validate(const nlohmann::json& Document, std::string& Error);
    static bool Read(const std::filesystem::path& Path, nlohmann::json& Document, std::string& Error);
    bool Open(const AssetKey& Asset, std::string& Error);
    bool Edit(const nlohmann::json& Document, std::string& Error);
    bool Save(std::string& Error);
    void Undo();
    void Redo();
    bool CanUndo() const { return Position > 0; }
    bool CanRedo() const { return Position + 1 < History.size(); }
    bool IsDirty() const { return GetDocument() != Saved; }
    const nlohmann::json& GetDocument() const { return History[Position]; }
private:
    AssetRegistry& Registry;
    AssetRegistryEntry Entry;
    nlohmann::json Saved;
    ContentHash Fingerprint;
    std::vector<nlohmann::json> History;
    size_t Position = 0;
};
