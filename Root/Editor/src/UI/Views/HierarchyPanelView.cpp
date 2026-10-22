#include "UI/Views/HierarchyPanelView.h"
#include "UI/Styles/StudioIcons.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QApplication>
#include <QStyle>
#include <functional>

namespace EditorUi
{
HierarchyPanelView::HierarchyPanelView(QTreeWidget& InTree, QWidget* Parent) : QWidget(Parent), Tree(InTree)
{
    setObjectName("HierarchyPanelContent");
    setAttribute(Qt::WA_StyledBackground);
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
    auto* SearchBox = new QWidget(SearchRow);
    SearchBox->setObjectName("HierarchySearchBox");
    auto* SearchBoxLayout = new QHBoxLayout(SearchBox);
    SearchBoxLayout->setContentsMargins(9, 0, 7, 0);
    SearchBoxLayout->setSpacing(7);
    auto* SearchIcon = new QLabel(SearchBox);
    SearchIcon->setPixmap(MakeStudioIcon(StudioIcon::Search).pixmap(14, 14));
    SearchIcon->setFixedSize(14, 14);
    SearchBoxLayout->addWidget(SearchIcon);
    Search = new QLineEdit(SearchBox);
    Search->setObjectName("HierarchySearch");
    Search->setPlaceholderText(tr("Найти объект…"));
    Search->setAccessibleName(tr("Найти объект"));
    Search->setFrame(false);
    Search->setClearButtonEnabled(true);
    SearchBoxLayout->addWidget(Search, 1);
    SearchBox->setFocusProxy(Search);
    connect(qApp, &QApplication::focusChanged, SearchBox, [SearchBox, Search = Search](QWidget*, QWidget* Focus) {
        const bool Focused = Focus == Search || (Focus && Search->isAncestorOf(Focus));
        if (SearchBox->property("searchFocused").toBool() == Focused) { return; }
        SearchBox->setProperty("searchFocused", Focused);
        SearchBox->style()->unpolish(SearchBox);
        SearchBox->style()->polish(SearchBox);
        SearchBox->update();
    });
    SearchLayout->addWidget(SearchBox, 1);
    Layout->addWidget(SearchRow);
    auto* HeadingRow = new QWidget(this);
    auto* HeadingRowLayout = new QHBoxLayout(HeadingRow);
    HeadingRowLayout->setContentsMargins(8, 0, 8, 4);
    auto* Heading = new QWidget(HeadingRow);
    Heading->setObjectName("SceneObjectsHeading");
    auto* HeadingLayout = new QHBoxLayout(Heading);
    HeadingLayout->setContentsMargins(8, 0, 8, 0);
    HeadingLayout->addWidget(new QLabel(tr("Объекты сцены"), Heading));
    HeadingLayout->addStretch();
    Count = new QLabel(Heading);
    Count->setObjectName("SceneObjectCount");
    Count->setAlignment(Qt::AlignCenter);
    Count->setMinimumWidth(22);
    HeadingLayout->addWidget(Count, 0, Qt::AlignVCenter);
    HeadingRowLayout->addWidget(Heading);
    Layout->addWidget(HeadingRow);
    Tree.setAnimated(true);
    Tree.setUniformRowHeights(true);
    Tree.setIndentation(16);
    Tree.setIconSize(QSize(14, 14));
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
