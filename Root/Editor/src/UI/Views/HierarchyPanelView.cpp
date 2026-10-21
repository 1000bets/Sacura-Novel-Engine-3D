#include "UI/Views/HierarchyPanelView.h"
#include "UI/Styles/StudioIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <functional>

namespace EditorUi
{
HierarchyPanelView::HierarchyPanelView(QTreeWidget& InTree, QWidget* Parent) : QWidget(Parent), Tree(InTree)
{
    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(0, 0, 0, 0);
    Layout->setSpacing(0);
    auto* SearchRow = new QWidget(this);
    auto* SearchLayout = new QHBoxLayout(SearchRow);
    SearchLayout->setContentsMargins(12, 10, 12, 8);
    SearchLayout->setSpacing(6);
    Add = new QPushButton(MakeStudioIcon(StudioIcon::Plus), {}, SearchRow);
    Add->setObjectName("HierarchyAdd");
    Add->setToolTip(tr("Создать объект"));
    SearchLayout->addWidget(Add);
    Search = new QLineEdit(SearchRow);
    Search->setObjectName("HierarchySearch");
    Search->setPlaceholderText(tr("Найти объект…"));
    Search->addAction(MakeStudioIcon(StudioIcon::Search), QLineEdit::LeadingPosition);
    Search->setClearButtonEnabled(true);
    SearchLayout->addWidget(Search, 1);
    Layout->addWidget(SearchRow);
    auto* Heading = new QWidget(this);
    Heading->setObjectName("SceneObjectsHeading");
    auto* HeadingLayout = new QHBoxLayout(Heading);
    HeadingLayout->setContentsMargins(8, 0, 8, 0);
    HeadingLayout->addWidget(new QLabel(tr("Объекты сцены"), Heading));
    HeadingLayout->addStretch();
    Count = new QLabel(Heading);
    Count->setObjectName("SceneObjectCount");
    HeadingLayout->addWidget(Count, 0, Qt::AlignVCenter);
    Layout->addWidget(Heading);
    Layout->addWidget(&Tree, 1);
    Footer = new QLabel(this);
    Footer->setObjectName("HierarchyFooter");
    Layout->addWidget(Footer);
    connect(Search, &QLineEdit::textChanged, this, [this]() { ApplyFilter(); });
}

void HierarchyPanelView::Refresh()
{
    int ObjectCount = 0;
    QTreeWidgetItemIterator Iterator(&Tree);
    while (*Iterator != nullptr)
    {
        ++ObjectCount;
        ++Iterator;
    }
    Count->setText(QString::number(ObjectCount));
    Footer->setText(tr("%1 объектов").arg(ObjectCount));
    ApplyFilter();
}

void HierarchyPanelView::ApplyFilter()
{
    const QString Query = Search->text();
    std::function<bool(QTreeWidgetItem*)> Filter = [&](QTreeWidgetItem* Item)
    {
        bool bMatches = Item->text(0).contains(Query, Qt::CaseInsensitive);
        for (int Index = 0; Index < Item->childCount(); ++Index)
        {
            bMatches = Filter(Item->child(Index)) || bMatches;
        }
        Item->setHidden(!bMatches);
        return bMatches;
    };
    for (int Index = 0; Index < Tree.topLevelItemCount(); ++Index)
    {
        Filter(Tree.topLevelItem(Index));
    }
}
}
