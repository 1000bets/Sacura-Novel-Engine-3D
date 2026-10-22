#pragma once

#include <QStyledItemDelegate>

namespace EditorUi
{
// Presentation roles only; object identities remain owned by the scene adapter.
inline constexpr int HierarchyPrimaryCameraRole = Qt::UserRole + 2;
inline constexpr int HierarchyCameraRole = Qt::UserRole + 3;

class HierarchyStyle final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
    void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    void updateEditorGeometry(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    static void PaintBackground(QPainter&, const QRect&, QStyle::State);
};
}
