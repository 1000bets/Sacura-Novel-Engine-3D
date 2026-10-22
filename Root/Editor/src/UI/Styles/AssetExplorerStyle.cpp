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
    return {132, 122};
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
    Icon.paint(Painter, QRect(Bounds.center().x() - 18, Bounds.top() + 12, 36, 36));
    const QFont NameFont = ControlStyle::Font(12, true);
    Painter->setFont(NameFont);
    Painter->setPen(EditorStyle::Color("--text-primary"));
    const QRect TextBounds = Bounds.adjusted(8, 56, -8, -24);
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
    Painter->setClipRect(TextBounds);
    Text.draw(Painter, TextBounds.topLeft());
    Painter->setClipping(false);
    Painter->setFont(ControlStyle::Font(11));
    Painter->setPen(EditorStyle::Color("--text-secondary"));
    const QString Subtitle = Painter->fontMetrics().elidedText(
        Index.data(ExplorerSubtitleRole).toString(), Qt::ElideRight, Bounds.width() - 16);
    const QRect SubtitleBounds(Bounds.left() + 8, TextBounds.top() + static_cast<int>(Height) + 7,
                               Bounds.width() - 16, 16);
    Painter->drawText(SubtitleBounds, Qt::AlignHCenter | Qt::AlignTop, Subtitle);
    Painter->restore();
}
}
