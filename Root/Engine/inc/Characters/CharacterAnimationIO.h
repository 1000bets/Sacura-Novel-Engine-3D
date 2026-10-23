#pragma once
#include "Assets/Resources/AnimationClipResource.h"
#include <filesystem>
#include <string>

std::shared_ptr<const AnimationClipResource> ReadCharacterAnimation(const std::filesystem::path& Path, std::string& Error);
