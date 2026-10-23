#include "Characters/CharacterAnimationIO.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/base/memory/unique_ptr.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>

std::shared_ptr<const AnimationClipResource> ReadCharacterAnimation(const std::filesystem::path& Path, std::string& Error)
{
    try
    {
        std::error_code FileError;
        if (std::filesystem::file_size(Path, FileError) > 128 * 1024 * 1024 || FileError)
        {
            throw std::runtime_error("Animation file is missing or exceeds 128 MB");
        }
        std::ifstream Stream(Path);
        const auto Data = nlohmann::json::parse(Stream);
        if (Data.at("format") != "sakura.animation" || Data.at("version") != 1)
        {
            throw std::runtime_error("Unsupported character animation format");
        }
        auto Result = std::make_shared<AnimationClipResource>();
        Result->Name = Data.at("name").get<std::string>();
        ozz::animation::offline::RawAnimation Raw;
        Raw.duration = Data.at("duration").get<float>();
        Raw.name = Result->Name.c_str();
        if (!std::isfinite(Raw.duration) || Raw.duration <= 0.f || Raw.duration > 600.f)
        {
            throw std::runtime_error("Invalid animation duration");
        }
        const auto& Tracks = Data.at("tracks");
        if (!Tracks.is_array() || Tracks.empty() || Tracks.size() > 1024)
        {
            throw std::runtime_error("Invalid animation track count");
        }
        Raw.tracks.resize(Tracks.size());
        for (size_t Index = 0; Index < Tracks.size(); ++Index)
        {
            const auto& Track = Tracks[Index];
            Result->JointNames.push_back(Track.at("joint").get<std::string>());
            const auto ReadKeys = [&](const char* Field, size_t Count, auto Add)
            {
                const auto& Keys = Track.at(Field);
                if (!Keys.is_array() || Keys.empty() || Keys.size() > 18001)
                {
                    throw std::runtime_error("Invalid animation key count");
                }
                for (const auto& Key : Keys)
                {
                    const auto Values = Key.get<std::vector<float>>();
                    if (Values.size() != Count)
                    {
                        throw std::runtime_error("Invalid animation key size");
                    }
                    for (float Value : Values)
                    {
                        if (!std::isfinite(Value))
                        {
                            throw std::runtime_error("Animation keys must be finite");
                        }
                    }
                    Add(Values);
                }
            };
            ReadKeys("translation", 4, [&](const auto& Values)
            { Raw.tracks[Index].translations.push_back({Values[0], {Values[1], Values[2], Values[3]}}); });
            ReadKeys("rotation", 5, [&](const auto& Values)
            { Raw.tracks[Index].rotations.push_back({Values[0], {Values[1], Values[2], Values[3], Values[4]}}); });
            ReadKeys("scale", 4, [&](const auto& Values)
            { Raw.tracks[Index].scales.push_back({Values[0], {Values[1], Values[2], Values[3]}}); });
        }
        if (!Raw.Validate())
        {
            throw std::runtime_error("Invalid character animation key times");
        }
        ozz::animation::offline::AnimationBuilder Builder;
        auto Built = Builder(Raw);
        if (!Built)
        {
            throw std::runtime_error("Animation compilation failed");
        }
        Result->Animation = std::shared_ptr<const ozz::animation::Animation>(Built.release(), ozz::Deleter<ozz::animation::Animation>());
        return Result;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return nullptr;
    }
}
