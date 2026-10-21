#pragma once

#include "Assets/AssetRegistry.h"
#include "Assets/ContentHash.h"
#include "Story/StoryTypes.h"
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

    bool AddNode(StoryNodeKind Kind, StoryGraphPosition Position, StoryNodeId& Created, std::string& Error);
    bool DeleteNodes(const std::vector<StoryNodeId>& Identities, std::string& Error);
    bool ReplaceNode(const StoryNode& Node, std::string& Error);
    // Port -1 is the single continuation; nonnegative ports address choice rows.
    bool Connect(const StoryNodeId& Source, int Port, const StoryNodeId& Target, std::string& Error);
    bool MoveNodes(const std::map<StoryNodeId, StoryGraphPosition>& Positions, std::string& Error);
    bool SetStart(const StoryNodeId& Identity, std::string& Error);
    bool ValidateForPlayback(std::string& Error) const;

  private:
    struct Revision
    {
        nlohmann::json Data;
        std::string Label;
    };
    bool Apply(const char* Label, const std::function<bool(nlohmann::json&, std::string&)>& Edit, std::string& Error);
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
