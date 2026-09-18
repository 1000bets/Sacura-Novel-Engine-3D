#include "Assets/AssetMetadata.h"

#include <fstream>

namespace
{
constexpr int32_t SupportedSchemaVersion = 1;
}

nlohmann::json AssetMetadataIO::ToJson(const AssetMetadata& Metadata)
{
    nlohmann::json Document;
    Document["schemaVersion"] = Metadata.SchemaVersion;
    Document["guid"] = Metadata.Guid.ToString();
    Document["assetType"] = AssetTypeToString(Metadata.Type);
    Document["loadSettings"] = Metadata.LoadSettings;
    Document["sourceFingerprint"] = Metadata.SourceFingerprint.ToHex();
    Document["importInfo"] = Metadata.ImportInfo;

    nlohmann::json SubAssets = nlohmann::json::array();
    for (const SubAssetRecord& Record : Metadata.SubAssets)
    {
        nlohmann::json Entry;
        Entry["id"] = Record.Id.ToString();
        Entry["type"] = AssetTypeToString(Record.Type);
        Entry["name"] = Record.Name;
        Entry["selector"] = {
            {"kind", Record.Selector.Kind},
            {"index", Record.Selector.Index}
        };
        SubAssets.push_back(Entry);
    }
    Document["subAssets"] = SubAssets;
    return Document;
}

bool AssetMetadataIO::TryFromJson(const nlohmann::json& Document, AssetMetadata& OutMetadata, AssetDiagnostic& OutError)
{
    if (!Document.is_object())
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Metadata root must be an object");
        return false;
    }

    if (!Document.contains("schemaVersion") || !Document["schemaVersion"].is_number_integer())
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Missing schemaVersion");
        return false;
    }

    const int32_t SchemaVersion = Document["schemaVersion"].get<int32_t>();
    if (SchemaVersion != SupportedSchemaVersion)
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Unsupported schemaVersion");
        return false;
    }

    if (!Document.contains("guid") || !Document["guid"].is_string())
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Missing guid");
        return false;
    }

    Guid ParsedGuid{};
    if (!Guid::TryParse(Document["guid"].get<std::string>(), ParsedGuid))
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Invalid guid");
        return false;
    }

    if (!Document.contains("assetType") || !Document["assetType"].is_string())
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Missing assetType");
        return false;
    }

    AssetType ParsedType = AssetType::Unknown;
    if (!TryParseAssetType(Document["assetType"].get<std::string>(), ParsedType))
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Unknown assetType");
        return false;
    }

    AssetMetadata Metadata{};
    Metadata.SchemaVersion = SchemaVersion;
    Metadata.Guid = ParsedGuid;
    Metadata.Type = ParsedType;
    Metadata.LoadSettings = Document.value("loadSettings", nlohmann::json::object());
    Metadata.ImportInfo = Document.value("importInfo", nlohmann::json::object());

    if (Document.contains("sourceFingerprint") && Document["sourceFingerprint"].is_string())
    {
        const std::string Hex = Document["sourceFingerprint"].get<std::string>();
        try
        {
            Metadata.SourceFingerprint.Value = std::stoull(Hex, nullptr, 16);
        }
        catch (...)
        {
            OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Invalid sourceFingerprint");
            return false;
        }
    }

    if (Document.contains("subAssets"))
    {
        if (!Document["subAssets"].is_array())
        {
            OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "subAssets must be an array");
            return false;
        }

        for (const nlohmann::json& Entry : Document["subAssets"])
        {
            SubAssetRecord Record{};
            if (!Entry.contains("id") || !Entry["id"].is_string() || !Guid::TryParse(Entry["id"].get<std::string>(), Record.Id))
            {
                OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Invalid subAsset id");
                return false;
            }
            if (!Entry.contains("type") || !Entry["type"].is_string() || !TryParseAssetType(Entry["type"].get<std::string>(), Record.Type))
            {
                OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", "Invalid subAsset type");
                return false;
            }
            Record.Name = Entry.value("name", std::string{});
            if (Entry.contains("selector") && Entry["selector"].is_object())
            {
                Record.Selector.Kind = Entry["selector"].value("kind", std::string{});
                Record.Selector.Index = Entry["selector"].value("index", -1);
            }
            Metadata.SubAssets.push_back(Record);
        }
    }

    OutMetadata = std::move(Metadata);
    OutError = AssetDiagnostic::Ok();
    return true;
}

bool AssetMetadataIO::TryLoadFromFile(const std::string& AbsoluteMetaPath, AssetMetadata& OutMetadata, AssetDiagnostic& OutError)
{
    std::ifstream Stream(AbsoluteMetaPath);
    if (!Stream)
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "AssetMetadata", "Failed to open .meta", {}, AbsoluteMetaPath);
        return false;
    }

    nlohmann::json Document;
    try
    {
        Stream >> Document;
    }
    catch (const std::exception& Exception)
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetMetadata", Exception.what(), {}, AbsoluteMetaPath);
        return false;
    }

    return TryFromJson(Document, OutMetadata, OutError);
}

bool AssetMetadataIO::TrySaveToFile(const std::string& AbsoluteMetaPath, const AssetMetadata& Metadata, AssetDiagnostic& OutError)
{
    std::ofstream Stream(AbsoluteMetaPath, std::ios::trunc);
    if (!Stream)
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetMetadata", "Failed to write .meta", {}, AbsoluteMetaPath);
        return false;
    }

    Stream << ToJson(Metadata).dump(2);
    if (!Stream)
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InternalError, "AssetMetadata", "Failed while writing .meta", {}, AbsoluteMetaPath);
        return false;
    }

    OutError = AssetDiagnostic::Ok();
    return true;
}

nlohmann::json AssetMetadataIO::AssetRefToJson(const AssetKey& Key)
{
    nlohmann::json Document;
    Document["assetId"] = Key.Asset.ToString();
    if (Key.SubAsset.has_value())
    {
        Document["subAssetId"] = Key.SubAsset->ToString();
    }
    else
    {
        Document["subAssetId"] = nullptr;
    }
    return Document;
}

bool AssetMetadataIO::TryAssetRefFromJson(const nlohmann::json& Document, AssetKey& OutKey, AssetDiagnostic& OutError)
{
    if (!Document.is_object() || !Document.contains("assetId") || !Document["assetId"].is_string())
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetRef", "Invalid AssetRef JSON");
        return false;
    }

    AssetKey Key{};
    if (!Guid::TryParse(Document["assetId"].get<std::string>(), Key.Asset))
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetRef", "Invalid assetId");
        return false;
    }

    if (Document.contains("subAssetId") && !Document["subAssetId"].is_null())
    {
        if (!Document["subAssetId"].is_string())
        {
            OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetRef", "Invalid subAssetId");
            return false;
        }
        SubAssetId ParsedSub{};
        if (!Guid::TryParse(Document["subAssetId"].get<std::string>(), ParsedSub))
        {
            OutError = AssetDiagnostic::Fail(AssetErrorCode::InvalidMetadata, "AssetRef", "Invalid subAssetId");
            return false;
        }
        Key.SubAsset = ParsedSub;
    }

    OutKey = Key;
    OutError = AssetDiagnostic::Ok();
    return true;
}
