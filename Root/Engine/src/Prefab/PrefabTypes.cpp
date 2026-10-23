#include "Prefab/PrefabTypes.h"

#include "Assets/Guid.h"

#include <algorithm>

namespace
{
Guid HashTextToGuid(const std::string& Text)
{
    // Two independent 64-bit FNV-1a lanes -> 128 bits. Deterministic across platforms.
    uint64_t LaneA = 1469598103934665603ull;
    uint64_t LaneB = 0x84222325cbf29ce4ull;
    for (unsigned char Character : Text)
    {
        LaneA ^= Character;
        LaneA *= 1099511628211ull;
        LaneB ^= static_cast<uint64_t>(Character) + 0x9eu;
        LaneB *= 0x100000001b3ull;
    }
    Guid Result{};
    for (int Index = 0; Index < 8; ++Index)
    {
        Result.Bytes[static_cast<size_t>(Index)] = static_cast<uint8_t>(LaneA >> (Index * 8));
        Result.Bytes[static_cast<size_t>(Index + 8)] = static_cast<uint8_t>(LaneB >> (Index * 8));
    }
    if (!Result.IsValid())
    {
        Result.Bytes[0] = 1;
    }
    return Result;
}

Guid ToCanonicalGuid(const std::string& Id)
{
    Guid Parsed{};
    if (Guid::TryParse(Id, Parsed))
    {
        return Parsed;
    }
    return HashTextToGuid(Id);
}
}

namespace PrefabIds
{
std::string Canonicalize(const std::string& Id)
{
    if (Id.empty())
    {
        return {};
    }
    return ToCanonicalGuid(Id).ToString();
}

std::string Combine(const std::string& A, const std::string& B)
{
    const Guid Left = ToCanonicalGuid(A);
    const Guid Right = ToCanonicalGuid(B);
    Guid Result{};
    for (size_t Index = 0; Index < Result.Bytes.size(); ++Index)
    {
        Result.Bytes[Index] = static_cast<uint8_t>(Left.Bytes[Index] ^ Right.Bytes[Index]);
    }
    if (!Result.IsValid())
    {
        // A == B would produce the nil GUID; this cannot happen for distinct random GUIDs,
        // but keep the id usable instead of producing an invalid one.
        return HashTextToGuid(A + "#" + B).ToString();
    }
    return Result.ToString();
}

std::string Generate()
{
    return Guid::Generate().ToString();
}
}

const PrefabGraphObject* PrefabGraph::FindObject(const std::string& Id) const
{
    const auto Found = ObjectIndex.find(Id);
    if (Found != ObjectIndex.end() && Found->second < Objects.size() && Objects[Found->second].Id == Id)
    {
        return &Objects[Found->second];
    }
    for (const PrefabGraphObject& Candidate : Objects)
    {
        if (Candidate.Id == Id)
        {
            return &Candidate;
        }
    }
    return nullptr;
}

PrefabGraphObject* PrefabGraph::FindObject(const std::string& Id)
{
    return const_cast<PrefabGraphObject*>(static_cast<const PrefabGraph*>(this)->FindObject(Id));
}

const PrefabGraphInstance* PrefabGraph::FindInstance(const std::string& InstanceId) const
{
    for (const PrefabGraphInstance& Instance : Instances)
    {
        if (Instance.InstanceId == InstanceId)
        {
            return &Instance;
        }
    }
    return nullptr;
}

bool PrefabGraph::DependsOn(const AssetId& Asset) const
{
    return std::find(Dependencies.begin(), Dependencies.end(), Asset) != Dependencies.end();
}

void PrefabGraph::RebuildIndex()
{
    ObjectIndex.clear();
    ObjectIndex.reserve(Objects.size());
    for (size_t Index = 0; Index < Objects.size(); ++Index)
    {
        ObjectIndex[Objects[Index].Id] = Index;
    }
}
