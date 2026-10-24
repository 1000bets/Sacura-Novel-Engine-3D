#include "UI/WidgetDefinition.h"
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <cmath>
using Json = nlohmann::json;

bool WidgetDefinition::Validate(const Json& Document, std::string& Error)
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
        Check(Document.is_object() && Document.value("format", "") == "sakura.widget"
            && Document.at("version") == 1, "Expected sakura.widget version 1");
        Check(Document.dump(2).size() + 1 <= 524288, "Widget exceeds 512 KiB");
        const auto& Widget = Document.at("widget");
        const std::set<std::string> Kinds{"dialogue", "hud", "mainMenu", "pauseMenu"};
        const std::set<std::string> Types{"panel", "text", "button", "image", "choices", "progress", "inventory"};
        const auto Identifier = Widget.at("id").get<std::string>();
        const auto Kind = Widget.at("kind").get<std::string>();
        Check(!Identifier.empty(), "Widget ID must be nonempty");
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
        return true;
    }
    catch (const std::exception& Failure)
    {
        Error = Failure.what();
        return false;
    }
}

bool WidgetDefinition::Read(const std::filesystem::path& Path, Json& Document, std::string& Error)
{
    std::ifstream File(Path, std::ios::binary | std::ios::ate);
    if (!File || File.tellg() < 0 || File.tellg() > 524288)
    {
        Error = "Widget is unavailable or exceeds 512 KiB";
        return false;
    }
    File.seekg(0);
    const auto Candidate = Json::parse(File, nullptr, false);
    if (Candidate.is_discarded() || !Validate(Candidate, Error))
    {
        if (Candidate.is_discarded())
        {
            Error = "Widget contains invalid JSON";
        }
        return false;
    }
    Document = Candidate;
    return true;
}

