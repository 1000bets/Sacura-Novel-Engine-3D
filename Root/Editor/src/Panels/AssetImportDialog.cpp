#include "UI/Views/StudioControls.h"
#include "Panels/AssetImportDialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

AssetImportDialog::AssetImportDialog(
    const QStringList& SourceFiles,
    const QString& InitialDestinationFolder,
    QWidget* Parent,
    const QStringList& RelativeFolders,
    int SkippedFiles)
    : QDialog(Parent)
    , Sources(SourceFiles)
    , SourceFolders(RelativeFolders)
{
    setWindowTitle(tr("Импорт ресурсов"));
    setModal(true);
    resize(680, 420);

    auto* Layout = new QVBoxLayout(this);
    auto* Description = new QLabel(
        tr("Проверьте имена и папки перед импортом в Assets. Существующие файлы не перезаписываются."),
        this);
    Description->setWordWrap(true);
    Layout->addWidget(Description);

    auto* DestinationLayout = new QFormLayout();
    DestinationFolderEdit = new QLineEdit(InitialDestinationFolder, this);
    DestinationFolderEdit->setPlaceholderText(tr("Models"));
    DestinationLayout->addRow(tr("Папка назначения"), DestinationFolderEdit);
    Layout->addLayout(DestinationLayout);

    FilesTable = new QTableWidget(SourceFiles.size(), 3, this);
    FilesTable->setObjectName("AssetImportFiles");
    FilesTable->setHorizontalHeaderLabels({tr("Источник"), tr("Имя файла"), tr("Вложенная папка")});
    FilesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    FilesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    FilesTable->verticalHeader()->hide();
    FilesTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    bool bHasFbxSource = false;
    for (int SourceIndex = 0; SourceIndex < SourceFiles.size(); ++SourceIndex)
    {
        const QFileInfo SourceInformation(SourceFiles[SourceIndex]);
        auto* SourceItem = new QTableWidgetItem(SourceInformation.filePath());
        SourceItem->setFlags(SourceItem->flags() & ~Qt::ItemIsEditable);
        FilesTable->setItem(SourceIndex, 0, SourceItem);

        QString DestinationName = SourceInformation.fileName();
        if (SourceInformation.suffix().compare("fbx", Qt::CaseInsensitive) == 0)
        {
            DestinationName = SourceInformation.completeBaseName() + ".glb";
            bHasFbxSource = true;
        }
        FilesTable->setItem(SourceIndex, 1, new QTableWidgetItem(DestinationName));
        auto* FolderItem = new QTableWidgetItem(SourceFolders.value(SourceIndex));
        FolderItem->setFlags(FolderItem->flags() & ~Qt::ItemIsEditable);
        FilesTable->setItem(SourceIndex, 2, FolderItem);
    }
    Layout->addWidget(FilesTable, 1);

    GenerateMissingNormalsCheck = new EditorUi::StudioCheckBox(tr("Создать недостающие нормали моделей FBX"), this);
    GenerateMissingNormalsCheck->setChecked(true);
    GenerateMissingNormalsCheck->setVisible(bHasFbxSource);
    Layout->addWidget(GenerateMissingNormalsCheck);

    auto* FormatInformation = new QLabel(
        tr("GLB без внешних файлов, FBX (риг импортируется в статической позе), PNG и JPEG · до 100 МБ на файл. FBX преобразуется в GLB. Пропущено неподдерживаемых файлов: %1.").arg(SkippedFiles),
        this);
    FormatInformation->setWordWrap(true);
    Layout->addWidget(FormatInformation);

    auto* Buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        this);
    Buttons->button(QDialogButtonBox::Ok)->setText(tr("Импортировать"));
    Buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    connect(Buttons, &QDialogButtonBox::accepted, this, &AssetImportDialog::accept);
    connect(Buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    Layout->addWidget(Buttons);
}

QString AssetImportDialog::GetDestinationFolder() const
{
    QString Folder = DestinationFolderEdit->text().trimmed();
    Folder.replace('\\', '/');
    while (Folder.startsWith('/'))
    {
        Folder.remove(0, 1);
    }
    while (Folder.endsWith('/'))
    {
        Folder.chop(1);
    }
    return Folder;
}

QString AssetImportDialog::GetDestinationFileName(int Index) const
{
    QTableWidgetItem* Item = FilesTable->item(Index, 1);
    return Item != nullptr ? Item->text().trimmed() : QString{};
}

QString AssetImportDialog::GetDestinationRelativePath(int Index) const
{
    QStringList Parts;
    if (!GetDestinationFolder().isEmpty())
    {
        Parts.append(GetDestinationFolder());
    }
    if (!SourceFolders.value(Index).isEmpty())
    {
        Parts.append(SourceFolders[Index]);
    }
    Parts.append(GetDestinationFileName(Index));
    return Parts.join('/');
}

bool AssetImportDialog::GetGenerateMissingNormals() const
{
    return GenerateMissingNormalsCheck->isChecked();
}

void AssetImportDialog::accept()
{
    const QString Folder = GetDestinationFolder();
    if (Folder.split('/').contains("..") || Folder.contains(':'))
    {
        QMessageBox::warning(this, tr("Invalid Destination"), tr("Destination must stay inside Game Content."));
        return;
    }

    for (int SourceIndex = 0; SourceIndex < Sources.size(); ++SourceIndex)
    {
        const QString FileName = GetDestinationFileName(SourceIndex);
        if (FileName.isEmpty() || FileName == "." || FileName == ".."
            || FileName.contains('/') || FileName.contains('\\'))
        {
            QMessageBox::warning(
                this,
                tr("Invalid Asset Name"),
                tr("Every asset name must be a single file name."));
            return;
        }

        const QString Suffix = QFileInfo(FileName).suffix().toLower();
        if (Suffix != "glb" && Suffix != "png" && Suffix != "jpg" && Suffix != "jpeg")
        {
            QMessageBox::warning(
                this,
                tr("Unsupported Destination"),
                tr("Destination names must use GLB, PNG, JPG or JPEG extensions."));
            return;
        }
    }

    QDialog::accept();
}
