#pragma once
#include <map>
#include <string>
#include <vector>

struct StoryEventPackageMetadata
{
    std::string Name, Description, Author, License;
};
struct StoryEventPackageInput
{
    std::string Id, Label;
};
struct StoryEventPackagePreview
{
    StoryEventPackageMetadata Metadata;
    std::vector<StoryEventPackageInput> Inputs;
    size_t Groups = 0, Actions = 0;
};

// A deliberately declared finite native profile of the local package envelope.
// Preview and apply validate independently; neither evaluates scripts or writes assets.
bool PreviewStoryEventPackage(const std::string& Text, StoryEventPackagePreview& Preview, std::string& Error);
