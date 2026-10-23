#include "UI/Styles/AssetExplorerStyle.h"
#include "UI/Styles/ControlStyle.h"
#include "UI/Styles/EditorStyle.h"

#include <QIcon>
#include <QPainter>
#include <QTextLayout>

namespace EditorUi
{
QSize AssetExplorerTileDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return {144, 176};
}

void AssetExplorerTileDelegate::paint(QPainter* Painter, const QStyleOptionViewItem& Option,
                                    const QModelIndex& Index) const
{
    Painter->save();
    Painter->setRenderHint(QPainter::Antialiasing);
    const QRect Bounds = Option.rect.adjusted(4, 4, -4, -4);
    const bool Selected = Option.state.testFlag(QStyle::State_Selected);
    if (Selected || Option.state.testFlag(QStyle::State_MouseOver))
    {
        Painter->setBrush(EditorStyle::Color(Selected ? "--bg-active" : "--bg-hover"));
        Painter->setPen(Selected ? QPen(EditorStyle::Color("--accent")) : QPen(Qt::NoPen));
        Painter->drawRoundedRect(Bounds.adjusted(1, 1, -1, -1), 3, 3);
    }
    const QIcon Icon = qvariant_cast<QIcon>(Index.data(Qt::DecorationRole));
    Icon.paint(Painter, QRect(Bounds.center().x() - 54, Bounds.top() + 6, 108, 108));
    const QFont NameFont = ControlStyle::Font(12, true);
    Painter->setFont(NameFont);
    Painter->setPen(EditorStyle::Color("--text-primary"));
    const QRect TextBounds = Bounds.adjusted(8, 118, -8, -18);
    QTextOption TextOption(Qt::AlignHCenter);
    TextOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    QTextLayout Text(Index.data(Qt::DisplayRole).toString(), NameFont);
    Text.setTextOption(TextOption);
    Text.beginLayout();
    qreal Height = 0;
    for (int LineIndex = 0; LineIndex < 2; ++LineIndex)
    {
        QTextLine Line = Text.createLine();
        if (!Line.isValid())
        {
            break;
        }
        Line.setLineWidth(TextBounds.width());
        Line.setPosition(QPointF(0, Height));
        Height += Line.height();
    }
    Text.endLayout();
    Painter->setClipRect(QRect(TextBounds.topLeft(), QSize(TextBounds.width(), 32)));
    Text.draw(Painter, TextBounds.topLeft());
    Painter->setClipping(false);
    Painter->setFont(ControlStyle::Font(11));
    Painter->setPen(EditorStyle::Color("--text-secondary"));
    const QString Subtitle = Painter->fontMetrics().elidedText(
        Index.data(ExplorerSubtitleRole).toString(), Qt::ElideRight, Bounds.width() - 16);
    const QRect SubtitleBounds(Bounds.left() + 8, Bounds.bottom() - 17,
                               Bounds.width() - 16, 16);
    Painter->drawText(SubtitleBounds, Qt::AlignHCenter | Qt::AlignTop, Subtitle);
    Painter->restore();
}
}
