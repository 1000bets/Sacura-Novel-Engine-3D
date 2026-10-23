#include "UI/Adapters/AssetThumbnails.h"
#include "Panels/ContentBrowserWidget.h"
#include "UI/Styles/StudioIcons.h"
#include "UI/Adapters/AssetImportSources.h"
#include "UI/Styles/AssetExplorerStyle.h"
#include "UI/Views/AssetExplorerView.h"

#include "Panels/AssetImportDialog.h"
#include "Assets/MaterialAssetCreation.h"
#include "Assets/AssetImporter.h"
#include "Assets/ImportRequest.h"
#include "Assets/AssetMetadata.h"
#include "Assets/AssetPath.h"
#include "Core/Threading/ThreadContext.h"
#include "Panels/EditorAssetMime.h"
#include "Engine.h"

#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QSignalBlocker>
#include <QTreeWidgetItemIterator>
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
    bool bWritable = false;

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
            if (!bWritable)
            {
                Event->ignore();
                return;
            }
            for (const QUrl& Url : Event->mimeData()->urls())
            {
                if (Url.isLocalFile() && (QFileInfo(Url.toLocalFile()).isDir() || EditorUi::IsSupportedAssetImport(Url.toLocalFile())))
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
            if (!bWritable)
            {
                Event->ignore();
                return;
            }
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
        if (!bWritable)
        {
            Event->ignore();
            return;
        }
        for (const QUrl& Url : Event->mimeData()->urls())
        {
            const QString LocalFile = Url.toLocalFile();
            if (!LocalFile.isEmpty() && (QFileInfo(LocalFile).isDir() || EditorUi::IsSupportedAssetImport(LocalFile)))
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
    const char* Name = Type == TextureAssetType ? "Image"
        : Type == MaterialAssetType || Type == MaterialInstanceAssetType ? "Palette"
        : Type == ModelAssetType || Type == StaticMeshAssetType ? "Box"
        : Type == SkeletalMeshAssetType || Type == SkeletonAssetType || Type == SkinBindingAssetType ? "PersonStanding"
        : Type == AnimationClipAssetType ? "Clapperboard"
        : Type == SceneAssetType ? "Layers"
        : Type == StoryAssetType ? "MessageSquare"
        : Type == ShaderSourceAssetType ? "Code2" : "File";
    return EditorUi::MakeFrontendIcon(QString::fromLatin1(Name), "--accent");
}

QIcon MakeFolderIcon()
{
    return EditorUi::MakeFrontendIcon(QStringLiteral("Folder"), "--accent");
}

ContentBrowserWidget::ContentBrowserWidget(QWidget* Parent)
    : QWidget(Parent)
{
    setObjectName("ContentBrowser");
    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);

    Explorer = new EditorUi::AssetExplorerView(this);
    Layout->addWidget(Explorer);
    Search = Explorer->Search;
    ImportButton = Explorer->ImportFiles;
    TypeFilterMenu = new QMenu(this);
    TypeFilterMenu->setObjectName("ContentBrowserTypeFilters");
    FilterAction = Search->addAction(EditorUi::MakeFrontendIcon("ListFilter"), QLineEdit::TrailingPosition);
    FilterAction->setToolTip(tr("Фильтр по типу ресурса"));
    connect(FilterAction, &QAction::triggered, this, [this]()
    {
        TypeFilterMenu->popup(Search->mapToGlobal(QPoint(0, Search->height())));
    });
    RebuildTypeFilters();
    connect(Explorer, &EditorUi::AssetExplorerView::FolderRequested, this, &ContentBrowserWidget::SetCurrentFolder);
    connect(Explorer->NewFolder, &QToolButton::clicked, this, &ContentBrowserWidget::CreateFolder);
    connect(Explorer->ImportFolder, &QToolButton::clicked, this, &ContentBrowserWidget::SelectImportFolder);
    auto* BrowserSplitter = Explorer->Body;
    auto* FolderTree = new FolderTreeWidget(BrowserSplitter);
    SourcesTree = FolderTree;
    SourcesTree->setObjectName("ContentBrowserSources");
    SourcesTree->setHeaderHidden(true);
    SourcesTree->setAccessibleName(tr("Папки проекта"));
    SourcesTree->setMinimumWidth(130);
    SourcesTree->setIndentation(14);
    SourcesTree->setIconSize(QSize(16, 16));
    SourcesTree->setUniformRowHeights(true);
    auto* AssetList = new AssetListWidget(BrowserSplitter);
    AssetView = AssetList;
    AssetView->setObjectName("ContentBrowserAssets");
    AssetView->setViewMode(QListView::IconMode);
    AssetView->setResizeMode(QListView::Adjust);
    AssetView->setMovement(QListView::Static);
    AssetView->setIconSize(QSize(36, 36));
    AssetView->setGridSize(QSize(132, 122));
    AssetView->setItemDelegate(new EditorUi::AssetExplorerTileDelegate(AssetView));
    AssetView->setMouseTracking(true);
    AssetView->setAccessibleName(tr("Файлы и папки"));
    Explorer->EmptyState->setParent(AssetView->viewport());
    auto* EmptyLayout = new QVBoxLayout(AssetView->viewport());
    EmptyLayout->setContentsMargins(16, 16, 16, 16);
    EmptyLayout->addWidget(Explorer->EmptyState);
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
    SourcesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    SourcesTree->installEventFilter(this);
    AssetView->installEventFilter(this);
    BrowserSplitter->addWidget(SourcesTree);
    BrowserSplitter->addWidget(AssetView);
    BrowserSplitter->setStretchFactor(0, 0);
    BrowserSplitter->setStretchFactor(1, 1);
    BrowserSplitter->setSizes({190, 800});

    connect(Search, &QLineEdit::textChanged, this, [this]()
    {
        RefreshAssetView();
    });
    connect(SourcesTree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* Item)
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
    connect(SourcesTree, &QTreeWidget::customContextMenuRequested, this, &ContentBrowserWidget::ShowSourcesContextMenu);

    auto* RenameAction = new QAction(tr("Переименовать"), AssetView);
    RenameAction->setShortcut(QKeySequence(Qt::Key_F2));
    RenameAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    AssetView->addAction(RenameAction);
    SourcesTree->addAction(RenameAction);
    connect(RenameAction, &QAction::triggered, this, &ContentBrowserWidget::RenameSelection);

    auto* DeleteAction = new QAction(tr("Удалить"), AssetView);
    DeleteAction->setShortcut(QKeySequence::Delete);
    DeleteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    AssetView->addAction(DeleteAction);
    SourcesTree->addAction(DeleteAction);
    connect(DeleteAction, &QAction::triggered, this, &ContentBrowserWidget::DeleteSelection);

    auto* NewFolderAction = new QAction(tr("Новая папка"), this);
    NewFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    NewFolderAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    addAction(NewFolderAction);
    connect(NewFolderAction, &QAction::triggered, this, &ContentBrowserWidget::CreateFolder);

}

void ContentBrowserWidget::SetEngine(Engine* EngineInstance)
{
    BoundEngine = EngineInstance;
    Registry = BoundEngine != nullptr ? &BoundEngine->GetAssetRegistry() : nullptr;
    RebuildTypeFilters();
    Operations.reset();
    if (EngineInstance != nullptr)
    {
        Operations = std::make_unique<AssetOperations>(*EngineInstance);
    }
    Refresh();
    emit AssetsChanged();
}

void ContentBrowserWidget::SetProjectContext(ProjectSession* Session, SceneDocument* Document)
{
    if (Operations != nullptr)
    {
        Operations->SetProjectContext(Session, Document);
    }
}

void ContentBrowserWidget::Refresh()
{
    CachedEntries.clear();
    CachedFolders.clear();
    if (Registry == nullptr)
    {
        SourcesTree->clear();
        AssetView->clear();
        static_cast<AssetListWidget*>(AssetView)->bWritable = false;
        Explorer->SetFolder("/Game", false);
        Explorer->SetEmptyState(true, false);
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
    const QSignalBlocker Block(SourcesTree);
    std::set<QString> Expanded;
    const bool FirstBuild = SourcesTree->topLevelItemCount() == 0;
    for (QTreeWidgetItemIterator It(SourcesTree); *It; ++It)
    {
        if ((*It)->isExpanded())
        {
            Expanded.insert((*It)->data(0, FolderPathRole).toString());
        }
    }
    SourcesTree->clear();
    std::map<QString, QTreeWidgetItem*> ItemsByPath;
    auto AddRoot = [&](const QString& Path, const QString& Name)
    {
        auto* Root = new QTreeWidgetItem(SourcesTree, {Name});
        Root->setData(0, FolderPathRole, Path);
        Root->setIcon(0, EditorUi::MakeFrontendIcon("FolderOpen", "--text-secondary"));
        Root->setToolTip(0, Path);
        ItemsByPath.emplace(Path, Root);
    };
    AddRoot("/Game", "Assets");
    AddRoot("/Engine", tr("Ресурсы движка"));

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
    CollectGameFoldersOnDisk(Folders);
    CachedFolders = Folders;
    for (const QString& Folder : Folders)
    {
        const QString ParentPath = Folder.section('/', 0, -2);
        auto Parent = ItemsByPath.find(ParentPath);
        if (Parent == ItemsByPath.end())
        {
            continue;
        }
        if (ItemsByPath.find(Folder) != ItemsByPath.end())
        {
            continue;
        }
        auto* Item = new QTreeWidgetItem(Parent->second, {Folder.section('/', -1)});
        Item->setData(0, FolderPathRole, Folder);
        Item->setIcon(0, EditorUi::MakeFrontendIcon("Folder", "--text-secondary"));
        Item->setToolTip(0, Folder);
        ItemsByPath.emplace(Folder, Item);
    }
    for (const auto& [Path, Item] : ItemsByPath)
    {
        Item->setExpanded(Expanded.count(Path) > 0 || (FirstBuild && Path == "/Game"));
    }
    if (ItemsByPath.find(CurrentFolder) == ItemsByPath.end())
    {
        CurrentFolder = "/Game";
    }
}

void ContentBrowserWidget::RebuildTypeFilters()
{
    TypeFilterMenu->clear();
    if (auto* Previous = TypeFilterMenu->findChild<QActionGroup*>())
    {
        delete Previous;
    }
    auto* Group = new QActionGroup(TypeFilterMenu);
    Group->setExclusive(true);
    auto AddFilter = [&](const QString& Label, const QString& Identifier)
    {
        auto* Action = TypeFilterMenu->addAction(Label);
        Action->setData(Identifier);
        Action->setCheckable(true);
        Action->setChecked(Identifier == ActiveTypeFilter);
        Group->addAction(Action);
    };
    AddFilter(tr("Все типы"), {});
    if (Registry != nullptr)
    {
        for (const auto& Registration : Registry->GetAssetTypeRegistrations())
        {
            if (Registration.bVisibleInContentBrowser)
            {
                AddFilter(QString::fromStdString(Registration.DisplayName),
                          QString::fromStdString(Registration.Type.GetIdentifier()));
            }
        }
    }
    if (Group->checkedAction() == nullptr)
    {
        ActiveTypeFilter.clear();
        Group->actions().front()->setChecked(true);
    }
    connect(Group, &QActionGroup::triggered, this, [this](QAction* Action)
    {
        ActiveTypeFilter = Action->data().toString();
        FilterAction->setIcon(EditorUi::MakeFrontendIcon("ListFilter", ActiveTypeFilter.isEmpty() ? "--text-muted" : "--accent"));
        FilterAction->setToolTip(tr("Тип ресурса: %1").arg(Action->text()));
        RefreshAssetView();
    });
}

AssetType ContentBrowserWidget::GetActiveTypeFilter() const
{
    AssetType SelectedType = UnknownAssetType;
    TryParseAssetType(ActiveTypeFilter.toStdString(), SelectedType);
    return SelectedType;
}

void ContentBrowserWidget::RefreshAssetView()
{
    AssetView->clear();
    const QString SearchText = Search->text().trimmed();
    const AssetType SelectedType = GetActiveTypeFilter();
    const bool bTypeFilterActive = SelectedType.IsValid();
    std::set<QString> ChildFolders = CachedFolders;
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
    if (!bTypeFilterActive)
    {
        std::set<QString> DirectChildFolders;
        for (const QString& Folder : ChildFolders)
        {
            if (Folder.section('/', 0, -2) == CurrentFolder
                && (SearchText.isEmpty() || Folder.section('/', -1).contains(SearchText, Qt::CaseInsensitive)))
            {
                DirectChildFolders.insert(Folder);
            }
        }
        for (auto FolderIterator = DirectChildFolders.rbegin(); FolderIterator != DirectChildFolders.rend(); ++FolderIterator)
        {
            const QString& Folder = *FolderIterator;
            auto* Item = new QListWidgetItem(MakeFolderIcon(), Folder.section('/', -1));
            Item->setData(ItemKindRole, FolderItemKind);
            Item->setData(EditorUi::ExplorerSubtitleRole, tr("Папка"));
            Item->setData(FolderPathRole, Folder);
            Item->setToolTip(Folder);
            AssetView->insertItem(0, Item);
        }
    }
    Explorer->SetEmptyState(AssetView->count() == 0, !SearchText.isEmpty() || bTypeFilterActive);
}

bool ContentBrowserWidget::FocusAsset(const AssetKey& Key)
{
    AssetRegistryEntry Entry;
    if (Registry == nullptr || !Registry->TryResolveKey(Key, Entry))
    {
        return false;
    }
    {
        const QSignalBlocker Block(Search);
        Search->clear();
    }
    ActiveTypeFilter.clear();
    RebuildTypeFilters();
    FilterAction->setIcon(EditorUi::MakeFrontendIcon("ListFilter", "--text-muted"));
    FilterAction->setToolTip(tr("Тип ресурса: %1").arg(tr("Все типы")));
    Refresh();
    SetCurrentFolder(QString::fromStdString(Entry.VirtualPath).section('/', 0, -2));
    const QString AssetIdentifier = QString::fromStdString(Key.Asset.ToString());
    QString SubAssetIdentifier;
    if (Key.HasSubAsset())
    {
        SubAssetIdentifier = QString::fromStdString(Key.SubAsset->ToString());
    }
    for (int Index = 0; Index < AssetView->count(); ++Index)
    {
        auto* Item = AssetView->item(Index);
        if (Item->data(AssetIdRole).toString() == AssetIdentifier
            && (!Key.HasSubAsset() || Item->data(SubAssetIdRole).toString() == SubAssetIdentifier))
        {
            AssetView->setCurrentItem(Item, QItemSelectionModel::ClearAndSelect);
            AssetView->scrollToItem(Item, QAbstractItemView::PositionAtCenter);
            AssetView->setFocus(Qt::OtherFocusReason);
            return true;
        }
    }
    return false;
}

void ContentBrowserWidget::SetCurrentFolder(const QString& Folder)
{
    const bool ValidMount = Folder == "/Game" || Folder.startsWith("/Game/")
        || Folder == "/Engine" || Folder.startsWith("/Engine/");
    const QString NextFolder = ValidMount ? Folder : QString("/Game");
    if (NextFolder != CurrentFolder)
    {
        const QSignalBlocker Block(Search);
        Search->clear();
    }
    CurrentFolder = NextFolder;
    {
        const QSignalBlocker Block(SourcesTree);
        for (QTreeWidgetItemIterator It(SourcesTree); *It; ++It)
        {
            if ((*It)->data(0, FolderPathRole).toString() == CurrentFolder)
            {
                SourcesTree->setCurrentItem(*It);
                for (auto* Parent = (*It)->parent(); Parent != nullptr; Parent = Parent->parent())
                {
                    Parent->setExpanded(true);
                }
                SourcesTree->scrollToItem(*It);
                break;
            }
        }
    }
    Explorer->SetFolder(CurrentFolder, Registry != nullptr && IsWritableGameFolder(CurrentFolder));
    static_cast<AssetListWidget*>(AssetView)->bWritable = Registry != nullptr && IsWritableGameFolder(CurrentFolder);
    RefreshAssetView();
}

void ContentBrowserWidget::SelectImportFolder()
{
    const QString Folder = QFileDialog::getExistingDirectory(this, tr("Импорт папки"));
    if (!Folder.isEmpty())
    {
        ImportFiles({Folder});
    }
}

void ContentBrowserWidget::SelectImportFiles()
{
    const QStringList SourceFiles = QFileDialog::getOpenFileNames(
        this,
        tr("Импорт файлов"),
        QString{},
        tr("Ресурсы (*.glb *.fbx *.png *.jpg *.jpeg);;Модели GLB (*.glb);;Модели FBX (*.fbx);;Изображения (*.png *.jpg *.jpeg)"));
    if (!SourceFiles.isEmpty())
    {
        ImportFiles(SourceFiles);
    }
}

void ContentBrowserWidget::ImportFiles(const QStringList& SourceFiles)
{
    if (Registry == nullptr)
    {
        QMessageBox::warning(this, tr("Импорт не выполнен"), tr("Сначала откройте проект."));
        return;
    }
    if (CurrentFolder != "/Game" && !CurrentFolder.startsWith("/Game/"))
    {
        QMessageBox::warning(this, tr("Импорт не выполнен"), tr("Ресурсы движка доступны только для чтения. Выберите папку в Assets."));
        return;
    }

    const EditorUi::AssetImportSources Sources = EditorUi::CollectAssetImportSources(SourceFiles);
    if (!Sources.Errors.isEmpty())
    {
        QMessageBox::warning(this, tr("Импорт не выполнен"), Sources.Errors.join('\n'));
        return;
    }
    const QStringList& SupportedFiles = Sources.Files;
    if (SupportedFiles.isEmpty())
    {
        QMessageBox::warning(this, tr("Импорт не выполнен"),
            tr("Поддерживаемые файлы не найдены. Выберите GLB, статический FBX, PNG или JPEG."));
        return;
    }

    const QString InitialFolder = CurrentFolder == "/Game"
        ? QString{}
        : CurrentFolder.mid(QString("/Game/").size());
    AssetImportDialog Dialog(SupportedFiles, InitialFolder, this, Sources.RelativeFolders, Sources.SkippedFiles);
    if (Dialog.exec() != QDialog::Accepted)
    {
        return;
    }

    AssetImporter Importer(*Registry);
    QStringList Failures;
    int ImportedCount = 0;
    for (int SourceIndex = 0; SourceIndex < SupportedFiles.size(); ++SourceIndex)
    {
        const QString DestinationPath = Dialog.GetDestinationRelativePath(SourceIndex);

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
    if (ImportedCount > 0)
    {
        emit AssetsChanged();
    }
    if (Failures.isEmpty())
    {
        QMessageBox::information(
            this,
            tr("Импорт завершён"),
            tr("Импортировано ресурсов: %1.").arg(ImportedCount));
    }
    else
    {
        QMessageBox::warning(
            this,
            tr("Импорт завершён с ошибками"),
            tr("Импортировано ресурсов: %1.\n\n%2").arg(ImportedCount).arg(Failures.join('\n')));
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
    EditorAssetPayload Payload;
    if (Operations == nullptr || !DecodeEditorAssetPayload(QGuiApplication::clipboard()->mimeData(), Payload))
    {
        return false;
    }
    const AssetDiagnostic Result = Operations->DuplicateAsset(Payload.Key, CurrentFolder.toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось вставить"), QString::fromStdString(Result.Message));
        Refresh();
        return false;
    }
    Refresh();
    emit AssetsChanged();
    return true;
}

void ContentBrowserWidget::ShowAssetContextMenu(const QPoint& Position)
{
    QListWidgetItem* Item = AssetView->itemAt(Position);
    QMenu Menu(this);
    if (IsWritableGameFolder(CurrentFolder))
    {
        Menu.addAction(tr("Создать материал"), this, [this]() { CreateMaterial(false); });
        Menu.addAction(tr("Создать экземпляр материала"), this, [this]() { CreateMaterial(true); });
    }
    if (Item != nullptr && (Item->data(AssetTypeRole).toString() == "Material"
        || Item->data(AssetTypeRole).toString() == "MaterialInstance"))
    {
        AssetView->setCurrentItem(Item);
        Menu.addAction(tr("Создать дочерний экземпляр"), this, [this]() { CreateMaterial(true, true); });
    }
    if (Item != nullptr)
    {
        AssetView->setCurrentItem(Item);
        if (Item->data(ItemKindRole).toInt() == FolderItemKind)
        {
            const QString Folder = Item->data(FolderPathRole).toString();
            if (IsWritableGameFolder(Folder))
            {
                Menu.addAction(tr("Новая папка"), this, &ContentBrowserWidget::CreateFolder);
                if (Folder != "/Game")
                {
                    Menu.addAction(tr("Переименовать"), this, &ContentBrowserWidget::RenameSelectedFolder);
                    Menu.addAction(tr("Удалить"), this, &ContentBrowserWidget::DeleteSelectedFolder);
                }
            }
        }
        else if (Item->data(ItemKindRole).toInt() == AssetItemKind)
        {
            Menu.addAction(tr("Копировать"), this, &ContentBrowserWidget::CopySelectionToClipboard);
            if (Item->data(VirtualPathRole).toString().startsWith("/Game/"))
            {
                Menu.addAction(tr("Переименовать"), this, &ContentBrowserWidget::RenameSelectedAsset);
                Menu.addAction(tr("Удалить"), this, &ContentBrowserWidget::DeleteSelectedAsset);
            }
        }
        Menu.addSeparator();
    }
    else if (IsWritableGameFolder(CurrentFolder))
    {
        Menu.addAction(tr("Новая папка"), this, &ContentBrowserWidget::CreateFolder);
        Menu.addSeparator();
    }
    QAction* PasteAction = Menu.addAction(tr("Вставить"), this, &ContentBrowserWidget::PasteFromClipboard);
    PasteAction->setEnabled(
        IsWritableGameFolder(CurrentFolder)
        && QGuiApplication::clipboard()->mimeData()->hasFormat(SakuraAssetMimeType));
    Menu.exec(AssetView->viewport()->mapToGlobal(Position));
}

void ContentBrowserWidget::ShowSourcesContextMenu(const QPoint& Position)
{
    QTreeWidgetItem* Item = SourcesTree->itemAt(Position);
    if (Item != nullptr)
    {
        SourcesTree->setCurrentItem(Item);
        SetCurrentFolder(Item->data(0, FolderPathRole).toString());
    }

    const QString Folder = SelectedFolderPath();
    QMenu Menu(this);
    if (IsWritableGameFolder(Folder))
    {
        Menu.addAction(tr("Новая папка"), this, &ContentBrowserWidget::CreateFolder);
        if (Folder != "/Game" && Folder.startsWith("/Game/"))
        {
            Menu.addAction(tr("Переименовать"), this, &ContentBrowserWidget::RenameSelectedFolder);
            Menu.addAction(tr("Удалить"), this, &ContentBrowserWidget::DeleteSelectedFolder);
        }
    }
    if (Menu.actions().isEmpty())
    {
        return;
    }
    Menu.exec(SourcesTree->viewport()->mapToGlobal(Position));
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
        QMessageBox::warning(this, tr("Не удалось переименовать"), tr("Ресурсы движка доступны только для чтения."));
        return;
    }

    const QString SubAssetText = Item->data(SubAssetIdRole).toString();
    const QFileInfo SourceInformation(SourceVirtualPath);
    bool bAccepted = false;
    const QString NewBaseName = QInputDialog::getText(
        this,
        tr("Переименовать ресурс"),
        tr("Имя ресурса"),
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
        QMessageBox::warning(this, tr("Не удалось переименовать"), tr("Введите имя ресурса без разделителей пути."));
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
        const AssetDiagnostic Result = Operations->RenameSubAsset(Key, NewBaseName.toUtf8().toStdString());
        if (Result.HasError())
        {
            QMessageBox::warning(this, tr("Не удалось переименовать"), QString::fromStdString(Result.Message));
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

    const AssetDiagnostic Result = Operations->RenameAsset(
        SourceVirtualPath.toUtf8().toStdString(),
        DestinationVirtualPath.toUtf8().toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось переименовать"), QString::fromStdString(Result.Message));
        return;
    }
    Refresh();
    emit AssetsChanged();
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
        QMessageBox::warning(this, tr("Не удалось удалить"), tr("Ресурсы движка доступны только для чтения."));
        return;
    }
    if (QMessageBox::question(
            this,
            tr("Удалить ресурс"),
            tr("Удалить %1?\nФайл будет удалён вместе с последним ресурсом в нём. Ссылки на удалённый ресурс в сценах перестанут работать.")
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
        QMessageBox::warning(this, tr("Не удалось удалить"), tr("Ресурс больше не зарегистрирован."));
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

    const AssetDiagnostic Result = Operations->DeleteAsset(Key);
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось удалить"), QString::fromStdString(Result.Message));
        return;
    }

    Refresh();
    emit AssetsChanged();
}

void ContentBrowserWidget::MoveAsset(
    const QString& SourceVirtualPath,
    const QString& DestinationFolder)
{
    if (Registry == nullptr)
    {
        return;
    }
    if (!SourceVirtualPath.startsWith("/Game/") || !IsWritableGameFolder(DestinationFolder))
    {
        QMessageBox::warning(this, tr("Не удалось переместить"), tr("Перемещать ресурсы можно только внутри Assets."));
        return;
    }

    const QString DestinationVirtualPath = DestinationFolder + "/" + QFileInfo(SourceVirtualPath).fileName();
    if (DestinationVirtualPath == SourceVirtualPath)
    {
        return;
    }
    const AssetDiagnostic Result = Operations->RenameAsset(
        SourceVirtualPath.toUtf8().toStdString(),
        DestinationVirtualPath.toUtf8().toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось переместить"), QString::fromStdString(Result.Message));
        return;
    }
    Refresh();
    emit AssetsChanged();
}

void ContentBrowserWidget::RenameSelection()
{
    QListWidgetItem* AssetItem = AssetView != nullptr ? AssetView->currentItem() : nullptr;
    if (AssetItem != nullptr && AssetView->hasFocus())
    {
        if (AssetItem->data(ItemKindRole).toInt() == FolderItemKind)
        {
            RenameSelectedFolder();
            return;
        }
        if (AssetItem->data(ItemKindRole).toInt() == AssetItemKind)
        {
            RenameSelectedAsset();
            return;
        }
    }
    RenameSelectedFolder();
}

void ContentBrowserWidget::DeleteSelection()
{
    QListWidgetItem* AssetItem = AssetView != nullptr ? AssetView->currentItem() : nullptr;
    if (AssetItem != nullptr && AssetView->hasFocus())
    {
        if (AssetItem->data(ItemKindRole).toInt() == FolderItemKind)
        {
            DeleteSelectedFolder();
            return;
        }
        if (AssetItem->data(ItemKindRole).toInt() == AssetItemKind)
        {
            DeleteSelectedAsset();
            return;
        }
    }
    DeleteSelectedFolder();
}

QString ContentBrowserWidget::SelectedFolderPath() const
{
    if (AssetView != nullptr && AssetView->hasFocus())
    {
        QListWidgetItem* Item = AssetView->currentItem();
        if (Item != nullptr && Item->data(ItemKindRole).toInt() == FolderItemKind)
        {
            return Item->data(FolderPathRole).toString();
        }
    }
    if (SourcesTree != nullptr)
    {
        QTreeWidgetItem* Item = SourcesTree->currentItem();
        if (Item != nullptr)
        {
            return Item->data(0, FolderPathRole).toString();
        }
    }
    return CurrentFolder;
}

bool ContentBrowserWidget::IsWritableGameFolder(const QString& Folder) const
{
    return Folder == "/Game" || Folder.startsWith("/Game/");
}

bool ContentBrowserWidget::ResolveGameFolderPath(const QString& VirtualFolder, std::filesystem::path& OutAbsolute) const
{
    return Operations != nullptr && Operations->ResolveGameFolder(VirtualFolder.toStdString(), OutAbsolute);
}

void ContentBrowserWidget::CollectGameFoldersOnDisk(std::set<QString>& OutFolders) const
{
    if (Registry == nullptr || Registry->GetGameContentRoot().empty())
    {
        return;
    }
    const std::filesystem::path ContentRoot = Registry->GetGameContentRoot();
    std::error_code Error;
    if (!std::filesystem::exists(ContentRoot, Error))
    {
        return;
    }
    for (const std::filesystem::directory_entry& Entry
        : std::filesystem::recursive_directory_iterator(ContentRoot, Error))
    {
        if (Error || !Entry.is_directory())
        {
            continue;
        }
        std::string Relative;
        AssetDiagnostic Diagnostic{};
        if (!AssetPath::TryMakeRelative(Entry.path(), ContentRoot, Relative, Diagnostic) || Relative.empty())
        {
            continue;
        }
        QString Folder = QString::fromStdString(AssetPath::MakeVirtualPath(AssetMount::Game, Relative));
        while (Folder.count('/') > 1)
        {
            OutFolders.insert(Folder);
            Folder = Folder.section('/', 0, -2);
        }
    }
}

void ContentBrowserWidget::RescanAndRefresh()
{
    if (Registry == nullptr)
    {
        return;
    }
    const AssetDiagnostic Result = Registry->ScanContent();
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось обновить список ресурсов"), QString::fromStdString(Result.Message));
    }
    Refresh();
    emit AssetsChanged();
}

void ContentBrowserWidget::CreateFolder()
{
    QString ParentFolder = CurrentFolder;
    if (AssetView != nullptr)
    {
        QListWidgetItem* Item = AssetView->currentItem();
        if (Item != nullptr && Item->data(ItemKindRole).toInt() == FolderItemKind)
        {
            ParentFolder = Item->data(FolderPathRole).toString();
        }
    }
    if (Registry == nullptr || !IsWritableGameFolder(ParentFolder))
    {
        QMessageBox::warning(this, tr("Не удалось создать папку"), tr("Ресурсы движка доступны только для чтения. Выберите папку в Assets."));
        return;
    }

    bool bAccepted = false;
    const QString FolderName = QInputDialog::getText(
        this,
        tr("Новая папка"),
        tr("Имя папки"),
        QLineEdit::Normal,
        tr("Новая папка"),
        &bAccepted).trimmed();
    if (!bAccepted)
    {
        return;
    }
    if (FolderName.isEmpty() || FolderName == "." || FolderName == ".."
        || FolderName.contains('/') || FolderName.contains('\\'))
    {
        QMessageBox::warning(this, tr("Не удалось создать папку"), tr("Введите имя папки без разделителей пути."));
        return;
    }

    const QString NewFolder = ParentFolder + "/" + FolderName;
    const AssetDiagnostic Result = Operations->CreateFolder(NewFolder.toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось создать папку"), QString::fromStdString(Result.Message));
        return;
    }

    PrintString(std::string("ContentBrowser: created folder ") + NewFolder.toStdString());
    Refresh();
    SetCurrentFolder(NewFolder);
}

void ContentBrowserWidget::RenameSelectedFolder()
{
    const QString SourceFolder = SelectedFolderPath();
    if (Registry == nullptr || !IsWritableGameFolder(SourceFolder) || SourceFolder == "/Game")
    {
        QMessageBox::warning(this, tr("Не удалось переименовать"), tr("Переименовывать можно только вложенные папки в Assets."));
        return;
    }

    bool bAccepted = false;
    const QString NewName = QInputDialog::getText(
        this,
        tr("Переименовать папку"),
        tr("Имя папки"),
        QLineEdit::Normal,
        SourceFolder.section('/', -1),
        &bAccepted).trimmed();
    if (!bAccepted)
    {
        return;
    }
    if (NewName.isEmpty() || NewName == "." || NewName == ".."
        || NewName.contains('/') || NewName.contains('\\'))
    {
        QMessageBox::warning(this, tr("Не удалось переименовать"), tr("Введите имя папки без разделителей пути."));
        return;
    }

    const QString DestinationFolder = SourceFolder.section('/', 0, -2) + "/" + NewName;
    if (DestinationFolder == SourceFolder)
    {
        return;
    }

    const AssetDiagnostic Result = Operations->RenameFolder(SourceFolder.toStdString(), DestinationFolder.toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось переименовать"), QString::fromStdString(Result.Message));
        Refresh();
        return;
    }

    PrintString(std::string("ContentBrowser: renamed folder ")
        + SourceFolder.toStdString()
        + " -> "
        + DestinationFolder.toStdString());
    if (CurrentFolder == SourceFolder || CurrentFolder.startsWith(SourceFolder + "/"))
    {
        CurrentFolder = DestinationFolder + CurrentFolder.mid(SourceFolder.size());
    }
    RescanAndRefresh();
}

void ContentBrowserWidget::DeleteSelectedFolder()
{
    const QString Folder = SelectedFolderPath();
    if (Registry == nullptr || BoundEngine == nullptr || !IsWritableGameFolder(Folder) || Folder == "/Game")
    {
        QMessageBox::warning(this, tr("Не удалось удалить"), tr("Удалять можно только вложенные папки в Assets."));
        return;
    }

    const auto Answer = QMessageBox::question(
        this,
        tr("Удалить папку"),
        tr("Удалить папку %1 и все ресурсы внутри?\nСсылки на удалённые ресурсы в сценах перестанут работать.")
            .arg(Folder),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    if (Answer != QMessageBox::Yes)
    {
        return;
    }

    const AssetDiagnostic Result = Operations->DeleteFolder(Folder.toStdString());
    if (Result.HasError())
    {
        QMessageBox::warning(this, tr("Не удалось удалить"), QString::fromStdString(Result.Message));
        Refresh();
        return;
    }

    PrintString(std::string("ContentBrowser: deleted folder ") + Folder.toStdString());
    if (CurrentFolder == Folder || CurrentFolder.startsWith(Folder + "/"))
    {
        CurrentFolder = Folder.section('/', 0, -2);
        if (CurrentFolder.isEmpty())
        {
            CurrentFolder = "/Game";
        }
    }
    RescanAndRefresh();
}

void ContentBrowserWidget::AddAssetItem(const AssetRegistryEntry& Entry)
{
    const QString VirtualPath = QString::fromStdString(Entry.VirtualPath);
    auto* Item = new QListWidgetItem(
        MakeAssetIcon(Entry.Metadata.Type),
        QFileInfo(VirtualPath).completeBaseName(),
        AssetView);
    Item->setData(ItemKindRole, AssetItemKind);
    Item->setData(EditorUi::ExplorerSubtitleRole, QString::fromUtf8(AssetTypeToString(Entry.Metadata.Type)));
    Item->setData(AssetIdRole, QString::fromStdString(Entry.Metadata.Guid.ToString()));
    Item->setData(AssetTypeRole, QString::fromStdString(Entry.Metadata.Type.GetIdentifier()));
    Item->setData(VirtualPathRole, VirtualPath);
    Item->setToolTip(VirtualPath + "\n" + QString::fromUtf8(AssetTypeToString(Entry.Metadata.Type)));
    Item->setFlags(Item->flags() | Qt::ItemIsDragEnabled);
    AssetThumbnails::Bind(*AssetView->model(), AssetView->indexFromItem(Item), *Registry, {Entry.Metadata.Guid, std::nullopt});
}

void ContentBrowserWidget::AddSubAssetItem(const AssetRegistryEntry& Entry, const SubAssetRecord& SubAsset)
{
    auto* Item = new QListWidgetItem(
        MakeAssetIcon(SubAsset.Type),
        QString::fromStdString(SubAsset.Name),
        AssetView);
    Item->setData(ItemKindRole, AssetItemKind);
    Item->setData(EditorUi::ExplorerSubtitleRole, QString::fromUtf8(AssetTypeToString(SubAsset.Type)));
    Item->setData(AssetIdRole, QString::fromStdString(Entry.Metadata.Guid.ToString()));
    Item->setData(SubAssetIdRole, QString::fromStdString(SubAsset.Id.ToString()));
    Item->setData(AssetTypeRole, QString::fromStdString(SubAsset.Type.GetIdentifier()));
    Item->setData(VirtualPathRole, QString::fromStdString(Entry.VirtualPath));
    Item->setToolTip(QString::fromStdString(Entry.VirtualPath) + "\n" + QString::fromUtf8(AssetTypeToString(SubAsset.Type)));
    Item->setFlags(Item->flags() | Qt::ItemIsDragEnabled);
    AssetThumbnails::Bind(*AssetView->model(), AssetView->indexFromItem(Item), *Registry, {Entry.Metadata.Guid, SubAsset.Id});
}

void ContentBrowserWidget::CreateMaterial(bool bInstance, bool bSelectedParent)
{
    if (Registry == nullptr)
    {
        return;
    }
    AssetKey Parent;
    bool bAccepted = false;
    MaterialDomain Domain = MaterialDomain::Surface;
    if (bSelectedParent)
    {
        EditorAssetPayload Payload;
        if (!TryBuildAssetPayload(AssetView->currentItem(), Payload))
        {
            return;
        }
        Parent = Payload.Key;
    }
    else if (bInstance)
    {
        QStringList Parents;
        for (const auto& Type : {MaterialAssetType, MaterialInstanceAssetType})
        {
            for (const auto& Entry : Registry->FindByType(Type))
            {
                Parents.append(QString::fromStdString(Entry.VirtualPath));
            }
        }
        Parents.sort();
        const auto Selected = QInputDialog::getItem(this, tr("Экземпляр материала"), tr("Родительский материал"), Parents, 0, false, &bAccepted);
        if (!bAccepted || Selected.isEmpty())
        {
            return;
        }
        AssetRegistryEntry Entry;
        Registry->TryGetByPath(Selected.toStdString(), Entry);
        Parent = AssetKey{Entry.Metadata.Guid, {}};
    }
    const auto Name = QInputDialog::getText(this, tr("Создать материал"), tr("Имя ресурса"), QLineEdit::Normal, {}, &bAccepted);
    if (!bAccepted || Name.isEmpty())
    {
        return;
    }
    if (Name.contains('/') || Name.contains('\\') || Name.contains('.') || Name.contains(':'))
    {
        QMessageBox::warning(this, tr("Создать материал"), tr("Введите имя ресурса без расширения и пути."));
        return;
    }
    QString Folder = CurrentFolder;
    if (!IsWritableGameFolder(Folder))
    {
        Folder = "/Game";
    }
    QString Extension = ".material";
    if (bInstance)
    {
        Extension = ".materialinstance";
    }
    const QString Path = Folder + "/" + Name + Extension;
    AssetKey Created;
    const auto Diagnostic = MaterialAssetCreation::Create(*Registry, Path.toStdString(), Parent, Domain, Created);
    if (Diagnostic.HasError())
    {
        QMessageBox::warning(this, tr("Создать материал"), QString::fromStdString(Diagnostic.Message));
        return;
    }
    RescanAndRefresh();
    AssetRegistryEntry Entry;
    Registry->TryResolveKey(Created, Entry);
    emit AssetActivated(QString::fromStdString(Created.Asset.ToString()), {},
        QString::fromStdString(Entry.Metadata.Type.GetIdentifier()), Path);
}
