#include "Prefab/PrefabLibrary.h"

#include "Assets/AssetRegistry.h"
#include "Core/Threading/ThreadContext.h"
#include "World/Serialization/SceneSerializer.h"
#include "Prefab/PrefabGraphOps.h"

#include <algorithm>
#include <fstream>

PrefabLibrary& PrefabLibrary::Get()
{
    static PrefabLibrary Instance;
    return Instance;
}

void PrefabLibrary::BindRegistry(AssetRegistry* Registry)
{
    BoundRegistry = Registry;
    Cache.clear();
}

bool PrefabLibrary::TryGetPrefabPath(const AssetId& Prefab, std::filesystem::path& OutAbsolutePath) const
{
    if (BoundRegistry == nullptr || !Prefab.IsValid())
    {
        return false;
    }
    AssetRegistryEntry Entry{};
    if (!BoundRegistry->TryGetById(Prefab, Entry))
    {
        return false;
    }
    if (Entry.Metadata.Type != PrefabAssetType)
    {
        return false;
    }
    OutAbsolutePath = Entry.AbsolutePath;
    return true;
}

std::string PrefabLibrary::GetPrefabDisplayName(const AssetId& Prefab) const
{
    std::filesystem::path AbsolutePath;
    if (TryGetPrefabPath(Prefab, AbsolutePath))
    {
        return AbsolutePath.stem().string();
    }
    return "Missing Prefab " + Prefab.ToString();
}

bool PrefabLibrary::ReadDocumentFile(
    const std::filesystem::path& AbsolutePath,
    nlohmann::json& OutDocument,
    std::string& OutError)
{
    std::ifstream Input(AbsolutePath, std::ios::binary);
    if (!Input)
    {
        OutError = "Failed to open prefab file '" + AbsolutePath.generic_string() + "'";
        return false;
    }
    const std::string Text((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
    try
    {
        OutDocument = nlohmann::json::parse(Text);
    }
    catch (const std::exception& Exception)
    {
        OutError = std::string("Invalid prefab JSON: ") + Exception.what();
        return false;
    }

    if (!OutDocument.is_object())
    {
        OutError = "Prefab root must be a JSON object";
        return false;
    }
    if (!OutDocument.contains("format")
        || !OutDocument.at("format").is_string()
        || OutDocument.at("format").get<std::string>() != PrefabDocumentFormatId)
    {
        OutError = "Unsupported or missing prefab format id";
        return false;
    }
    if (!OutDocument.contains("formatVersion")
        || !OutDocument.at("formatVersion").is_number_unsigned()
        || OutDocument.at("formatVersion").get<uint32_t>() != PrefabDocumentFormatVersion)
    {
        OutError = "Unsupported prefab formatVersion";
        return false;
    }
    if (!OutDocument.contains("objects") || !OutDocument.at("objects").is_array())
    {
        OutError = "Prefab objects array is required";
        return false;
    }
    return true;
}

bool PrefabLibrary::WriteDocumentFile(
    const std::filesystem::path& AbsolutePath,
    const nlohmann::json& Document,
    std::string& OutError)
{
    const SceneSerializeResult Written = SceneSerializer::WriteTextFileAtomically(AbsolutePath, Document.dump(2));
    if (!Written.bOk)
    {
        OutError = Written.Error;
        return false;
    }
    return true;
}

nlohmann::json PrefabLibrary::MakeDocument(const std::string& Name, nlohmann::json Entries)
{
    nlohmann::json Document = nlohmann::json::object();
    Document["format"] = PrefabDocumentFormatId;
    Document["formatVersion"] = PrefabDocumentFormatVersion;
    Document["name"] = Name;
    Document["objects"] = std::move(Entries);
    return Document;
}

bool PrefabLibrary::ReadDocument(const AssetId& Prefab, nlohmann::json& OutDocument, std::string& OutError) const
{
    std::filesystem::path AbsolutePath;
    if (!TryGetPrefabPath(Prefab, AbsolutePath))
    {
        OutError = "Prefab asset " + Prefab.ToString() + " is not registered";
        return false;
    }
    return ReadDocumentFile(AbsolutePath, OutDocument, OutError);
}

bool PrefabLibrary::IsCacheEntryFresh(const CacheEntry& Entry) const
{
    for (const auto& Pair : Entry.FileTimes)
    {
        std::error_code Error;
        const std::filesystem::file_time_type Current = std::filesystem::last_write_time(Pair.first, Error);
        if (Error || Current != Pair.second)
        {
            return false;
        }
    }
    return true;
}

std::shared_ptr<const PrefabGraph> PrefabLibrary::Resolve(const AssetId& Prefab, std::string& OutError)
{
    std::vector<AssetId> Stack;
    return ResolveWithStack(Prefab, Stack, OutError);
}

std::shared_ptr<const PrefabGraph> PrefabLibrary::ResolveWithStack(
    const AssetId& Prefab,
    std::vector<AssetId>& ResolveStack,
    std::string& OutError)
{
    if (std::find(ResolveStack.begin(), ResolveStack.end(), Prefab) != ResolveStack.end())
    {
        OutError = "Prefab nesting cycle detected at " + GetPrefabDisplayName(Prefab);
        return nullptr;
    }

    const auto Cached = Cache.find(Prefab);
    if (Cached != Cache.end())
    {
        if (IsCacheEntryFresh(Cached->second))
        {
            return Cached->second.Graph;
        }
        Cache.erase(Cached);
    }

    std::filesystem::path AbsolutePath;
    if (!TryGetPrefabPath(Prefab, AbsolutePath))
    {
        OutError = "Prefab asset " + Prefab.ToString() + " is not registered";
        return nullptr;
    }

    nlohmann::json Document;
    if (!ReadDocumentFile(AbsolutePath, Document, OutError))
    {
        return nullptr;
    }

    ResolveStack.push_back(Prefab);
    auto Graph = std::make_shared<PrefabGraph>();
    PrefabGraphOps::ExpandOptions Options;
    Options.bCanonicalizeIds = true;
    Options.bAllowMissingPrefabs = false;
    const bool bExpanded = PrefabGraphOps::Expand(Document.at("objects"), Options, ResolveStack, *Graph, OutError);
    ResolveStack.pop_back();
    if (!bExpanded)
    {
        OutError = GetPrefabDisplayName(Prefab) + ": " + OutError;
        return nullptr;
    }

    size_t RootCount = 0;
    for (const PrefabGraphObject& Object : Graph->Objects)
    {
        if (Object.ParentId.empty())
        {
            ++RootCount;
            Graph->RootId = Object.Id;
        }
    }
    if (RootCount != 1)
    {
        OutError = GetPrefabDisplayName(Prefab) + ": prefab must contain exactly one root object";
        return nullptr;
    }

    CacheEntry Entry;
    std::error_code TimeError;
    Entry.FileTimes[AbsolutePath.string()] = std::filesystem::last_write_time(AbsolutePath, TimeError);
    for (const AssetId& Dependency : Graph->Dependencies)
    {
        std::filesystem::path DependencyPath;
        if (TryGetPrefabPath(Dependency, DependencyPath))
        {
            Entry.FileTimes[DependencyPath.string()] = std::filesystem::last_write_time(DependencyPath, TimeError);
        }
    }
    Entry.Graph = Graph;
    Cache[Prefab] = Entry;
    return Graph;
}

bool PrefabLibrary::DependsOn(const AssetId& Prefab, const AssetId& Candidate)
{
    if (Prefab == Candidate)
    {
        return true;
    }
    std::string Error;
    std::shared_ptr<const PrefabGraph> Graph = Resolve(Prefab, Error);
    return Graph != nullptr && Graph->DependsOn(Candidate);
}

void PrefabLibrary::ClearCache()
{
    Cache.clear();
}

void PrefabLibrary::Invalidate(const AssetId& ChangedPrefab)
{
    for (auto Iterator = Cache.begin(); Iterator != Cache.end();)
    {
        const bool bAffected = Iterator->first == ChangedPrefab
            || (Iterator->second.Graph != nullptr && Iterator->second.Graph->DependsOn(ChangedPrefab));
        if (bAffected)
        {
            Iterator = Cache.erase(Iterator);
        }
        else
        {
            ++Iterator;
        }
    }
}

void PrefabLibrary::NotifyPrefabChanged(const AssetId& ChangedPrefab)
{
    AssertGameThread();
    Invalidate(ChangedPrefab);
    if (bNotifying)
    {
        return;
    }
    bNotifying = true;
    // Copy: a listener may unregister itself (e.g. a prefab editor window closing).
    const auto ListenersCopy = Listeners;
    for (const auto& Pair : ListenersCopy)
    {
        if (Pair.second)
        {
            Pair.second(ChangedPrefab);
        }
    }
    bNotifying = false;
}

uint64_t PrefabLibrary::AddChangeListener(ChangeListener Listener)
{
    const uint64_t Id = NextListenerId++;
    Listeners.emplace(Id, std::move(Listener));
    return Id;
}

void PrefabLibrary::RemoveChangeListener(uint64_t ListenerId)
{
    Listeners.erase(ListenerId);
}
