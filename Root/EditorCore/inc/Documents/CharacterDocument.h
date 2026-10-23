#pragma once

#include "Characters/CharacterDefinition.h"
#include "Assets/AssetManager.h"
#include "Commands/EditorCommandStack.h"
#include <map>
class Scene;

class CharacterDocument
{
public:
    bool Open(AssetRegistry& Registry, const AssetRegistryEntry& Entry, std::string& Error);
    bool Apply(const nlohmann::json& Candidate, std::string& Error);
    bool Save(AssetManager& Assets, std::string& Error);
    void Undo();
    void Redo();
    bool IsDirty() const { return Data.ToJson() != Saved; }
    bool IsReadOnly() const { return Entry.Mount == AssetMount::Engine; }
    const CharacterDefinition& GetData() const { return Data; }
    const AssetRegistryEntry& GetEntry() const { return Entry; }
    static bool Create(AssetRegistry& Registry, const CharacterDefinition& Data, AssetRegistryEntry& Entry, std::string& Error);
    bool ImportModel(AssetRegistry& Registry, const std::filesystem::path& Source, bool bAnimationsOnly, std::string& Error);
private:
    AssetRegistry* Registry = nullptr;
    AssetRegistryEntry Entry;
    CharacterDefinition Data;
    nlohmann::json Saved;
    std::vector<CharacterDefinition> Past;
    std::vector<CharacterDefinition> Future;
};

class CharacterDocuments
{
public:
    std::shared_ptr<CharacterDocument> Open(AssetRegistry& Registry, const AssetRegistryEntry& Entry, std::string& Error);
    bool HasDirtyDocuments() const;
    bool SaveAll(AssetManager& Assets, std::string& Error);
    bool Delete(AssetRegistry& Registry, AssetManager& Assets, const AssetRegistryEntry& Entry, std::string& Error);
private:
    std::map<std::string, std::shared_ptr<CharacterDocument>> Documents;
};

bool InstantiateCharacter(Scene* World, EditorCommandStack& Commands, const AssetRegistryEntry& Entry,
    ObjectHandle& Created, std::string& Error);
