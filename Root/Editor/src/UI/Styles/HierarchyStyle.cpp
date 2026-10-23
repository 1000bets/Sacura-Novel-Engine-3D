#include "UI/Styles/HierarchyStyle.h"
#include "UI/Styles/ControlStyle.h"
#include "UI/Styles/EditorStyle.h"
#include "UI/Styles/StudioIcons.h"

#include <QPainter>

namespace EditorUi
{
namespace
{
constexpr int RowHeight = 30;
constexpr int Padding = 8;
constexpr int IconSize = 14;
constexpr int Gap = 7;
constexpr int BadgeSize = 12;

QRect TextRect(const QRect& Row, bool Primary)
{
    return Row.adjusted(Padding + IconSize + Gap, 0,
                        -Padding - (Primary ? BadgeSize + Gap : 0), 0);
}
}

QSize HierarchyStyle::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return {0, RowHeight};
}

void HierarchyStyle::PaintBackground(QPainter& Painter, const QRect& Rect, QStyle::State State)
{
    if (!(State & (QStyle::State_Selected | QStyle::State_MouseOver))) { return; }
    Painter.save();
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.setPen(Qt::NoPen);
    // Selection wins over hover, including when keyboard focus is in the Inspector.
    Painter.setBrush(EditorStyle::Color(State & QStyle::State_Selected ? "--bg-active" : "--bg-hover"));
    Painter.drawRoundedRect(Rect, 4, 4);
    Painter.restore();
}

void HierarchyStyle::paint(QPainter* Painter, const QStyleOptionViewItem& Option, const QModelIndex& Index) const
{
    const bool Primary = Index.data(HierarchyPrimaryCameraRole).toBool();
    auto Font = ControlStyle::Font(Index.data(HierarchyCameraRole).toBool() ? 12 : 13);
    if (Index.data(Qt::FontRole).isValid())
    {
        const QFont ItemFont = qvariant_cast<QFont>(Index.data(Qt::FontRole));
        Font.setBold(ItemFont.bold());
        Font.setItalic(ItemFont.italic());
    }
    Painter->save();
    Painter->setClipRect(Option.rect);
    if (!(Option.state & QStyle::State_Enabled)) { Painter->setOpacity(.4); }
    const auto Icon = qvariant_cast<QIcon>(Index.data(Qt::DecorationRole));
    Icon.paint(Painter, QRect(Option.rect.left() + Padding,
                            Option.rect.top() + (RowHeight - IconSize) / 2, IconSize, IconSize));
    const auto Text = TextRect(Option.rect, Primary);
    Painter->setFont(Font);
    Painter->setPen(EditorStyle::Color(Option.state & QStyle::State_Selected ? "--text-primary" : "--text-secondary"));
    if (Index.data(Qt::ForegroundRole).isValid())
    {
        Painter->setPen(qvariant_cast<QBrush>(Index.data(Qt::ForegroundRole)).color());
    }
    Painter->drawText(Text, Qt::AlignLeft | Qt::AlignVCenter,
                      QFontMetrics(Font).elidedText(Index.data().toString(), Qt::ElideRight, qMax(0, Text.width())));
    if (Primary)
    {
        // The frontend's --muted alias points to --text-secondary for this badge.
        MakeStudioIcon(StudioIcon::Star, "--text-secondary").paint(Painter,
            QRect(Option.rect.right() - Padding - BadgeSize + 1,
                  Option.rect.top() + (RowHeight - BadgeSize) / 2, BadgeSize, BadgeSize));
    }
    Painter->restore();
}

void HierarchyStyle::updateEditorGeometry(QWidget* Editor, const QStyleOptionViewItem& Option,
                                           const QModelIndex& Index) const
{
    Editor->setFont(ControlStyle::Font(Index.data(HierarchyCameraRole).toBool() ? 12 : 13));
    Editor->setGeometry(TextRect(Option.rect, Index.data(HierarchyPrimaryCameraRole).toBool()));
}
}
