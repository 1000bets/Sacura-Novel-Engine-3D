#include "Characters/CharacterDefinition.h"
#include <fstream>
#include <set>
#include <cmath>

static nlohmann::json CharacterAssetJson(const AssetKey& Key)
{
    nlohmann::json Data = nlohmann::json::object();
    if (Key.IsValid())
    {
        Data["asset"] = Key.Asset.ToString();
        if (Key.HasSubAsset())
        {
            Data["subAsset"] = Key.SubAsset->ToString();
        }
    }
    return Data;
}

static AssetKey ReadCharacterAsset(const nlohmann::json& Data)
{
    if (!Data.is_object())
    {
        throw std::runtime_error("Asset must be an object");
    }
    AssetKey Key;
    if (Data.empty())
    {
        return Key;
    }
    if (!Guid::TryParse(Data.at("asset").get<std::string>(), Key.Asset) || !Key.Asset.IsValid())
    {
        throw std::runtime_error("Invalid character asset identity");
    }
    if (Data.contains("subAsset"))
    {
        Guid SubAsset;
        if (!Guid::TryParse(Data.at("subAsset").get<std::string>(), SubAsset) || !SubAsset.IsValid())
        {
            throw std::runtime_error("Invalid character subasset identity");
        }
        Key.SubAsset = SubAsset;
    }
    return Key;
}

const CharacterAnimation* CharacterDefinition::FindAnimation(const std::string& Identifier) const
{
    for (const auto& Animation : Animations)
    {
        if (Animation.Identifier == Identifier && Animation.bEnabled)
        {
            return &Animation;
        }
    }
    return nullptr;
}

CharacterDefinition CharacterDefinition::Create(const std::string& Name)
{
    CharacterDefinition Result;
    Result.Name = Name;
    const std::pair<const char*, const char*> Poses[] = {
        {"idle", "Ожидание"}, {"walk", "Ходьба"}, {"sit", "Сидит"}, {"wave", "Приветствие"},
        {"smile", "Радость"}, {"think", "Задумчивость"}, {"dance", "Танец"}, {"sad", "Грусть"}};
    for (const auto& Pose : Poses)
    {
        CharacterAnimation Animation;
        Animation.Identifier = Pose.first;
        Animation.Name = Pose.second;
        Animation.Pose = Pose.first;
        Result.Animations.push_back(std::move(Animation));
    }
    return Result;
}

nlohmann::json CharacterDefinition::ToJson() const
{
    nlohmann::json Data{{"format", "sakura.character"}, {"version", 1}, {"name", Name},
        {"description", Description}, {"enabled", bEnabled}, {"model", CharacterAssetJson(Model)},
        {"skeleton", CharacterAssetJson(Skeleton)}, {"dialogueWidget", DialogueWidget},
        {"idleAnimation", IdleAnimation}, {"walkAnimation", WalkAnimation},
        {"color", {Color.x, Color.y, Color.z, Color.w}}, {"animations", nlohmann::json::array()}};
    for (const auto& Animation : Animations)
    {
        Data["animations"].push_back({{"id", Animation.Identifier}, {"name", Animation.Name},
            {"description", Animation.Description}, {"clip", CharacterAssetJson(Animation.Clip)},
            {"pose", Animation.Pose}, {"speed", Animation.Speed}, {"enabled", Animation.bEnabled},
            {"loop", Animation.bLoop}, {"inPlace", Animation.bInPlace}});
    }
    return Data;
}

bool CharacterDefinition::Parse(const nlohmann::json& Data, CharacterDefinition& Result, std::string& Error)
{
    try
    {
        if (Data.at("format") != "sakura.character" || Data.at("version") != 1)
        {
            throw std::runtime_error("Unsupported character document format");
        }
        CharacterDefinition Candidate;
        Candidate.Name = Data.at("name").get<std::string>();
        if (Candidate.Name.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            throw std::runtime_error("Character name is required");
        }
        Candidate.Description = Data.value("description", "");
        Candidate.bEnabled = Data.value("enabled", true);
        Candidate.Model = ReadCharacterAsset(Data.value("model", nlohmann::json::object()));
        Candidate.Skeleton = ReadCharacterAsset(Data.value("skeleton", nlohmann::json::object()));
        Candidate.DialogueWidget = Data.value("dialogueWidget", "");
        Candidate.IdleAnimation = Data.value("idleAnimation", "");
        Candidate.WalkAnimation = Data.value("walkAnimation", "");
        if (Data.contains("color"))
        {
            const auto Channels = Data.at("color").get<std::vector<float>>();
            if (Channels.size() != 4)
            {
                throw std::runtime_error("Character color requires four channels");
            }
            for (float Channel : Channels)
            {
                if (!std::isfinite(Channel) || Channel < 0.f || Channel > 1.f)
                {
                    throw std::runtime_error("Character color is outside [0, 1]");
                }
            }
            Candidate.Color = {Channels[0], Channels[1], Channels[2], Channels[3]};
        }
        std::set<std::string> Identifiers;
        const std::set<std::string> Poses{"idle", "walk", "sit", "wave", "smile", "think", "dance", "sad"};
        for (const auto& Value : Data.at("animations"))
        {
            CharacterAnimation Animation;
            Animation.Identifier = Value.at("id").get<std::string>();
            Animation.Name = Value.at("name").get<std::string>();
            Animation.Description = Value.value("description", "");
            Animation.Clip = ReadCharacterAsset(Value.value("clip", nlohmann::json::object()));
            Animation.Pose = Value.value("pose", "idle");
            Animation.Speed = Value.value("speed", 1.f);
            Animation.bEnabled = Value.value("enabled", true);
            Animation.bLoop = Value.value("loop", true);
            Animation.bInPlace = Value.value("inPlace", true);
            if (Animation.Identifier.empty() || Animation.Name.empty() || !Identifiers.insert(Animation.Identifier).second
                || !std::isfinite(Animation.Speed) || Animation.Speed < 0.1f || Animation.Speed > 4.f
                || Poses.count(Animation.Pose) == 0)
            {
                throw std::runtime_error("Invalid animation name, identity, pose or speed");
            }
            Candidate.Animations.push_back(std::move(Animation));
        }
        Result = std::move(Candidate);
        Error.clear();
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool CharacterDefinition::Read(const std::filesystem::path& Path, CharacterDefinition& Result, std::string& Error)
{
    std::error_code FileError;
    const auto Size = std::filesystem::file_size(Path, FileError);
    if (FileError || Size > 4 * 1024 * 1024)
    {
        Error = "Character file is missing or exceeds 4 MB";
        return false;
    }
    std::ifstream Stream(Path);
    const auto Data = nlohmann::json::parse(Stream, nullptr, false);
    return Parse(Data, Result, Error);
}

std::shared_ptr<const void> CharacterLoader::Load(const AssetLoadContext& Context, AssetDiagnostic& Diagnostic)
{
    auto Result = std::make_shared<CharacterDefinition>();
    std::string Error;
    if (!CharacterDefinition::Read(Context.Entry.AbsolutePath, *Result, Error))
    {
        Diagnostic = AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "CharacterLoad", Error, Context.Key);
        return nullptr;
    }
    return Result;
}
