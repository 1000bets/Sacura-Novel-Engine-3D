#pragma once

#include <QStyledItemDelegate>

namespace EditorUi
{
// Presentation-only roles; asset identities remain owned by ContentBrowserWidget.
constexpr int ExplorerSubtitleRole = Qt::UserRole + 6;

class AssetExplorerTileDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
};
}
