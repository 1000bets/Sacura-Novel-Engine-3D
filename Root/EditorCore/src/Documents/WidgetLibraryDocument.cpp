#include "Documents/WidgetLibraryDocument.h"
#include "Core/IO/AtomicFileWriter.h"
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <cmath>

using Json = nlohmann::json;

bool WidgetLibraryDocument::Validate(const Json& Document, std::string& Error)
{
    try
    {
        const auto Check = [](bool Condition, const char* Message)
        {
            if (!Condition)
            {
                throw std::runtime_error(Message);
            }
        };
        Check(Document.is_object() && Document.value("format", "") == "sakura.widgets"
            && Document.at("version") == 1, "Expected sakura.widgets version 1");
        Check(Document.dump(2).size() + 1 <= 524288, "Widget library exceeds 512 KiB");
        const auto& Widgets = Document.at("widgets");
        Check(Widgets.is_array() && !Widgets.empty() && Widgets.size() <= 128, "Expected 1 to 128 widgets");
        const std::set<std::string> Kinds{"dialogue", "hud", "mainMenu", "pauseMenu"};
        const std::set<std::string> Types{"panel", "text", "button", "image", "choices", "progress", "inventory"};
        std::map<std::string, std::string> WidgetKinds;
        for (const auto& Widget : Widgets)
        {
            const auto Identifier = Widget.at("id").get<std::string>();
            const auto Kind = Widget.at("kind").get<std::string>();
            Check(!Identifier.empty() && WidgetKinds.emplace(Identifier, Kind).second, "Widget IDs must be nonempty and unique");
            Check(Kinds.count(Kind) != 0 && Widget.at("name").is_string(), "Invalid widget kind or name");
            Check(Widget.at("referenceSize").is_array() && Widget.at("referenceSize").size() == 2, "Invalid reference resolution");
            for (const auto& Dimension : Widget.at("referenceSize"))
            {
                Check(Dimension.is_number() && Dimension >= 100 && Dimension <= 7680, "Reference resolution must be 100 to 7680");
            }
            Check(Widget.at("css").is_string(), "Widget CSS must be text");
            const auto& Elements = Widget.at("elements");
            Check(Elements.is_array() && Elements.size() <= 512, "Widget supports at most 512 elements");
            std::map<std::string, const Json*> Identifiers;
            for (const auto& Element : Elements)
            {
                const auto ElementIdentifier = Element.at("id").get<std::string>();
                Check(!ElementIdentifier.empty() && Identifiers.emplace(ElementIdentifier, &Element).second,
                    "Element IDs must be nonempty and unique inside a widget");
                Check(Types.count(Element.at("type").get<std::string>()) != 0, "Unknown widget element type");
                Check(Element.at("name").is_string() && Element.at("visible").is_boolean()
                    && Element.at("style").is_object() && Element.at("role").is_string(), "Invalid element properties");
                const auto& Layout = Element.at("layout");
                for (const auto* Property : {"x", "y", "width", "height"})
                {
                    Check(Layout.at(Property).is_number() && std::isfinite(Layout.at(Property).get<double>()), "Invalid layout number");
                }
                for (const auto* Property : {"anchorMin", "anchorMax", "pivot"})
                {
                    Check(Layout.at(Property).is_array() && Layout.at(Property).size() == 2, "Invalid layout vector");
                    for (const auto& Coordinate : Layout.at(Property))
                    {
                        Check(Coordinate.is_number() && Coordinate >= 0 && Coordinate <= 1, "Anchors and pivots must be between 0 and 1");
                    }
                }
                for (size_t Axis = 0; Axis < 2; ++Axis)
                {
                    Check(Layout.at("anchorMin")[Axis] <= Layout.at("anchorMax")[Axis], "Minimum anchor exceeds maximum anchor");
                }
            }
            for (const auto& Element : Elements)
            {
                std::set<std::string> Visited{Element.at("id").get<std::string>()};
                const Json* Current = &Element;
                while (!Current->at("parentId").is_null())
                {
                    const auto Parent = Current->at("parentId").get<std::string>();
                    Check(Identifiers.count(Parent) != 0 && Visited.insert(Parent).second, "Missing parent or cyclic widget hierarchy");
                    Current = Identifiers.at(Parent);
                    Check(Current->at("type") == "panel", "Widget parent must be a panel");
                }
            }
        }
        for (const auto& Kind : Kinds)
        {
            const auto Identifier = Document.at("ui").at(Kind).get<std::string>();
            Check(WidgetKinds.count(Identifier) != 0 && WidgetKinds.at(Identifier) == Kind, "Default widget assignment has missing or incompatible target");
        }
        return true;
    }
    catch (const std::exception& Failure)
    {
        Error = Failure.what();
        return false;
    }
}

bool WidgetLibraryDocument::Read(const std::filesystem::path& Path, Json& Document, std::string& Error)
{
    std::ifstream File(Path, std::ios::binary | std::ios::ate);
    if (!File || File.tellg() < 0 || File.tellg() > 524288)
    {
        Error = "Widget library is unavailable or exceeds 512 KiB";
        return false;
    }
    File.seekg(0);
    const auto Candidate = Json::parse(File, nullptr, false);
    if (Candidate.is_discarded() || !Validate(Candidate, Error))
    {
        if (Candidate.is_discarded())
        {
            Error = "Widget library contains invalid JSON";
        }
        return false;
    }
    Document = Candidate;
    return true;
}

bool WidgetLibraryDocument::Open(const AssetKey& Asset, std::string& Error)
{
    AssetRegistryEntry Resolved;
    if (Asset.HasSubAsset() || !Registry.TryResolveKey(Asset, Resolved) || Resolved.Metadata.Type != WidgetLibraryAssetType)
    {
        Error = "Widget library asset not found";
        return false;
    }
    Json Document;
    ContentHash Current;
    AssetDiagnostic Diagnostic;
    if (!Read(std::filesystem::u8path(Resolved.AbsolutePath), Document, Error)
        || !ContentHash::TryHashFile(Resolved.AbsolutePath, Current, Diagnostic))
    {
        return false;
    }
    Entry = std::move(Resolved);
    Saved = std::move(Document);
    History = {Saved};
    Position = 0;
    Fingerprint = Current;
    return true;
}

bool WidgetLibraryDocument::Edit(const Json& Document, std::string& Error)
{
    if (Entry.Mount != AssetMount::Game)
    {
        Error = "Engine widgets are read-only";
        return false;
    }
    if (!Validate(Document, Error))
    {
        return false;
    }
    if (Document != GetDocument())
    {
        History.resize(Position + 1);
        History.push_back(Document);
        ++Position;
    }
    return true;
}

void WidgetLibraryDocument::Undo()
{
    if (CanUndo())
    {
        --Position;
    }
}

void WidgetLibraryDocument::Redo()
{
    if (CanRedo())
    {
        ++Position;
    }
}

bool WidgetLibraryDocument::Save(std::string& Error)
{
    AssetRegistryEntry Current;
    ContentHash CurrentFingerprint;
    AssetDiagnostic Diagnostic;
    if (Entry.Mount != AssetMount::Game || !Registry.TryGetById(Entry.Metadata.Guid, Current))
    {
        Error = "Widget library is unavailable or read-only";
        return false;
    }
    if (!ContentHash::TryHashFile(Current.AbsolutePath, CurrentFingerprint, Diagnostic) || CurrentFingerprint != Fingerprint)
    {
        Error = "Widget library changed on disk; preserve your draft and reload before saving";
        return false;
    }
    const auto Bytes = GetDocument().dump(2) + "\n";
    if (Bytes.size() > 524288)
    {
        Error = "Widget library exceeds 512 KiB";
        return false;
    }
    if (!AtomicFileWriter::WriteText(std::filesystem::u8path(Current.AbsolutePath), Bytes, Error))
    {
        return false;
    }
    Saved = GetDocument();
    Fingerprint = ContentHash::FromBytes(Bytes.data(), Bytes.size());
    Entry = std::move(Current);
    const auto Updated = Registry.UpdateSourceFingerprint(Entry.Metadata.Guid, Fingerprint);
    Error = Updated.Message;
    return !Updated.HasError();
}
