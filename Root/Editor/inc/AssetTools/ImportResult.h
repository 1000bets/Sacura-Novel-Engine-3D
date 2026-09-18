#pragma once

#include "Assets/AssetMetadata.h"
#include "Assets/AssetTypes.h"

#include <string>

struct ImportResult
{
    AssetDiagnostic Diagnostic = AssetDiagnostic::Ok();
    AssetMetadata Metadata{};
    std::string PublishedRelativePath;
    std::filesystem::path PublishedAbsolutePath;

    bool Succeeded() const { return !Diagnostic.HasError(); }
};
