#pragma once

#include <QStringList>

namespace EditorUi
{
struct AssetImportSources
{
    QStringList Files;
    QStringList RelativeFolders;
    QStringList Errors;
    int SkippedFiles = 0;
};

bool IsSupportedAssetImport(const QString& Path);
AssetImportSources CollectAssetImportSources(const QStringList& Paths);
}
