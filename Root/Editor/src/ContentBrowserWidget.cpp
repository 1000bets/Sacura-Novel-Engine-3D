#include "ContentBrowserWidget.h"

#include "AssetImportDialog.h"
#include "AssetTools/AssetImporter.h"
#include "AssetTools/ImportRequest.h"
#include "Assets/AssetMetadata.h"
#include "EditorAssetMime.h"
#include "Engine.h"

#include <QAction>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QClipboard>
#include <QDrag>
#include <QEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QListWidget>
#include <QMimeData>
#include <QMessageBox>
#include <QMenu>
#include <QPainter>
#include <QSplitter>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <map>
#include <set>

constexpr int AssetIdRole = Qt::UserRole;
constexpr int SubAssetIdRole = Qt::UserRole + 1;
constexpr int AssetTypeRole = Qt::UserRole + 2;
constexpr int VirtualPathRole = Qt::UserRole + 3;
constexpr int FolderPathRole = Qt::UserRole + 4;
constexpr int ItemKindRole = Qt::UserRole + 5;
constexpr int FolderItemKind = 1;
constexpr int AssetItemKind = 2;

bool IsSupportedImportFile(const QString& Path)
{
    const QString Extension = QFileInfo(Path).suffix().toLower();
    return Extension == "glb" || Extension == "fbx"
        || Extension == "png" || Extension == "jpg" || Extension == "jpeg";
}

bool TryBuildAssetPayload(QListWidgetItem* Item, EditorAssetPayload& OutPayload)
{
    if (Item == nullptr || Item->data(AssetIdRole).toString().isEmpty())
    {
        return false;
    }
    if (!Guid::TryParse(Item->data(AssetIdRole).toString().toStdString(), OutPayload.Key.Asset))
    {
        return false;
    }
    const QString SubAssetText = Item->data(SubAssetIdRole).toString();
    if (!SubAssetText.isEmpty())
    {
        SubAssetId ParsedSubAsset{};
        if (!Guid::TryParse(SubAssetText.toStdString(), ParsedSubAsset))
        {
            return false;
        }
        OutPayload.Key.SubAsset = ParsedSubAsset;
    }
    if (!TryParseAssetType(Item->data(AssetTypeRole).toString().toStdString(), OutPayload.Type))
    {
        return false;
    }
    OutPayload.VirtualPath = Item->data(VirtualPathRole).toString();
    return true;
}

class AssetListWidget : public QListWidget
{
public:
    using QListWidget::QListWidget;

    std::function<void(const QStringList&)> ExternalFilesDropped;
    std::function<void(const EditorAssetPayload&, const QString&)> AssetMoveRequested;

protected:
    void startDrag(Qt::DropActions) override
    {
        QListWidgetItem* Item = currentItem();
        EditorAssetPayload Payload;
        if (!TryBuildAssetPayload(Item, Payload))
        {
            return;
        }
        auto* MimeData = new QMimeData();
        MimeData->setData(SakuraAssetMimeType, EncodeEditorAssetPayload(Payload));
        auto* Drag = new QDrag(this);
        Drag->setMimeData(MimeData);
        Drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);
    }

    void dragEnterEvent(QDragEnterEvent* Event) override
    {
        if (Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            Event->setDropAction(Qt::MoveAction);
            Event->accept();
            return;
        }
        if (Event->mimeData()->hasUrls())
        {
            for (const QUrl& Url : Event->mimeData()->urls())
            {
                if (Url.isLocalFile() && IsSupportedImportFile(Url.toLocalFile()))
                {
                    Event->acceptProposedAction();
                    return;
                }
            }
        }
        QListWidget::dragEnterEvent(Event);
    }

    void dragMoveEvent(QDragMoveEvent* Event) override
    {
        if (Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            Event->setDropAction(Qt::MoveAction);
            Event->accept();
            return;
        }
        if (Event->mimeData()->hasUrls())
        {
            Event->acceptProposedAction();
            return;
        }
        QListWidget::dragMoveEvent(Event);
    }

    void dropEvent(QDropEvent* Event) override
    {
        if (Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            EditorAssetPayload Payload;
            if (DecodeEditorAssetPayload(Event->mimeData(), Payload) && AssetMoveRequested)
            {
                QString DestinationFolder;
                QListWidgetItem* TargetItem = itemAt(Event->position().toPoint());
                if (TargetItem != nullptr && TargetItem->data(ItemKindRole).toInt() == FolderItemKind)
                {
                    DestinationFolder = TargetItem->data(FolderPathRole).toString();
                }
                AssetMoveRequested(Payload, DestinationFolder);
                Event->setDropAction(Qt::MoveAction);
                Event->accept();
            }
            return;
        }
        if (!Event->mimeData()->hasUrls())
        {
            QListWidget::dropEvent(Event);
            return;
        }

        QStringList SourceFiles;
        for (const QUrl& Url : Event->mimeData()->urls())
        {
            const QString LocalFile = Url.toLocalFile();
            if (!LocalFile.isEmpty() && IsSupportedImportFile(LocalFile))
            {
                SourceFiles.push_back(LocalFile);
            }
        }
        if (!SourceFiles.isEmpty() && ExternalFilesDropped)
        {
            ExternalFilesDropped(SourceFiles);
            Event->acceptProposedAction();
        }
    }
};

class FolderTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

    std::function<void(const EditorAssetPayload&, const QString&)> AssetMoveRequested;

protected:
    void dragEnterEvent(QDragEnterEvent* Event) override
    {
        if (Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            Event->setDropAction(Qt::MoveAction);
            Event->accept();
            return;
        }
        QTreeWidget::dragEnterEvent(Event);
    }

    void dragMoveEvent(QDragMoveEvent* Event) override
    {
        if (Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            Event->setDropAction(Qt::MoveAction);
            Event->accept();
            return;
        }
        QTreeWidget::dragMoveEvent(Event);
    }

    void dropEvent(QDropEvent* Event) override
    {
        if (!Event->mimeData()->hasFormat(SakuraAssetMimeType))
        {
            QTreeWidget::dropEvent(Event);
            return;
        }

        EditorAssetPayload Payload;
        QTreeWidgetItem* TargetItem = itemAt(Event->position().toPoint());
        if (TargetItem != nullptr
            && DecodeEditorAssetPayload(Event->mimeData(), Payload)
            && AssetMoveRequested)
        {
            AssetMoveRequested(Payload, TargetItem->data(0, FolderPathRole).toString());
            Event->setDropAction(Qt::MoveAction);
            Event->accept();
        }
    }
};

QIcon MakeAssetIcon(AssetType Type)
{
    QPixmap Image(72, 72);
    Image.fill(QColor(38, 39, 48));
    QPainter Painter(&Image);
    Painter.setRenderHint(QPainter::Antialiasing, true);
    const QColor Accent = Type == TextureAssetType ? QColor(92, 151, 188)
        : Type == MaterialAssetType ? QColor(182, 112, 145)
        : Type == ModelAssetType || Type == StaticMeshAssetType ? QColor(113, 164, 125)
        : QColor(145, 132, 171);
    Painter.setPen(QPen(Accent.lighter(130), 2.0));
    Painter.setBrush(Accent.darker(145));
    Painter.drawRoundedRect(QRectF(10, 10, 52, 52), 6, 6);
    Painter.setPen(QColor(235, 230, 238));
    Painter.drawText(Image.rect(), Qt::AlignCenter, QString::fromUtf8(AssetTypeToString(Type)).left(3).toUpper());
    return QIcon(Image);
}

QIcon MakeFolderIcon()
{
    QPixmap Image(72, 72);
    Image.fill(QColor(38, 39, 48));
    QPainter Painter(&Image);
    Painter.setRenderHint(QPainter::Antialiasing, true);
    Painter.setPen(QPen(QColor(204, 162, 104), 2.0));
    Painter.setBrush(QColor(117, 85, 52));
    Painter.drawRoundedRect(QRectF(8, 22, 56, 38), 5, 5);
    Painter.drawRoundedRect(QRectF(12, 14, 24, 14), 4, 4);
    return QIcon(Image);
}

ContentBrowserWidget::ContentBrowserWidget(QWidget* Parent)
    : QWidget(Parent)
{
    setObjectName("ContentBrowser");
    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);

    auto* Toolbar = new QWidget(this);
    Toolbar->setObjectName("ContentBrowserToolbar");
    auto* ToolbarLayout = new QHBoxLayout(Toolbar);
    ToolbarLayout->setContentsMargins(8, 6, 8, 6);
    ToolbarLayout->setSpacing(6);
    TypeFilterLayout = new QHBoxLayout();
    TypeFilterLayout->setContentsMargins(0, 0, 0, 0);
    TypeFilterLayout->setSpacing(4);
    TypeFilterGroup = new QButtonGroup(this);
    TypeFilterGroup->setExclusive(true);
    ToolbarLayout->addLayout(TypeFilterLayout, 1);
    ImportButton = new QToolButton(Toolbar);
    ImportButton->setObjectName("ContentBrowserImport");
    ImportButton->setText(tr("Import"));
    ImportButton->setToolTip(tr("Import assets into the current Game Content folder"));
    ToolbarLayout->addWidget(ImportButton);
    Search = new QLineEdit(this);
    Search->setPlaceholderText(tr("Search Content"));
    Search->setMaximumWidth(260);
    ToolbarLayout->addWidget(Search);
    Layout->addWidget(Toolbar);
    RebuildTypeFilters();

    auto* BrowserSplitter = new QSplitter(Qt::Horizontal, this);
    auto* FolderTree = new FolderTreeWidget(BrowserSplitter);
    SourcesTree = FolderTree;
    SourcesTree->setObjectName("ContentBrowserSources");
    SourcesTree->setHeaderLabel(tr("Sources"));
    SourcesTree->setMinimumWidth(180);
    auto* AssetList = new AssetListWidget(BrowserSplitter);
    AssetView = AssetList;
    AssetView->setObjectName("ContentBrowserAssets");
    AssetView->setViewMode(QListView::IconMode);
    AssetView->setResizeMode(QListView::Adjust);
    AssetView->setMovement(QListView::Static);
    AssetView->setIconSize(QSize(72, 72));
    AssetView->setGridSize(QSize(120, 112));
    AssetView->setWordWrap(true);
    AssetView->setDragEnabled(true);
    AssetView->setAcceptDrops(true);
    AssetView->viewport()->setAcceptDrops(true);
    AssetView->setDragDropMode(QAbstractItemView::DragDrop);
    AssetView->setDefaultDropAction(Qt::MoveAction);
    AssetView->setSelectionMode(QAbstractItemView::SingleSelection);
    AssetView->setContextMenuPolicy(Qt::CustomContextMenu);
    AssetList->ExternalFilesDropped = [this](const QStringList& SourceFiles)
    {
        ImportFiles(SourceFiles);
    };
    AssetList->AssetMoveRequested = [this](const EditorAssetPayload& Payload, const QString& DestinationFolder)
    {
        MoveAsset(
            Payload.VirtualPath,
            DestinationFolder.isEmpty() ? CurrentFolder : DestinationFolder);
    };
    SourcesTree->setAcceptDrops(true);
    SourcesTree->viewport()->setAcceptDrops(true);
    SourcesTree->setDragDropMode(QAbstractItemView::DropOnly);
    SourcesTree->setDefaultDropAction(Qt::MoveAction);
    FolderTree->AssetMoveRequested = [this](const EditorAssetPayload& Payload, const QString& DestinationFolder)
    {
        MoveAsset(Payload.VirtualPath, DestinationFolder);
    };
    SourcesTree->installEventFilter(this);
    AssetView->installEventFilter(this);
    BrowserSplitter->addWidget(SourcesTree);
    BrowserSplitter->addWidget(AssetView);
    BrowserSplitter->setStretchFactor(0, 0);
    BrowserSplitter->setStretchFactor(1, 1);
    BrowserSplitter->setSizes({210, 800});
    Layout->addWidget(BrowserSplitter, 1);

    connect(Search, &QLineEdit::textChanged, this, [this]()
    {
        RefreshAssetView();
    });
    connect(TypeFilterGroup, &QButtonGroup::idClicked, this, [this](int)
    {
        RefreshAssetView();
    });
    connect(SourcesTree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* Item)
    {
        if (Item != nullptr)
        {
            SetCurrentFolder(Item->data(0, FolderPathRole).toString());
        }
    });
    connect(AssetView, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* Item)
    {
        if (Item != nullptr && Item->data(ItemKindRole).toInt() == FolderItemKind)
        {
            SetCurrentFolder(Item->data(FolderPathRole).toString());
            return;
        }
        if (Item != nullptr && Item->data(ItemKindRole).toInt() == AssetItemKind)
        {
            emit AssetActivated(
                Item->data(AssetIdRole).toString(),
                Item->data(SubAssetIdRole).toString(),
                Item->data(AssetTypeRole).toString(),
                Item->data(VirtualPathRole).toString());
        }
    });
    connect(ImportButton, &QToolButton::clicked, this, &ContentBrowserWidget::SelectImportFiles);
    connect(AssetView, &QListWidget::customContextMenuRequested, this, &ContentBrowserWidget::ShowAssetContextMenu);

    auto* RenameAction = new QAction(tr("Rename Asset"), AssetView);
    RenameAction->setShortcut(QKeySequence(Qt::Key_F2));
    RenameAction->setShortcutContext(Qt::WidgetShortcut);
    AssetView->addAction(RenameAction);
    connect(RenameAction, &QAction::triggered, this, &ContentBrowserWidget::RenameSelectedAsset);

    auto* DeleteAction = new QAction(tr("Delete Asset"), AssetView);
    DeleteAction->setShortcut(QKeySequence::Delete);
    DeleteAction->setShortcutContext(Qt::WidgetShortcut);
    AssetView->addAction(DeleteAction);
    connect(DeleteAction, &QAction::triggered, this, &ContentBrowserWidget::DeleteSelectedAsset);

}

void ContentBrowserWidget::SetEngine(Engine* EngineInstance)
{
    BoundEngine = EngineInstance;
    Registry = BoundEngine != nullptr ? &BoundEngine->GetAssetRegistry() : nullptr;
    RebuildTypeFilters();
    Refresh();
}

void ContentBrowserWidget::Refresh()
{
    CachedEntries.clear();
    if (Registry == nullptr)
    {
        SourcesTree->clear();
        AssetView->clear();
        return;
    }
    CachedEntries = Registry->FindByDirectory("/Game");
    std::vector<AssetRegistryEntry> EngineEntries = Registry->FindByDirectory("/Engine");
    CachedEntries.insert(CachedEntries.end(), EngineEntries.begin(), EngineEntries.end());
    std::sort(CachedEntries.begin(), CachedEntries.end(), [](const AssetRegistryEntry& First, const AssetRegistryEntry& Second)
    {
        return First.VirtualPath < Second.VirtualPath;
    });
    BuildSourcesTree(CachedEntries);
    SetCurrentFolder(CurrentFolder);
}

void ContentBrowserWidget::BuildSourcesTree(const std::vector<AssetRegistryEntry>& Entries)
{
    SourcesTree->clear();
    std::map<QString, QTreeWidgetItem*> ItemsByPath;
    auto AddRoot = [&](const QString& Path, const QString& Name)
    {
        auto* Root = new QTreeWidgetItem(SourcesTree, {Name});
        Root->setData(0, FolderPathRole, Path);
        ItemsByPath.emplace(Path, Root);
    };
    AddRoot("/Game", tr("Game Content"));
    AddRoot("/Engine", tr("Engine Content"));

    std::set<QString> Folders;
    for (const AssetRegistryEntry& Entry : Entries)
    {
        QString Folder = QString::fromStdString(Entry.VirtualPath).section('/', 0, -2);
        while (Folder.count('/') > 1)
        {
            Folders.insert(Folder);
            Folder = Folder.section('/', 0, -2);
        }
    }
    for (const QString& Folder : Folders)
    {
        const QString ParentPath = Folder.section('/', 0, -2);
        auto Parent = ItemsByPath.find(ParentPath);
        if (Parent == ItemsByPath.end())
        {
            continue;
        }
        auto* Item = new QTreeWidgetItem(Parent->second, {Folder.section('/', -1)});
        Item->setData(0, FolderPathRole, Folder);
        ItemsByPath.emplace(Folder, Item);
    }
    SourcesTree->expandAll();
}

void ContentBrowserWidget::RebuildTypeFilters()
{
    if (TypeFilterLayout == nullptr || TypeFilterGroup == nullptr)
    {
        return;
    }

    const QString PreviousFilter = TypeFilterGroup->checkedButton() != nullptr
        ? TypeFilterGroup->checkedButton()->property("assetType").toString()
        : QString{};

    const QList<QAbstractButton*> ExistingButtons = TypeFilterGroup->buttons();
    for (QAbstractButton* Button : ExistingButtons)
    {
        TypeFilterGroup->removeButton(Button);
    }
    while (QLayoutItem* Item = TypeFilterLayout->takeAt(0))
    {
        delete Item->widget();
        delete Item;
    }

    auto AddFilterButton = [this](const QString& Label, const QString& TypeIdentifier)
    {
        auto* Button = new QToolButton(this);
        Button->setObjectName("ContentBrowserTypeFilter");
        Button->setText(Label);
        Button->setCheckable(true);
        Button->setAutoRaise(false);
        Button->setProperty("assetType", TypeIdentifier);
        Button->setToolTip(TypeIdentifier.isEmpty()
            ? tr("Show folders and assets in the current Content folder")
            : tr("Show every %1 asset under the current Content folder").arg(Label));
        TypeFilterGroup->addButton(Button);
        TypeFilterLayout->addWidget(Button);
        return Button;
    };

    QToolButton* AllButton = AddFilterButton(tr("All"), QString{});
    if (Registry != nullptr)
    {
        for (const AssetTypeRegistration& Registration : Registry->GetAssetTypeRegistrations())
        {
            if (Registration.bVisibleInContentBrowser)
            {
                AddFilterButton(
                    QString::fromStdString(Registration.DisplayName),
                    QString::fromStdString(Registration.Type.GetIdentifier()));
            }
        }
    }
    TypeFilterLayout->addStretch(1);

    QAbstractButton* RestoredButton = nullptr;
    for (QAbstractButton* Button : TypeFilterGroup->buttons())
    {
        if (Button->property("assetType").toString() == PreviousFilter)
        {
            RestoredButton = Button;
            break;
        }
    }
    if (RestoredButton != nullptr)
    {
        RestoredButton->setChecked(true);
    }
    else
    {
        AllButton->setChecked(true);
    }
}

AssetType ContentBrowserWidget::GetActiveTypeFilter() const
{
    AssetType SelectedType = UnknownAssetType;
    if (TypeFilterGroup == nullptr || TypeFilterGroup->checkedButton() == nullptr)
    {
        return SelectedType;
    }
    TryParseAssetType(
        TypeFilterGroup->checkedButton()->property("assetType").toString().toStdString(),
        SelectedType);
    return SelectedType;
}

void ContentBrowserWidget::RefreshAssetView()
{
    AssetView->clear();
    const QString SearchText = Search->text().trimmed();
    const AssetType SelectedType = GetActiveTypeFilter();
    const bool bTypeFilterActive = SelectedType.IsValid();
    std::set<QString> ChildFolders;
    for (const AssetRegistryEntry& Entry : CachedEntries)
    {
        const QString VirtualPath = QString::fromStdString(Entry.VirtualPath);
        const bool bInCurrentFolder = VirtualPath.startsWith(CurrentFolder + "/")
            || VirtualPath == CurrentFolder;
        if (!bInCurrentFolder)
        {
            continue;
        }
        const QString RelativePath = VirtualPath == CurrentFolder
            ? QString{}
            : VirtualPath.mid(CurrentFolder.size() + 1);
        if (!bTypeFilterActive
            && RelativePath.contains('/')
            && SearchText.isEmpty())
        {
            ChildFolders.insert(CurrentFolder + "/" + RelativePath.section('/', 0, 0));
            continue;
        }
        if (!bTypeFilterActive && RelativePath.contains('/'))
        {
            continue;
        }

        const QString Name = QFileInfo(VirtualPath).fileName();
        const bool bMatchesSearch = SearchText.isEmpty()
            || Name.contains(SearchText, Qt::CaseInsensitive)
            || QString::fromUtf8(AssetTypeToString(Entry.Metadata.Type)).contains(SearchText, Qt::CaseInsensitive);
        const bool bMatchesType = !bTypeFilterActive || Entry.Metadata.Type == SelectedType;
        if (Entry.Metadata.SubAssets.empty() && bMatchesSearch && bMatchesType)
        {
            AddAssetItem(Entry);
        }
        for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
        {
            const bool bSubAssetMatchesSearch = SearchText.isEmpty()
                || QString::fromStdString(SubAsset.Name).contains(SearchText, Qt::CaseInsensitive)
                || QString::fromUtf8(AssetTypeToString(SubAsset.Type)).contains(SearchText, Qt::CaseInsensitive);
            const bool bSubAssetMatchesType = !bTypeFilterActive || SubAsset.Type == SelectedType;
            if (bSubAssetMatchesSearch && bSubAssetMatchesType)
            {
                AddSubAssetItem(Entry, SubAsset);
            }
        }
    }
    if (!bTypeFilterActive && SearchText.isEmpty())
    {
        for (auto FolderIterator = ChildFolders.rbegin(); FolderIterator != ChildFolders.rend(); ++FolderIterator)
        {
            const QString& Folder = *FolderIterator;
            auto* Item = new QListWidgetItem(MakeFolderIcon(), Folder.section('/', -1));
            Item->setData(ItemKindRole, FolderItemKind);
            Item->setData(FolderPathRole, Folder);
            Item->setToolTip(Folder);
            AssetView->insertItem(0, Item);
        }
    }
}

void ContentBrowserWidget::SetCurrentFolder(const QString& Folder)
{
    if (!Folder.startsWith("/Game") && !Folder.startsWith("/Engine"))
    {
        CurrentFolder = "/Game";
    }
    else
    {
        CurrentFolder = Folder;
    }
    ImportButton->setEnabled(CurrentFolder == "/Game" || CurrentFolder.startsWith("/Game/"));
    RefreshAssetView();
}

void ContentBrowserWidget::SelectImportFiles()
{
    const QStringList SourceFiles = QFileDialog::getOpenFileNames(
        this,
        tr("Import Assets"),
        QString{},
        tr("Supported Assets (*.glb *.fbx *.png *.jpg *.jpeg);;GLB Models (*.glb);;FBX Models (*.fbx);;Images (*.png *.jpg *.jpeg)"));
    if (!SourceFiles.isEmpty())
    {
        ImportFiles(SourceFiles);
    }
}

void ContentBrowserWidget::ImportFiles(const QStringList& SourceFiles)
{
    if (Registry == nullptr)
    {
        QMessageBox::warning(this, tr("Import Failed"), tr("No project asset registry is available."));
        return;
    }
    if (CurrentFolder != "/Game" && !CurrentFolder.startsWith("/Game/"))
    {
        QMessageBox::warning(this, tr("Import Failed"), tr("Engine Content is read-only. Select a Game Content folder."));
        return;
    }

    QStringList SupportedFiles;
    for (const QString& SourceFile : SourceFiles)
    {
        if (QFileInfo(SourceFile).isFile() && IsSupportedImportFile(SourceFile))
        {
            SupportedFiles.push_back(SourceFile);
        }
    }
    if (SupportedFiles.isEmpty())
    {
        QMessageBox::warning(this, tr("Import Failed"), tr("No supported asset files were selected."));
        return;
    }

    const QString InitialFolder = CurrentFolder == "/Game"
        ? QString{}
        : CurrentFolder.mid(QString("/Game/").size());
    AssetImportDialog Dialog(SupportedFiles, InitialFolder, this);
    if (Dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    AssetImporter Importer(*Registry);
    QStringList Failures;
    int ImportedCount = 0;
    for (int SourceIndex = 0; SourceIndex < SupportedFiles.size(); ++SourceIndex)
    {
        const QString DestinationFile = Dialog.GetDestinationFileName(SourceIndex);
        const QString DestinationPath = Dialog.GetDestinationFolder().isEmpty()
            ? DestinationFile
            : Dialog.GetDestinationFolder() + "/" + DestinationFile;

        ImportRequest Request{};
#if defined(_WIN32)
        Request.SourcePath = std::filesystem::path(SupportedFiles[SourceIndex].toStdWString());
#else
        Request.SourcePath = std::filesystem::path(SupportedFiles[SourceIndex].toUtf8().constData());
#endif
        Request.DestinationRelativePath = DestinationPath.toUtf8().toStdString();
        Request.bGenerateMissingNormals = Dialog.GetGenerateMissingNormals();

        const ImportResult Result = Importer.Import(Request);
        if (Result.Succeeded())
        {
            ++ImportedCount;
        }
        else
        {
            Failures.push_back(
                QFileInfo(SupportedFiles[SourceIndex]).fileName()
                + ": "
                + QString::fromStdString(Result.Diagnostic.Message));
        }
    }

    Refresh();
    if (Failures.isEmpty())
    {
        QMessageBox::information(
            this,
            tr("Import Complete"),
            tr("Imported %1 asset(s).").arg(ImportedCount));
    }
    else
    {
        QMessageBox::warning(
            this,
            tr("Import Completed With Errors"),
            tr("Imported %1 asset(s).\n\n%2").arg(ImportedCount).arg(Failures.join('\n')));
    }
}

bool ContentBrowserWidget::eventFilter(QObject* Watched, QEvent* Event)
{
    if (Event != nullptr
        && (Event->type() == QEvent::ShortcutOverride || Event->type() == QEvent::KeyPress)
        && (Watched == SourcesTree || Watched == AssetView))
    {
        auto* KeyEvent = static_cast<QKeyEvent*>(Event);
        if (KeyEvent->matches(QKeySequence::Copy) || KeyEvent->matches(QKeySequence::Paste))
        {
            if (Event->type() == QEvent::ShortcutOverride)
            {
                KeyEvent->accept();
                return true;
            }
            if (KeyEvent->matches(QKeySequence::Copy))
            {
                CopySelectionToClipboard();
            }
            else
            {
                PasteFromClipboard();
            }
            KeyEvent->accept();
            return true;
        }
    }
    return QWidget::eventFilter(Watched, Event);
}

bool ContentBrowserWidget::CopySelectionToClipboard()
{
    EditorAssetPayload Payload;
    if (!TryBuildAssetPayload(AssetView != nullptr ? AssetView->currentItem() : nullptr, Payload))
    {
        return false;
    }

    auto* MimeData = new QMimeData();
    MimeData->setData(SakuraAssetMimeType, EncodeEditorAssetPayload(Payload));
    MimeData->setText(Payload.VirtualPath);
    QGuiApplication::clipboard()->setMimeData(MimeData);
    return true;
}

bool ContentBrowserWidget::PasteFromClipboard()
{
    return DuplicateClipboardAsset();
}

bool ContentBrowserWidget::DuplicateClipboardAsset()
{
    if (Registry == nullptr
        || (CurrentFolder != "/Game" && !CurrentFolder.startsWith("/Game/")))
    {
        return false;
    }

    EditorAssetPayload Payload;
    if (!DecodeEditorAssetPayload(QGuiApplication::clipboard()->mimeData(), Payload))
    {
        return false;
    }

    AssetRegistryEntry SourceEntry{};
    if (!Registry->TryResolveKey(Payload.Key, SourceEntry))
    {
        QMessageBox::warning(this, tr("Paste Failed"), tr("The copied asset is no longer registered."));
        return false;
    }

    AssetMetadata DuplicatedMetadata{};
    AssetDiagnostic MetadataDiagnostic{};
    if (!AssetMetadataIO::TryLoadFromFile(
            SourceEntry.AbsoluteMetaPath,
            DuplicatedMetadata,
            MetadataDiagnostic))
    {
        QMessageBox::warning(this, tr("Paste Failed"), QString::fromStdString(MetadataDiagnostic.Message));
        return false;
    }

    const QFileInfo SourceInformation(QString::fromStdString(SourceEntry.AbsolutePath));
    const QString Extension = SourceInformation.suffix();
    const QString BaseName = SourceInformation.completeBaseName() + QStringLiteral("_Copy");
    const std::filesystem::path DestinationDirectory =
        Registry->GetGameContentRoot()
        / (CurrentFolder == "/Game"
            ? std::filesystem::path{}
            : std::filesystem::path(CurrentFolder.mid(QStringLiteral("/Game/").size()).toStdString()));

    std::error_code FileError;
    std::filesystem::create_directories(DestinationDirectory, FileError);
    if (FileError)
    {
        QMessageBox::warning(this, tr("Paste Failed"), QString::fromStdString(FileError.message()));
        return false;
    }

    QString DestinationBaseName = BaseName;
    int NameSuffix = 2;
    auto MakeDestinationPath = [&]()
    {
        return DestinationDirectory / (
            DestinationBaseName
            + (Extension.isEmpty() ? QString{} : QStringLiteral(".") + Extension)).toStdString();
    };
    while (std::filesystem::exists(MakeDestinationPath())
        || (DuplicatedMetadata.SubAssets.empty()
            && Registry->LeafNameExists(DestinationBaseName.toStdString())))
    {
        DestinationBaseName = BaseName + QStringLiteral("_") + QString::number(NameSuffix);
        ++NameSuffix;
    }

    std::set<QString> ReservedNames;
    for (SubAssetRecord& SubAsset : DuplicatedMetadata.SubAssets)
    {
        const QString SubAssetBaseName = QString::fromStdString(SubAsset.Name) + QStringLiteral("_Copy");
        QString SubAssetName = SubAssetBaseName;
        int SubAssetSuffix = 2;
        while (Registry->LeafNameExists(SubAssetName.toStdString())
            || ReservedNames.find(SubAssetName.toLower()) != ReservedNames.end())
        {
            SubAssetName = SubAssetBaseName + QStringLiteral("_") + QString::number(SubAssetSuffix);
            ++SubAssetSuffix;
        }
        ReservedNames.insert(SubAssetName.toLower());
        SubAsset.Id = Guid::Generate();
        SubAsset.Name = SubAssetName.toStdString();
    }
    DuplicatedMetadata.Guid = Guid::Generate();

    const std::filesystem::path DestinationPath = MakeDestinationPath();
    std::filesystem::copy_file(
        SourceEntry.AbsolutePath,
        DestinationPath,
        std::filesystem::copy_options::none,
        FileError);
    if (FileError)
    {
        QMessageBox::warning(this, tr("Paste Failed"), QString::fromStdString(FileError.message()));
        return false;
    }

    const std::filesystem::path DestinationMetaPath = DestinationPath.string() + ".meta";
    if (!AssetMetadataIO::TrySaveToFile(
            DestinationMetaPath.generic_string(),
            DuplicatedMetadata,
            MetadataDiagnostic))
    {
        std::filesystem::remove(DestinationPath, FileError);
        QMessageBox::warning(this, tr("Paste Failed"), QString::fromStdString(MetadataDiagnostic.Message));
        return false;
    }

    const AssetDiagnostic ScanResult = Registry->ScanContent();
    if (ScanResult.HasError())
    {
        QMessageBox::warning(this, tr("Paste Failed"), QString::fromStdString(ScanResult.Message));
        Refresh();
        return false;
    }

    Refresh();
    return true;
}

void ContentBrowserWidget::ShowAssetContextMenu(const QPoint& Position)
{
    QListWidgetItem* Item = AssetView->itemAt(Position);
    QMenu Menu(this);
    if (Item != nullptr && Item->data(ItemKindRole).toInt() == AssetItemKind)
    {
        AssetView->setCurrentItem(Item);
        Menu.addAction(tr("Copy"), this, &ContentBrowserWidget::CopySelectionToClipboard);
        if (Item->data(VirtualPathRole).toString().startsWith("/Game/"))
        {
            Menu.addAction(tr("Rename"), this, &ContentBrowserWidget::RenameSelectedAsset);
            Menu.addAction(tr("Delete"), this, &ContentBrowserWidget::DeleteSelectedAsset);
        }
        Menu.addSeparator();
    }
    QAction* PasteAction = Menu.addAction(tr("Paste"), this, &ContentBrowserWidget::PasteFromClipboard);
    PasteAction->setEnabled(
        (CurrentFolder == "/Game" || CurrentFolder.startsWith("/Game/"))
        && QGuiApplication::clipboard()->mimeData()->hasFormat(SakuraAssetMimeType));
    Menu.exec(AssetView->viewport()->mapToGlobal(Position));
}

void ContentBrowserWidget::RenameSelectedAsset()
{
    QListWidgetItem* Item = AssetView->currentItem();
    if (Registry == nullptr
        || Item == nullptr
        || Item->data(ItemKindRole).toInt() != AssetItemKind)
    {
        return;
    }

    const QString SourceVirtualPath = Item->data(VirtualPathRole).toString();
    if (!SourceVirtualPath.startsWith("/Game/"))
    {
        QMessageBox::warning(this, tr("Rename Failed"), tr("Engine Content is read-only."));
        return;
    }

    const QString SubAssetText = Item->data(SubAssetIdRole).toString();
    const QFileInfo SourceInformation(SourceVirtualPath);
    bool bAccepted = false;
    const QString NewBaseName = QInputDialog::getText(
        this,
        tr("Rename Asset"),
        tr("Asset name"),
        QLineEdit::Normal,
        SubAssetText.isEmpty() ? SourceInformation.completeBaseName() : Item->text(),
        &bAccepted).trimmed();
    if (!bAccepted)
    {
        return;
    }
    if (NewBaseName.isEmpty() || NewBaseName == "." || NewBaseName == ".."
        || NewBaseName.contains('/') || NewBaseName.contains('\\'))
    {
        QMessageBox::warning(this, tr("Rename Failed"), tr("Asset name must be a single non-empty name."));
        return;
    }

    if (!SubAssetText.isEmpty())
    {
        AssetKey Key{};
        if (!Guid::TryParse(Item->data(AssetIdRole).toString().toStdString(), Key.Asset))
        {
            return;
        }
        SubAssetId ParsedSubAsset{};
        if (!Guid::TryParse(SubAssetText.toStdString(), ParsedSubAsset))
        {
            return;
        }
        Key.SubAsset = ParsedSubAsset;
        const AssetDiagnostic Result = Registry->RenameSubAsset(Key, NewBaseName.toUtf8().toStdString());
        if (Result.HasError())
        {
            QMessageBox::warning(this, tr("Rename Failed"), QString::fromStdString(Result.Message));
            return;
        }
        Refresh();
        return;
    }

    const QString Extension = SourceInformation.suffix();
    const QString DestinationVirtualPath = SourceVirtualPath.section('/', 0, -2)
        + "/"
        + NewBaseName
        + (Extension.isEmpty() ? QString{} : "." + Extension);
    if (DestinationVirtualPath == SourceVirtualPath)
    {
        return;
    }

    const AssetDiagnostic Result = Registry->RenamePair(
        SourceVirtualPath.toUtf8().toStdString(),
        DestinationVirtualPath.toUtf8().toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Rename Failed"), QString::fromStdString(Result.Message));
        return;
    }
    Refresh();
}

void ContentBrowserWidget::DeleteSelectedAsset()
{
    QListWidgetItem* Item = AssetView->currentItem();
    if (Registry == nullptr
        || BoundEngine == nullptr
        || Item == nullptr
        || Item->data(ItemKindRole).toInt() != AssetItemKind)
    {
        return;
    }

    const QString VirtualPath = Item->data(VirtualPathRole).toString();
    if (!VirtualPath.startsWith("/Game/"))
    {
        QMessageBox::warning(this, tr("Delete Failed"), tr("Engine Content is read-only."));
        return;
    }
    if (QMessageBox::question(
            this,
            tr("Delete Asset"),
            tr("Delete %1?\nThe backing file is removed when its last asset is deleted. Existing scene references will become missing.")
                .arg(Item->text()),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel)
        != QMessageBox::Yes)
    {
        return;
    }

    AssetRegistryEntry Entry{};
    if (!Registry->TryGetByPath(VirtualPath.toUtf8().toStdString(), Entry))
    {
        QMessageBox::warning(this, tr("Delete Failed"), tr("Asset is no longer registered."));
        Refresh();
        return;
    }

    AssetKey Key{};
    if (!Guid::TryParse(Item->data(AssetIdRole).toString().toStdString(), Key.Asset))
    {
        return;
    }
    const QString SubAssetText = Item->data(SubAssetIdRole).toString();
    if (!SubAssetText.isEmpty())
    {
        SubAssetId ParsedSubAsset{};
        if (!Guid::TryParse(SubAssetText.toStdString(), ParsedSubAsset))
        {
            return;
        }
        Key.SubAsset = ParsedSubAsset;
    }

    const AssetDiagnostic Result = Key.HasSubAsset()
        ? Registry->DeleteSubAsset(Key)
        : Registry->DeletePair(VirtualPath.toUtf8().toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Delete Failed"), QString::fromStdString(Result.Message));
        return;
    }

    BoundEngine->GetAssetManager().InvalidateAsset(Entry.Metadata.Guid);
    BoundEngine->GetAssetGpuUploader().InvalidateAsset(Entry.Metadata.Guid);
    Refresh();
}

void ContentBrowserWidget::MoveAsset(
    const QString& SourceVirtualPath,
    const QString& DestinationFolder)
{
    if (Registry == nullptr)
    {
        return;
    }
    if (!SourceVirtualPath.startsWith("/Game/") || (DestinationFolder != "/Game" && !DestinationFolder.startsWith("/Game/")))
    {
        QMessageBox::warning(this, tr("Move Failed"), tr("Assets can only be moved within Game Content."));
        return;
    }

    const QString DestinationVirtualPath = DestinationFolder + "/" + QFileInfo(SourceVirtualPath).fileName();
    if (DestinationVirtualPath == SourceVirtualPath)
    {
        return;
    }
    const AssetDiagnostic Result = Registry->RenamePair(
        SourceVirtualPath.toUtf8().toStdString(),
        DestinationVirtualPath.toUtf8().toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Move Failed"), QString::fromStdString(Result.Message));
        return;
    }
    Refresh();
}

void ContentBrowserWidget::AddAssetItem(const AssetRegistryEntry& Entry)
{
    const QString VirtualPath = QString::fromStdString(Entry.VirtualPath);
    auto* Item = new QListWidgetItem(
        MakeAssetIcon(Entry.Metadata.Type),
        QFileInfo(VirtualPath).completeBaseName(),
        AssetView);
    Item->setData(ItemKindRole, AssetItemKind);
    Item->setData(AssetIdRole, QString::fromStdString(Entry.Metadata.Guid.ToString()));
    Item->setData(AssetTypeRole, QString::fromStdString(Entry.Metadata.Type.GetIdentifier()));
    Item->setData(VirtualPathRole, VirtualPath);
    Item->setToolTip(VirtualPath + "\n" + QString::fromUtf8(AssetTypeToString(Entry.Metadata.Type)));
    Item->setFlags(Item->flags() | Qt::ItemIsDragEnabled);
}

void ContentBrowserWidget::AddSubAssetItem(const AssetRegistryEntry& Entry, const SubAssetRecord& SubAsset)
{
    auto* Item = new QListWidgetItem(
        MakeAssetIcon(SubAsset.Type),
        QString::fromStdString(SubAsset.Name),
        AssetView);
    Item->setData(ItemKindRole, AssetItemKind);
    Item->setData(AssetIdRole, QString::fromStdString(Entry.Metadata.Guid.ToString()));
    Item->setData(SubAssetIdRole, QString::fromStdString(SubAsset.Id.ToString()));
    Item->setData(AssetTypeRole, QString::fromStdString(SubAsset.Type.GetIdentifier()));
    Item->setData(VirtualPathRole, QString::fromStdString(Entry.VirtualPath));
    Item->setToolTip(QString::fromStdString(Entry.VirtualPath) + "\n" + QString::fromUtf8(AssetTypeToString(SubAsset.Type)));
    Item->setFlags(Item->flags() | Qt::ItemIsDragEnabled);
}
