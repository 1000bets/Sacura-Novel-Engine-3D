#pragma once
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QTreeWidget;

namespace EditorUi
{
// The scene adapter supplies rows and identities; this view only filters/displays them.
class HierarchyPanelView final : public QWidget
{
public:
    explicit HierarchyPanelView(QTreeWidget& Tree, QWidget* Parent = nullptr);
    void Refresh();
    QPushButton& GetAddButton() const { return *Add; }
private:
    void ApplyFilter();
    QTreeWidget& Tree;
    QPushButton* Add = nullptr;
    QLineEdit* Search = nullptr;
    QLabel* Count = nullptr;
    QLabel* Footer = nullptr;
};
}
