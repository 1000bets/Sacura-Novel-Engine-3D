#pragma once

#include "Assets/AssetRegistry.h"

#include <QString>
#include <QStringList>
#include <QWidget>

#include <vector>

class Engine;
class QEvent;
class QButtonGroup;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QHBoxLayout;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class ContentBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ContentBrowserWidget(QWidget* Parent = nullptr);

    void SetEngine(Engine* EngineInstance);
    void Refresh();
    bool CopySelectionToClipboard();
    bool PasteFromClipboard();

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

signals:
    void AssetActivated(QString AssetId, QString SubAssetIdentifier, QString AssetTypeIdentifier, QString VirtualPath);

private:
    void BuildSourcesTree(const std::vector<AssetRegistryEntry>& Entries);
    void RebuildTypeFilters();
    void RefreshAssetView();
    void SetCurrentFolder(const QString& Folder);
    AssetType GetActiveTypeFilter() const;
    void AddAssetItem(const AssetRegistryEntry& Entry);
    void AddSubAssetItem(const AssetRegistryEntry& Entry, const SubAssetRecord& SubAsset);
    void SelectImportFiles();
    void ImportFiles(const QStringList& SourceFiles);
    void ShowAssetContextMenu(const QPoint& Position);
    void RenameSelectedAsset();
    void DeleteSelectedAsset();
    bool DuplicateClipboardAsset();
    void MoveAsset(const QString& SourceVirtualPath, const QString& DestinationFolder);

    Engine* BoundEngine = nullptr;
    AssetRegistry* Registry = nullptr;
    QLineEdit* Search = nullptr;
    QTreeWidget* SourcesTree = nullptr;
    QListWidget* AssetView = nullptr;
    QHBoxLayout* TypeFilterLayout = nullptr;
    QButtonGroup* TypeFilterGroup = nullptr;
    QToolButton* ImportButton = nullptr;
    QString CurrentFolder = "/Game";
    std::vector<AssetRegistryEntry> CachedEntries;
};
