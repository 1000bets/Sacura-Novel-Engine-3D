#pragma once

#include <QWidget>

class QBoxLayout;
class QHBoxLayout;
class QLineEdit;
class QToolButton;
class QLabel;
class QSplitter;

namespace EditorUi
{
// Chrome shared by all asset types. ContentBrowserWidget supplies the registry and operations.
class AssetExplorerView : public QWidget
{
    Q_OBJECT
public:
    explicit AssetExplorerView(QWidget* Parent = nullptr);
    void SetFolder(const QString& VirtualFolder, bool Writable);
    void SetEmptyState(bool Empty, bool Searching);

    QLineEdit* Search = nullptr;
    QToolButton* NewFolder = nullptr;
    QToolButton* ImportFiles = nullptr;
    QToolButton* ImportFolder = nullptr;
    QSplitter* Body = nullptr;
    QLabel* EmptyState = nullptr;

signals:
    void FolderRequested(QString VirtualFolder);

protected:
    void resizeEvent(QResizeEvent* Event) override;

private:
    QBoxLayout* ToolbarLayout = nullptr;
    QHBoxLayout* BreadcrumbLayout = nullptr;
    QWidget* Breadcrumbs = nullptr;
};
}
