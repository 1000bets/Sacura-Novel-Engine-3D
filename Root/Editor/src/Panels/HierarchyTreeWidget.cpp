#include "Panels/HierarchyTreeWidget.h"
#include "Shell/EditorMainWindowSupport.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include "UI/Styles/StudioIcons.h"
#include "UI/Styles/HierarchyStyle.h"
#include <QCursor>

void HierarchyTreeWidget::drawRow(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const
{
    QStyleOptionViewItem Row = Option;
    Row.rect = visualRect(Index);
    Row.rect.setRight(viewport()->width() - 1);
    Row.state.setFlag(QStyle::State_Selected, selectionModel()->isSelected(Index));
    Row.state.setFlag(QStyle::State_Enabled, isEnabled() && (Index.flags() & Qt::ItemIsEnabled));
    const QPoint Cursor = viewport()->mapFromGlobal(QCursor::pos());
    Row.state.setFlag(QStyle::State_MouseOver, viewport()->underMouse() && Row.rect.contains(Cursor));
    // Own the whole row: the native drawRow fills ancestor branches with the
    // platform's selection colour even when the item stylesheet is transparent.
    EditorUi::HierarchyStyle::PaintBackground(*Painter, Row.rect, Row.state);
    itemDelegateForIndex(Index)->paint(Painter, Row, Index);
    drawBranches(Painter, QRect(0, Row.rect.top(), Row.rect.left(), Row.rect.height()), Index);
}

void HierarchyTreeWidget::drawBranches(QPainter* Painter, const QRect& Rect, const QModelIndex& Index) const
{
    if (!model()->hasChildren(Index)) { return; }
    const auto Icon = isExpanded(Index) ? EditorUi::StudioIcon::ChevronDown : EditorUi::StudioIcon::ChevronRight;
    const QRect Indicator(Rect.right() - indentation() + 1 + (indentation() - 13) / 2,
                          Rect.top() + (Rect.height() - 13) / 2, 13, 13);
    EditorUi::MakeStudioIcon(Icon).paint(Painter, Indicator);
}

void HierarchyTreeWidget::dragEnterEvent(QDragEnterEvent* Event)
{
    if (Event->source() == this)
    {
        QTreeWidget::dragEnterEvent(Event);
        Event->setDropAction(Qt::CopyAction);
        Event->accept();
        return;
    }
    Event->ignore();
}

void HierarchyTreeWidget::dragMoveEvent(QDragMoveEvent* Event)
{
    if (Event->source() != this)
    {
        Event->ignore();
        return;
    }

    QTreeWidget::dragMoveEvent(Event);

    QTreeWidgetItem* Dragged = currentItem();
    QTreeWidgetItem* DropItem = itemAt(Event->position().toPoint());
    if (Dragged != nullptr
        && DropItem != nullptr
        && dropIndicatorPosition() == QAbstractItemView::OnItem
        && HierarchyItemIsDescendant(Dragged, DropItem))
    {
        Event->ignore();
        return;
    }

    Event->setDropAction(Qt::CopyAction);
    Event->accept();
}

void HierarchyTreeWidget::dropEvent(QDropEvent* Event)
{
    QTreeWidgetItem* Dragged = currentItem();
    if (Dragged == nullptr || !ReparentRequested || Event->source() != this)
    {
        Event->ignore();
        return;
    }

    QTreeWidgetItem* DropItem = itemAt(Event->position().toPoint());
    ObjectHandle NewParent;
    const QAbstractItemView::DropIndicatorPosition DropPosition = dropIndicatorPosition();
    if (DropItem != nullptr && DropPosition == QAbstractItemView::OnItem)
    {
        if (HierarchyItemIsDescendant(Dragged, DropItem) || DropItem == Dragged)
        {
            Event->setDropAction(Qt::IgnoreAction);
            Event->accept();
            return;
        }
        NewParent = LoadObjectHandle(DropItem);
    }
    else if (DropItem != nullptr
        && DropItem != Dragged
        && DropItem->parent() != nullptr
        && (DropPosition == QAbstractItemView::AboveItem
            || DropPosition == QAbstractItemView::BelowItem))
    {
        NewParent = LoadObjectHandle(DropItem->parent());
    }

    const ObjectHandle Target = LoadObjectHandle(Dragged);
    ReparentRequested(Target, NewParent);
    Event->setDropAction(Qt::IgnoreAction);
    Event->accept();
}
