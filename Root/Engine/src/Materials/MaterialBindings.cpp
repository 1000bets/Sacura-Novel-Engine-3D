#include "Materials/MaterialBindings.h"
#include "Assets/AssetMetadata.h"
#include <cmath>
#include <set>

bool MaterialSlotOverrides::operator==(const MaterialSlotOverrides& Other) const
{
    if (Entries.size() != Other.Entries.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < Entries.size(); ++Index)
    {
        if (Entries[Index].Slot != Other.Entries[Index].Slot || Entries[Index].Material != Other.Entries[Index].Material)
        {
            return false;
        }
    }
    return true;
}

bool MaterialPostProcessEffects::operator==(const MaterialPostProcessEffects& Other) const
{
    if (Entries.size() != Other.Entries.size())
    {
        return false;
    }
    for (size_t Index = 0; Index < Entries.size(); ++Index)
    {
        const auto& First = Entries[Index];
        const auto& Second = Other.Entries[Index];
        if (First.Material != Second.Material || First.bEnabled != Second.bEnabled || First.Intensity != Second.Intensity)
        {
            return false;
        }
    }
    return true;
}

ReflectionDiagnostic ReflectionValueCodec<MaterialSlotOverrides>::ToReflected(const MaterialSlotOverrides& Source, ReflectedValue& OutValue)
{
    nlohmann::json Document = nlohmann::json::array();
    for (const auto& Entry : Source.Entries)
    {
        Document.push_back({{"slot", Entry.Slot}, {"material", AssetMetadataIO::AssetRefToJson(Entry.Material)}});
    }
    OutValue = ReflectedValue::MakeString(Document.dump());
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionValueCodec<MaterialSlotOverrides>::FromReflected(MaterialSlotOverrides& Destination, const ReflectedValue& InValue)
{
    try
    {
        const auto Document = nlohmann::json::parse(InValue.StringValue);
        if (!Document.is_array())
        {
            throw std::runtime_error("Material slots must be an array");
        }
        MaterialSlotOverrides Result;
        std::set<int32_t> Slots;
        for (const auto& Item : Document)
        {
            MaterialSlotOverride Entry;
            Entry.Slot = Item.at("slot").get<int32_t>();
            AssetDiagnostic Diagnostic;
            if (Entry.Slot < 0 || !Slots.insert(Entry.Slot).second
                || !AssetMetadataIO::TryAssetRefFromJson(Item.at("material"), Entry.Material, Diagnostic))
            {
                throw std::runtime_error("Invalid or duplicate material slot");
            }
            Result.Entries.push_back(Entry);
        }
        Destination = std::move(Result);
        return ReflectionDiagnostic::Ok();
    }
    catch (const std::exception& Exception)
    {
        return ReflectionDiagnostic::Fail(Exception.what());
    }
}

ReflectionDiagnostic ReflectionValueCodec<MaterialPostProcessEffects>::ToReflected(const MaterialPostProcessEffects& Source, ReflectedValue& OutValue)
{
    nlohmann::json Document = nlohmann::json::array();
    for (const auto& Entry : Source.Entries)
    {
        Document.push_back({{"material", AssetMetadataIO::AssetRefToJson(Entry.Material)}, {"enabled", Entry.bEnabled}, {"intensity", Entry.Intensity}});
    }
    OutValue = ReflectedValue::MakeString(Document.dump());
    return ReflectionDiagnostic::Ok();
}

ReflectionDiagnostic ReflectionValueCodec<MaterialPostProcessEffects>::FromReflected(MaterialPostProcessEffects& Destination, const ReflectedValue& InValue)
{
    try
    {
        const auto Document = nlohmann::json::parse(InValue.StringValue);
        if (!Document.is_array())
        {
            throw std::runtime_error("Postprocess effects must be an array");
        }
        MaterialPostProcessEffects Result;
        for (const auto& Item : Document)
        {
            MaterialPostProcessEffect Entry;
            AssetDiagnostic Diagnostic;
            if (!AssetMetadataIO::TryAssetRefFromJson(Item.at("material"), Entry.Material, Diagnostic))
            {
                throw std::runtime_error("Invalid postprocess material");
            }
            Entry.bEnabled = Item.value("enabled", true);
            Entry.Intensity = Item.value("intensity", 1.f);
            if (!std::isfinite(Entry.Intensity) || Entry.Intensity < 0.f || Entry.Intensity > 1.f)
            {
                throw std::runtime_error("Postprocess intensity must be between zero and one");
            }
            Result.Entries.push_back(Entry);
        }
        Destination = std::move(Result);
        return ReflectionDiagnostic::Ok();
    }
    catch (const std::exception& Exception)
    {
        return ReflectionDiagnostic::Fail(Exception.what());
    }
}
