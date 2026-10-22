#pragma once

#include "Core/Object/ObjectHandle.h"
#include <QTreeWidget>
#include <functional>

class HierarchyTreeWidget : public QTreeWidget
{
public:
    using QTreeWidget::QTreeWidget;
    std::function<bool(ObjectHandle, ObjectHandle)> ReparentRequested;

protected:
    void drawRow(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;
    void drawBranches(QPainter* Painter, const QRect& Rect, const QModelIndex& Index) const override;
    void dragEnterEvent(QDragEnterEvent* Event) override;
    void dragMoveEvent(QDragMoveEvent* Event) override;
    void dropEvent(QDropEvent* Event) override;
};
