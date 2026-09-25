#include "ContentBrowserWidget.h"

#include "EditorAssetMime.h"

#include <QDrag>
#include <QLineEdit>
#include <QMimeData>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int AssetIdRole = Qt::UserRole;
constexpr int SubAssetIdRole = Qt::UserRole + 1;
constexpr int AssetTypeRole = Qt::UserRole + 2;
constexpr int VirtualPathRole = Qt::UserRole + 3;

class AssetTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;

protected:
    void startDrag(Qt::DropActions) override
    {
        QTreeWidgetItem* Item = currentItem();
        if (Item == nullptr || Item->data(0, AssetIdRole).toString().isEmpty())
        {
            return;
        }
        EditorAssetPayload Payload;
        if (!Guid::TryParse(Item->data(0, AssetIdRole).toString().toStdString(), Payload.Key.Asset))
        {
            return;
        }
        const QString SubAssetText = Item->data(0, SubAssetIdRole).toString();
        if (!SubAssetText.isEmpty())
        {
            SubAssetId SubAsset{};
            if (Guid::TryParse(SubAssetText.toStdString(), SubAsset))
            {
                Payload.Key.SubAsset = SubAsset;
            }
        }
        Payload.Type = static_cast<AssetType>(Item->data(0, AssetTypeRole).toInt());
        Payload.VirtualPath = Item->data(0, VirtualPathRole).toString();
        auto* MimeData = new QMimeData();
        MimeData->setData(SakuraAssetMimeType, EncodeEditorAssetPayload(Payload));
        auto* Drag = new QDrag(this);
        Drag->setMimeData(MimeData);
        Drag->exec(Qt::CopyAction);
    }
};
}

ContentBrowserWidget::ContentBrowserWidget(QWidget* Parent)
    : QWidget(Parent)
{
    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(6, 6, 6, 6);
    Search = new QLineEdit(this);
    Search->setPlaceholderText(tr("Search Content"));
    Tree = new AssetTreeWidget(this);
    Tree->setHeaderLabels({tr("Asset"), tr("Type")});
    Tree->setDragEnabled(true);
    Tree->setSelectionMode(QAbstractItemView::SingleSelection);
    Layout->addWidget(Search);
    Layout->addWidget(Tree, 1);
    connect(Search, &QLineEdit::textChanged, this, &ContentBrowserWidget::ApplyFilter);
}

void ContentBrowserWidget::SetRegistry(AssetRegistry* InRegistry)
{
    Registry = InRegistry;
    Refresh();
}

void ContentBrowserWidget::Refresh()
{
    Tree->clear();
    if (Registry == nullptr)
    {
        return;
    }
    auto* GameRoot = new QTreeWidgetItem(Tree, {tr("Game Content")});
    auto* EngineRoot = new QTreeWidgetItem(Tree, {tr("Engine Content")});
    std::vector<AssetRegistryEntry> Entries = Registry->FindByDirectory("/Game");
    std::vector<AssetRegistryEntry> EngineEntries = Registry->FindByDirectory("/Engine");
    Entries.insert(Entries.end(), EngineEntries.begin(), EngineEntries.end());
    std::sort(Entries.begin(), Entries.end(), [](const AssetRegistryEntry& First, const AssetRegistryEntry& Second)
    {
        return First.VirtualPath < Second.VirtualPath;
    });
    for (const AssetRegistryEntry& Entry : Entries)
    {
        AddEntry(Entry.Mount == AssetMount::Engine ? EngineRoot : GameRoot, Entry);
    }
    GameRoot->setExpanded(true);
    EngineRoot->setExpanded(true);
    Tree->resizeColumnToContents(0);
    ApplyFilter(Search->text());
}

void ContentBrowserWidget::AddEntry(QTreeWidgetItem* Root, const AssetRegistryEntry& Entry)
{
    auto* Item = new QTreeWidgetItem(Root, {
        QString::fromStdString(Entry.VirtualPath),
        QString::fromUtf8(AssetTypeToString(Entry.Metadata.Type))});
    Item->setData(0, AssetIdRole, QString::fromStdString(Entry.Metadata.Guid.ToString()));
    Item->setData(0, AssetTypeRole, static_cast<int>(Entry.Metadata.Type));
    Item->setData(0, VirtualPathRole, QString::fromStdString(Entry.VirtualPath));
    Item->setFlags(Item->flags() | Qt::ItemIsDragEnabled);
    for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
    {
        auto* Child = new QTreeWidgetItem(Item, {
            QString::fromStdString(SubAsset.Name),
            QString::fromUtf8(AssetTypeToString(SubAsset.Type))});
        Child->setData(0, AssetIdRole, QString::fromStdString(Entry.Metadata.Guid.ToString()));
        Child->setData(0, SubAssetIdRole, QString::fromStdString(SubAsset.Id.ToString()));
        Child->setData(0, AssetTypeRole, static_cast<int>(SubAsset.Type));
        Child->setData(0, VirtualPathRole, QString::fromStdString(Entry.VirtualPath));
        Child->setFlags(Child->flags() | Qt::ItemIsDragEnabled);
    }
}

void ContentBrowserWidget::ApplyFilter(const QString& Text)
{
    const QString Filter = Text.trimmed();
    for (int RootIndex = 0; RootIndex < Tree->topLevelItemCount(); ++RootIndex)
    {
        QTreeWidgetItem* Root = Tree->topLevelItem(RootIndex);
        bool bRootVisible = false;
        for (int EntryIndex = 0; EntryIndex < Root->childCount(); ++EntryIndex)
        {
            QTreeWidgetItem* Entry = Root->child(EntryIndex);
            bool bEntryVisible = Entry->text(0).contains(Filter, Qt::CaseInsensitive)
                || Entry->text(1).contains(Filter, Qt::CaseInsensitive);
            for (int ChildIndex = 0; ChildIndex < Entry->childCount(); ++ChildIndex)
            {
                QTreeWidgetItem* Child = Entry->child(ChildIndex);
                const bool bChildVisible = Child->text(0).contains(Filter, Qt::CaseInsensitive)
                    || Child->text(1).contains(Filter, Qt::CaseInsensitive);
                Child->setHidden(!Filter.isEmpty() && !bChildVisible);
                bEntryVisible = bEntryVisible || bChildVisible;
            }
            Entry->setHidden(!Filter.isEmpty() && !bEntryVisible);
            bRootVisible = bRootVisible || bEntryVisible;
        }
        Root->setHidden(!Filter.isEmpty() && !bRootVisible);
    }
}
