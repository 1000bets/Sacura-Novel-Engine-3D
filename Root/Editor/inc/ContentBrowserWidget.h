#pragma once

#include "Assets/AssetRegistry.h"

#include <QWidget>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

class ContentBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ContentBrowserWidget(QWidget* Parent = nullptr);

    void SetRegistry(AssetRegistry* Registry);
    void Refresh();

private:
    void AddEntry(QTreeWidgetItem* Root, const AssetRegistryEntry& Entry);
    void ApplyFilter(const QString& Text);

    AssetRegistry* Registry = nullptr;
    QLineEdit* Search = nullptr;
    QTreeWidget* Tree = nullptr;
};
