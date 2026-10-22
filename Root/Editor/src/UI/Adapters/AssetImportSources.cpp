#include "UI/Adapters/AssetImportSources.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QObject>
#include <QSet>

namespace EditorUi
{
bool IsSupportedAssetImport(const QString& Path)
{
    const QString Extension = QFileInfo(Path).suffix().toLower();
    return Extension == "glb" || Extension == "fbx"
        || Extension == "png" || Extension == "jpg" || Extension == "jpeg";
}

AssetImportSources CollectAssetImportSources(const QStringList& Paths)
{
    AssetImportSources Result;
    QSet<QString> Seen;
    auto AddFile = [&](const QFileInfo& File, const QString& RelativeFolder)
    {
        if (File.isSymLink() || !IsSupportedAssetImport(File.filePath()))
        {
            ++Result.SkippedFiles;
            return;
        }
        if (!File.isFile() || !File.isReadable())
        {
            Result.Errors.append(QObject::tr("Не удалось прочитать файл: %1").arg(File.filePath()));
            return;
        }
        if (File.size() > 100 * 1024 * 1024)
        {
            Result.Errors.append(QObject::tr("Файл больше 100 МБ: %1").arg(File.fileName()));
            return;
        }
        const QString Identity = File.canonicalFilePath();
        if (Seen.contains(Identity))
        {
            return;
        }
        Seen.insert(Identity);
        Result.Files.append(File.absoluteFilePath());
        Result.RelativeFolders.append(RelativeFolder);
    };
    for (const QString& Path : Paths)
    {
        const QFileInfo Source(Path);
        if (Source.isSymLink())
        {
            Result.Errors.append(QObject::tr("Выберите исходную папку или файл вместо ссылки: %1").arg(Path));
            continue;
        }
        if (Source.isDir())
        {
            if (!Source.isReadable())
            {
                Result.Errors.append(QObject::tr("Не удалось прочитать папку: %1").arg(Path));
                continue;
            }
            const QDir Parent = Source.dir();
            QDirIterator Files(Source.absoluteFilePath(), QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
            while (Files.hasNext())
            {
                Files.next();
                const QFileInfo File = Files.fileInfo();
                AddFile(File, Parent.relativeFilePath(File.absolutePath()));
            }
        }
        else
        {
            AddFile(Source, {});
        }
    }
    return Result;
}
}
