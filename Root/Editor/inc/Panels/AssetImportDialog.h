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
        QWidget* Parent = nullptr);

    QString GetDestinationFolder() const;
    QString GetDestinationFileName(int Index) const;
    bool GetGenerateMissingNormals() const;

protected:
    void accept() override;

private:
    QStringList Sources;
    QLineEdit* DestinationFolderEdit = nullptr;
    QTableWidget* FilesTable = nullptr;
    QCheckBox* GenerateMissingNormalsCheck = nullptr;
};
