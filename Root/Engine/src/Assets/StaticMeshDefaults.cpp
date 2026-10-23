#include "Assets/StaticMeshDefaults.h"
#include <cmath>
#include <limits>
#include <stdexcept>

nlohmann::json StaticMeshCollisionElement::ToJson() const
{
    return {{"shape", ShapeType}, {"center", {Center.x, Center.y, Center.z}},
        {"rotation", {RotationDegrees.x, RotationDegrees.y, RotationDegrees.z}},
        {"halfExtents", {HalfExtents.x, HalfExtents.y, HalfExtents.z}},
        {"radius", Radius}, {"capsuleHalfHeight", CapsuleHalfHeight}};
}

nlohmann::json StaticMeshDefaults::ToJson() const
{
    nlohmann::json Slots = nlohmann::json::array();
    for (const auto& Entry : Materials)
    {
        Slots.push_back({{"slot", Entry.first}, {"material", AssetMetadataIO::AssetRefToJson(Entry.second)}});
    }
    nlohmann::json Elements = nlohmann::json::array();
    for (const auto& Element : CollisionElements)
    {
        Elements.push_back(Element.ToJson());
    }
    return {{"materials", Slots}, {"collision", {{"enabled", bCollisionEnabled}, {"elements", Elements}}}};
}

bool StaticMeshDefaults::Parse(const nlohmann::json& Value, StaticMeshDefaults& Result, std::string& Error)
{
    try
    {
        if (!Value.is_object())
        {
            throw std::runtime_error("Mesh defaults must be an object");
        }
        StaticMeshDefaults Parsed;
        const auto Slots = Value.value("materials", nlohmann::json::array());
        if (!Slots.is_array())
        {
            throw std::runtime_error("Materials must be an array");
        }
        for (const auto& Entry : Slots)
        {
            if (!Entry.at("slot").is_number_integer())
            {
                throw std::runtime_error("Material slot must be an integer");
            }
            const auto Slot = Entry.at("slot").get<int64_t>();
            AssetKey Material;
            AssetDiagnostic Diagnostic;
            if (Slot < 0 || Slot > std::numeric_limits<int32_t>::max()
                || !AssetMetadataIO::TryAssetRefFromJson(Entry.at("material"), Material, Diagnostic)
                || !Material.IsValid() || !Parsed.Materials.emplace(static_cast<int32_t>(Slot), Material).second)
            {
                throw std::runtime_error("Invalid or duplicate material slot");
            }
        }
        const auto Collision = Value.value("collision", nlohmann::json::object());
        if (!Collision.is_object())
        {
            throw std::runtime_error("Collision must be an object");
        }
        Parsed.bCollisionEnabled = Collision.value("enabled", false);
        auto Elements = nlohmann::json::array({Collision});
        if (Collision.contains("elements"))
        {
            Elements = Collision.at("elements");
        }
        if (!Elements.is_array() || (Parsed.bCollisionEnabled && Elements.empty()))
        {
            throw std::runtime_error("Enabled collision requires at least one element");
        }
        Parsed.CollisionElements.clear();
        for (const auto& Element : Elements)
        {
            if (!Element.is_object())
            {
                throw std::runtime_error("Collision element must be an object");
            }
            StaticMeshCollisionElement Shape;
            if (Element.contains("shape") && !Element.at("shape").is_number_integer())
            {
                throw std::runtime_error("Collision shape must be an integer");
            }
            Shape.ShapeType = Element.value("shape", int64_t{0});
            auto ReadVector = [&](const char* Name, DirectX::SimpleMath::Vector3& Vector)
            {
                if (Element.contains(Name))
                {
                    const auto& Array = Element.at(Name);
                    if (!Array.is_array() || Array.size() != 3)
                    {
                        throw std::runtime_error("Collision vector requires three numbers");
                    }
                    Vector = {Array.at(0).get<float>(), Array.at(1).get<float>(), Array.at(2).get<float>()};
                }
                if (!std::isfinite(Vector.x) || !std::isfinite(Vector.y) || !std::isfinite(Vector.z))
                {
                    throw std::runtime_error("Collision vector must be finite");
                }
            };
            ReadVector("center", Shape.Center);
            ReadVector("rotation", Shape.RotationDegrees);
            ReadVector("halfExtents", Shape.HalfExtents);
            Shape.Radius = Element.value("radius", 0.5f);
            Shape.CapsuleHalfHeight = Element.value("capsuleHalfHeight", 0.5f);
            if (Shape.ShapeType < 0 || Shape.ShapeType > 3 || Shape.HalfExtents.x <= 0
                || Shape.HalfExtents.y <= 0 || Shape.HalfExtents.z <= 0 || !std::isfinite(Shape.Radius)
                || Shape.Radius <= 0 || !std::isfinite(Shape.CapsuleHalfHeight) || Shape.CapsuleHalfHeight <= 0)
            {
                throw std::runtime_error("Collision dimensions must be positive; shape must be 0..3");
            }
            Parsed.CollisionElements.push_back(Shape);
        }
        Result = std::move(Parsed);
        Error.clear();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

nlohmann::json StaticMeshDefaults::Read(const AssetMetadata& Metadata, const AssetKey& Key)
{
    if (!Key.HasSubAsset() || !Metadata.LoadSettings.is_object())
    {
        return nlohmann::json::object();
    }
    const auto Found = Metadata.LoadSettings.find("staticMeshDefaults");
    if (Found == Metadata.LoadSettings.end() || !Found->is_object())
    {
        return nlohmann::json::object();
    }
    return Found->value(Key.SubAsset->ToString(), nlohmann::json::object());
}
