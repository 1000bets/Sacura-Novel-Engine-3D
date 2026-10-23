#pragma once
#include "Characters/CharacterDefinition.h"

bool ImportCharacterFbxAnimations(AssetRegistry& Registry, const std::filesystem::path& Source,
    const CharacterDefinition& Character, std::vector<CharacterAnimation>& Animations, std::string& Error);
