#include "UI/Styles/ActionMenuStyle.h"
#include "UI/Styles/ControlStyle.h"
#include "UI/Styles/EditorStyle.h"
#include "UI/Styles/StudioIcons.h"
#include <QFontMetrics>
#include <QPainter>
#include <algorithm>

namespace EditorUi
{
namespace
{
QRect TextRect(const QRect& Rect, bool Submenu, const QString& Shortcut)
{
    // 7px padding + 16px icon + 9px gap, matching editorContextMenu.css.
    return Rect.adjusted(32, 7, Submenu ? -30 : (Shortcut.isEmpty() ? -7 : -48), -7);
}
}
QSize ActionMenuStyle::RowSize(const QString& Text, int Width, bool Submenu, const QString& Shortcut)
{
    const QFontMetrics Metrics(ControlStyle::Font());
    const int TextWidth = std::max(1, TextRect(QRect(0, 0, Width, 100), Submenu, Shortcut).width());
    const int Height = Metrics.boundingRect(QRect(0, 0, TextWidth, 10000), Qt::TextWordWrap, Text).height();
    return {Width, std::max(32, Height + 14)};
}
void ActionMenuStyle::PaintRow(QPainter& Painter, const QRect& Rect, const QString& Text,
                               const QIcon& Icon, bool Submenu, bool Active, bool Enabled, const QString& Shortcut)
{
    Painter.setRenderHint(QPainter::Antialiasing);
    if (Active && Enabled)
    {
        Painter.setPen(Qt::NoPen);
        Painter.setBrush(EditorStyle::Color("--bg-hover"));
        Painter.drawRoundedRect(Rect, 3, 3);
    }
    const auto Mode = Enabled ? QIcon::Normal : QIcon::Disabled;
    Icon.paint(&Painter, QRect(Rect.left() + 7, Rect.center().y() - 8, 16, 16), Qt::AlignCenter, Mode);
    Painter.setFont(ControlStyle::Font());
    Painter.setPen(EditorStyle::Color(Enabled ? "--text-primary" : "--text-disabled"));
    Painter.drawText(TextRect(Rect, Submenu, Shortcut), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, Text);
    if (!Shortcut.isEmpty() && !Submenu)
    {
        Painter.setPen(EditorStyle::Color(Enabled ? "--text-secondary" : "--text-disabled"));
        Painter.drawText(Rect.adjusted(0, 0, -7, 0), Qt::AlignRight | Qt::AlignVCenter, Shortcut);
    }
    if (Submenu)
    {
        MakeStudioIcon(StudioIcon::ChevronRight, "--text-primary")
            .paint(&Painter, QRect(Rect.right() - 21, Rect.center().y() - 7, 14, 14), Qt::AlignCenter, Mode);
    }
}
}
