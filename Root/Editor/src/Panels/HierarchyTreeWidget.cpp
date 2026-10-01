#include "Panels/HierarchyTreeWidget.h"
#include "Shell/EditorMainWindowSupport.h"
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>

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
