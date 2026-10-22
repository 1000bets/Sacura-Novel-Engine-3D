#pragma once
#include <nlohmann/json_fwd.hpp>
namespace StoryDocumentSchema
{
// Apply the difference between two canonical snapshots to the raw document.
// Unknown fields remain attached to their stable authored identities.
void Merge(nlohmann::json& Original, const nlohmann::json& Before, const nlohmann::json& After);
} // namespace StoryDocumentSchema
