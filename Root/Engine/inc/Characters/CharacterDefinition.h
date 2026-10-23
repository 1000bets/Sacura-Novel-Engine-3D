#pragma once

#include "Assets/Loaders/IAssetLoader.h"
#include <nlohmann/json.hpp>
#include <SimpleMath.h>
#include <filesystem>

struct CharacterAnimation
{
    std::string Identifier;
    std::string Name;
    std::string Description;
    AssetKey Clip;
    std::string Pose = "idle";
    float Speed = 1.f;
    bool bEnabled = true;
    bool bLoop = true;
    bool bInPlace = true;
};

struct CharacterDefinition
{
    std::string Name;
    std::string Description;
    AssetKey Model;
    AssetKey Skeleton;
    std::string DialogueWidget;
    std::string IdleAnimation = "idle";
    std::string WalkAnimation = "walk";
    DirectX::SimpleMath::Color Color{0.73f, 0.6f, 0.67f, 1.f};
    bool bEnabled = true;
    std::vector<CharacterAnimation> Animations;

    const CharacterAnimation* FindAnimation(const std::string& Identifier) const;
    static CharacterDefinition Create(const std::string& Name);
    static bool Parse(const nlohmann::json& Data, CharacterDefinition& Result, std::string& Error);
    static bool Read(const std::filesystem::path& Path, CharacterDefinition& Result, std::string& Error);
    nlohmann::json ToJson() const;
};

class CharacterLoader : public IAssetLoader
{
public:
    AssetType GetAssetType() const override { return CharacterAssetType; }
    const char* GetLoaderName() const override { return "CharacterLoader"; }
    std::shared_ptr<const void> Load(const AssetLoadContext& Context, AssetDiagnostic& Diagnostic) override;
};
