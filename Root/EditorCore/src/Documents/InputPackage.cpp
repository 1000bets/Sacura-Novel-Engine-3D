#include "Documents/InputPackage.h"
#include "Input/InputSession.h"
#include <set>

bool InputPackage::Export(AssetRegistry& Registry, const AssetRegistryEntry& Root,
    const std::filesystem::path& Destination, std::string& Error)
{
    InputConfiguration Configuration;
    if (!InputDocument::LoadConfiguration(Registry, Configuration, Error))
    {
        return false;
    }
    nlohmann::json Package{{"Version", 1}, {"Format", "sakura.input-package"},
        {"Name", std::filesystem::path(Root.RelativePath).stem().u8string()},
        {"Description", "Input actions and mapping contexts"}, {"Author", ""}, {"License", "Unspecified"},
        {"PackageId", Guid::Generate().ToString()}, {"PackageVersion", "1.0.0"},
        {"Requires", nlohmann::json::array({"sakura.input@1"})},
        {"Root", Root.Metadata.Guid.ToString()}, {"Assets", nlohmann::json::array()}};
    std::vector<std::string> Pending{Root.Metadata.Guid.ToString()};
    std::set<std::string> Visited;
    while (!Pending.empty())
    {
        const auto Identifier = Pending.back();
        Pending.pop_back();
        if (!Visited.insert(Identifier).second || Identifier.rfind("engine.input.", 0) == 0)
        {
            continue;
        }
        Guid AssetIdentifier;
        AssetRegistryEntry Entry;
        if (!Guid::TryParse(Identifier, AssetIdentifier) || !Registry.TryGetById(AssetIdentifier, Entry))
        {
            Error = "Missing input package dependency: " + Identifier;
            return false;
        }
        nlohmann::json Document;
        if (!InputDocument::Read(Entry.AbsolutePath, Document, Error))
        {
            return false;
        }
        if (Entry.Metadata.Type == InputMappingContextAssetType)
        {
            for (const auto& Binding : Document.at("Context").get<InputMappingContext>().Bindings)
            {
                Pending.push_back(Binding.Action);
            }
        }
        else if (Entry.Metadata.Type == InputActionAssetType)
        {
            const auto Chord = Document.at("Action").get<InputAction>().ChordAction;
            if (!Chord.empty())
            {
                Pending.push_back(Chord);
            }
        }
        else
        {
            Error = "Unsupported input package dependency";
            return false;
        }
        Package["Assets"].push_back({{"Identifier", Identifier}, {"Type", Entry.Metadata.Type.GetIdentifier()},
            {"Document", Document}, {"Origin", Entry.Metadata.ImportInfo}});
    }
    if (!Validate(Package, Error))
    {
        return false;
    }
    return InputDocument::Write(Destination, Package, Error);
}

bool InputPackage::Validate(const nlohmann::json& Package, std::string& Error)
{
    try
    {
        if (Package.at("Version") != 1 || Package.at("Format") != "sakura.input-package"
            || Package.at("Requires") != nlohmann::json::array({"sakura.input@1"})
            || !Package.at("Assets").is_array() || Package.at("Assets").size() > 10000)
        {
            Error = "Unsupported input package";
            return false;
        }
        for (const auto* Field : {"Name", "Description", "Author", "License", "PackageId", "PackageVersion", "Root"})
        {
            Package.at(Field).get<std::string>();
        }
        auto Configuration = InputDocument::CreateDefaults();
        std::set<std::string> Identifiers;
        for (const auto& Asset : Package.at("Assets"))
        {
            const auto Identifier = Asset.at("Identifier").get<std::string>();
            Guid Parsed;
            if (!Guid::TryParse(Identifier, Parsed) || !Parsed.IsValid() || !Identifiers.insert(Identifier).second)
            {
                Error = "Invalid or duplicate input package asset identifier";
                return false;
            }
            const auto& Document = Asset.at("Document");
            if (Document.at("Version") != 1)
            {
                Error = "Unsupported input asset version";
                return false;
            }
            if (Asset.at("Type") == "InputAction")
            {
                auto Action = Document.at("Action").get<InputAction>();
                Action.Identifier = Identifier;
                Configuration.Actions.push_back(Action);
            }
            else if (Asset.at("Type") == "InputMappingContext")
            {
                auto Context = Document.at("Context").get<InputMappingContext>();
                Context.Identifier = Identifier;
                Configuration.Contexts.push_back(Context);
            }
            else
            {
                Error = "Input packages cannot install code or unrelated assets";
                return false;
            }
        }
        if (!Identifiers.count(Package.at("Root").get<std::string>()))
        {
            Error = "Missing input package root";
            return false;
        }
        return InputSession::Validate(Configuration, Error);
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool InputPackage::Import(AssetRegistry& Registry, const nlohmann::json& Package,
    std::vector<AssetRegistryEntry>& Imported, std::string& Error)
{
    if (!Validate(Package, Error) || Registry.GetGameContentRoot().empty())
    {
        return false;
    }
    const auto ImportIdentifier = Guid::Generate().ToString();
    const auto Root = Registry.GetGameContentRoot();
    const auto Staging = Root.parent_path() / "Saved" / ("InputImport-" + ImportIdentifier);
    const auto Destination = Root / "Input" / ("Imported-" + ImportIdentifier);
    std::map<std::string, std::string> Remapping;
    for (const auto& Asset : Package.at("Assets"))
    {
        Remapping.emplace(Asset.at("Identifier").get<std::string>(), Guid::Generate().ToString());
    }
    bool bPublished = false;
    try
    {
        std::filesystem::create_directories(Staging);
        for (const auto& Asset : Package.at("Assets"))
        {
            auto Document = Asset.at("Document");
            const auto Identifier = Remapping.at(Asset.at("Identifier").get<std::string>());
            std::string Extension = ".inputaction";
            if (Asset.at("Type") == "InputAction")
            {
                auto Action = Document.at("Action").get<InputAction>();
                Action.Identifier = Identifier;
                if (Remapping.count(Action.ChordAction))
                {
                    Action.ChordAction = Remapping.at(Action.ChordAction);
                }
                Document["Action"] = Action;
            }
            else
            {
                Extension = ".inputcontext";
                auto Context = Document.at("Context").get<InputMappingContext>();
                Context.Identifier = Identifier;
                for (auto& Binding : Context.Bindings)
                {
                    Binding.Identifier = Guid::Generate().ToString();
                    if (Remapping.count(Binding.Action))
                    {
                        Binding.Action = Remapping.at(Binding.Action);
                    }
                }
                Document["Context"] = Context;
            }
            const auto Filename = Identifier + Extension;
            if (Registry.LeafNameExists(Filename))
            {
                throw std::runtime_error("Imported asset name conflicts with existing content");
            }
            const auto Path = Staging / Filename;
            if (!InputDocument::Write(Path, Document, Error))
            {
                throw std::runtime_error(Error);
            }
            AssetMetadata Metadata;
            Guid::TryParse(Identifier, Metadata.Guid);
            Metadata.Type = AssetType{Asset.at("Type").get<std::string>()};
            Metadata.ImportInfo = {{"InputPackage", Package.at("PackageId")}, {"Author", Package.at("Author")},
                {"License", Package.at("License")}, {"Origin", Asset.value("Origin", nlohmann::json::object())}};
            AssetDiagnostic Diagnostic;
            if (!AssetMetadataIO::TrySaveToFile(Path.string() + ".meta", Metadata, Diagnostic))
            {
                throw std::runtime_error(Diagnostic.Message);
            }
        }
        std::filesystem::create_directories(Destination.parent_path());
        std::filesystem::rename(Staging, Destination);
        bPublished = true;
        const auto Scanned = Registry.ScanContent();
        if (Scanned.HasError())
        {
            throw std::runtime_error(Scanned.Message);
        }
        std::vector<AssetRegistryEntry> Result;
        for (const auto& Mapping : Remapping)
        {
            Guid Identifier;
            Guid::TryParse(Mapping.second, Identifier);
            AssetRegistryEntry Entry;
            if (!Registry.TryGetById(Identifier, Entry))
            {
                throw std::runtime_error("Imported input asset was not registered");
            }
            Result.push_back(Entry);
        }
        Imported = std::move(Result);
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        std::error_code Ignored;
        std::filesystem::remove_all(Staging, Ignored);
        if (bPublished)
        {
            std::filesystem::remove_all(Destination, Ignored);
            Registry.ScanContent();
        }
        return false;
    }
}
