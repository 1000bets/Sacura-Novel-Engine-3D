#pragma once

#include <QDialog>
#include <QStringList>

class QCheckBox;
class QLineEdit;
class QTableWidget;

class AssetImportDialog : public QDialog
{
    Q_OBJECT

public:
    AssetImportDialog(
        const QStringList& SourceFiles,
        const QString& InitialDestinationFolder,
        QWidget* Parent = nullptr,
        const QStringList& RelativeFolders = {},
        int SkippedFiles = 0);

    QString GetDestinationFolder() const;
    QString GetDestinationFileName(int Index) const;
    QString GetDestinationRelativePath(int Index) const;
    bool GetGenerateMissingNormals() const;

protected:
    void accept() override;

private:
    QStringList Sources;
    QStringList SourceFolders;
    QLineEdit* DestinationFolderEdit = nullptr;
    QTableWidget* FilesTable = nullptr;
    QCheckBox* GenerateMissingNormalsCheck = nullptr;
};
