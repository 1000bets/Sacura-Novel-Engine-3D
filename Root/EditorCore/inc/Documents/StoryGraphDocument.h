#pragma once

#include "Assets/AssetRegistry.h"
#include "Assets/ContentHash.h"
#include "Story/StoryTypes.h"
#include "Documents/StoryEventPackage.h"
#include "Reflection/Json/ReflectionJson.h"
#include <functional>
#include <map>
#include <nlohmann/json.hpp>

struct StoryGraphPosition
{
    double X = 0;
    double Y = 0;
};

// An authored document and its command history. No QApplication or runtime world is required.
// Revisions own their data; failed edits, opens and saves leave the previous revision intact.
class StoryGraphDocument
{
  public:
    const AssetRegistry* GetAssetRegistry() const { return Registry; }
    bool Open(const std::filesystem::path& Path, std::string& Error);
    bool Open(AssetRegistry& Registry, const AssetKey& Asset, std::string& Error);
    bool Save(std::string& Error);
    void Close();
    bool IsOpen() const
    {
        return !History.empty();
    }
    bool IsDirty() const;
    bool IsReadOnly() const;
    bool HasExternalChanges() const;
    bool CanUndo() const
    {
        return IsOpen() && Current > 0 && !IsReadOnly();
    }
    bool CanRedo() const
    {
        return IsOpen() && Current + 1 < History.size() && !IsReadOnly();
    }
    bool Undo();
    bool Redo();
    const StoryDocument& GetStory() const
    {
        return Story;
    }
    const StoryNode* FindNode(const StoryNodeId& Identity) const;
    std::map<StoryNodeId, StoryGraphPosition> GetPositions() const;
    void SetInitialPositions(std::map<StoryNodeId, StoryGraphPosition> Positions)
    {
        InitialPositions = std::move(Positions);
    }
    const std::filesystem::path& GetPath() const
    {
        return Path;
    }
    const AssetKey& GetAsset() const
    {
        return Asset;
    }
    const std::string& GetUndoLabel() const;
    const std::string& GetRedoLabel() const;

    bool AddNarrativeNode(const std::string& Type, StoryGraphPosition Position, StoryNodeId& Created, std::string& Error);
    bool ConnectData(const StoryNodeId& Source, const std::string& Output, const StoryNodeId& Target, const std::string& Input, std::string& Error);
    bool SetNarrativeInput(const StoryNodeId& Node, const std::string& Input, const nlohmann::json& Value,
        std::string& Error, const ReflectionJson::ObjectResolver& ResolveObject = {});
    bool AddNode(StoryNodeKind Kind, StoryGraphPosition Position, StoryNodeId& Created, std::string& Error);
    bool DeleteNodes(const std::vector<StoryNodeId>& Identities, std::string& Error);
    bool ReplaceNode(const StoryNode& Node, std::string& Error);
    // Port -1 is the single continuation; nonnegative ports address choice rows.
    bool Connect(const StoryNodeId& Source, int Port, const StoryNodeId& Target, std::string& Error);
    bool MoveNodes(const std::map<StoryNodeId, StoryGraphPosition>& Positions, std::string& Error);
    bool SetStart(const StoryNodeId& Identity, std::string& Error);
    bool ValidateForPlayback(std::string& Error) const;
    bool AddEvent(const StoryNodeId& Node, StoryEventPhase Phase, const std::string& SourceEvent,
                  std::string& Binding, std::string& Error, const std::string& Batch = {});
    bool RemoveEventBinding(const StoryNodeId& Node, const std::string& Binding, std::string& Error);
    bool SetEventPlacement(const StoryNodeId& Node, const std::string& Binding, StoryEventPhase Phase,
                           StoryEventJoin Join, std::string& Error, const std::string& Batch = {}, const std::string& BeforeBinding = {});
    bool SetEventBatchMode(const StoryNodeId& Node, const std::string& Batch, bool bParallel, std::string& Error);
    bool MoveEventBinding(const StoryNodeId& Node, const std::string& Binding, int Offset, std::string& Error);
    bool ReplaceEvent(StoryEventDefinition Event, std::string& Error);
    bool SetEventOverrides(const StoryNodeId& Node, const std::string& Binding,
                           const std::vector<StoryActionDefinition>& Actions, std::string& Error);
    bool ExportEventPackage(const std::string& Event, const std::vector<StoryActionDefinition>& Overrides,
        const StoryEventPackageMetadata& Metadata, const std::map<std::string, std::string>& ObjectNames,
        std::string& Text, std::string& Error) const;
    bool ImportEventPackage(const std::string& Text, const std::map<std::string, std::string>& Bindings,
        const std::vector<std::string>& AvailableObjects, const StoryNodeId& Node, StoryEventPhase Phase,
        std::string& CreatedBinding, std::string& Error);

  private:
    struct Revision
    {
        nlohmann::json Data;
        std::string Label;
    };
    bool Apply(const char* Label, const std::function<bool(nlohmann::json&, std::string&)>& Edit, std::string& Error);
    bool EditStaging(const char* Label, const std::function<bool(StoryDocument&, std::string&)>& Edit, std::string& Error);
    static bool Decode(const nlohmann::json& Data, StoryDocument& Story, std::string& Error);
    const nlohmann::json& Data() const
    {
        return History.at(Current).Data;
    }
    std::filesystem::path Path;
    AssetRegistry* Registry = nullptr;
    AssetKey Asset;
    ContentHash Fingerprint;
    std::vector<Revision> History;
    size_t Current = 0;
    nlohmann::json Saved;
    StoryDocument Story;
    std::map<StoryNodeId, StoryGraphPosition> InitialPositions;
};
