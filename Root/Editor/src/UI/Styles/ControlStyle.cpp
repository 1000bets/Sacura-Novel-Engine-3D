#include "UI/Styles/ControlStyle.h"
#include "UI/Styles/EditorStyle.h"
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>

namespace EditorUi
{
QFont ControlStyle::Font(int Pixels, bool Strong)
{
    // Qt stylesheet family lists are not a browser font fallback chain. Resolve one real face.
    static const QString Family = []() {
        const auto Families = QFontDatabase::families();
        for (const auto& Candidate : {QStringLiteral("Inter"), QStringLiteral("Segoe UI")})
        {
            if (Families.contains(Candidate, Qt::CaseInsensitive)) { return Candidate; }
        }
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    }();
    QFont Result(Family);
    Result.setStyleHint(QFont::SansSerif);
    Result.setPixelSize(Pixels);
    Result.setWeight(Strong ? QFont::DemiBold : QFont::Normal);
    return Result;
}

void ControlStyle::PopupSurface(QPainter& Painter, const QRect& Bounds)
{
    Painter.setRenderHint(QPainter::Antialiasing);
    const QRectF Panel = QRectF(Bounds).adjusted(4.5, 4.5, -4.5, -4.5);
    Painter.setPen(Qt::NoPen);
    for (int Spread = 4; Spread > 0; --Spread)
    {
        Painter.setBrush(QColor(0, 0, 0, 10));
        Painter.drawRoundedRect(Panel.adjusted(-Spread, -Spread + 1, Spread, Spread + 1), 6 + Spread, 6 + Spread);
    }
    Painter.setBrush(EditorStyle::Color("--bg-panel-elevated"));
    Painter.setPen(QPen(EditorStyle::Color("--border-subtle"), 1));
    Painter.drawRoundedRect(Panel, 6, 6);
}

void ControlStyle::Check(QPainter& Painter, const QRectF& Box, qreal Progress, bool Enabled, bool Focused)
{
    Painter.save();
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.setOpacity(Enabled ? 1 : .4);
    Painter.setPen(QPen(EditorStyle::Color(Focused ? "--accent" : "--border-strong"), 1));
    Painter.setBrush(EditorStyle::Color("--bg-input"));
    Painter.drawRoundedRect(Box, 3, 3);
    QColor Fill = EditorStyle::Color("--accent");
    Fill.setAlphaF(Progress);
    Painter.setPen(Qt::NoPen);
    Painter.setBrush(Fill);
    Painter.drawRoundedRect(Box, 3, 3);
    QPainterPath Tick;
    Tick.moveTo(Box.left() + 3, Box.top() + 7);
    Tick.lineTo(Box.left() + 6, Box.top() + 10);
    Tick.lineTo(Box.left() + 11, Box.top() + 4);
    // Reveal along the stroke, rather than changing an OS checkbox bitmap.
    QPainterPath Visible;
    Visible.moveTo(Tick.pointAtPercent(0));
    for (int I = 1; I <= 30; ++I) { Visible.lineTo(Tick.pointAtPercent(Progress * I / 30)); }
    Painter.setPen(QPen(EditorStyle::Color("--bg-panel"), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    Painter.setBrush(Qt::NoBrush);
    if (Progress > 0) { Painter.drawPath(Visible); }
    Painter.restore();
}

void ControlStyle::Option(QPainter& Painter, const QRect& Rect, const QString& Text, bool Selected,
                          bool Active, bool Enabled, const QFont& Font)
{
    Painter.save();
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.setOpacity(Enabled ? 1 : .4);
    if (Active || Selected)
    {
        Painter.setPen(Qt::NoPen);
        Painter.setBrush(EditorStyle::Color("--bg-hover"));
        Painter.drawRoundedRect(Rect.adjusted(0, 1, 0, -1), 4, 4);
    }
    Painter.setFont(Font);
    Painter.setPen(EditorStyle::Color("--text-primary"));
    const QRect TextRect = Rect.adjusted(10, 0, -30, 0);
    Painter.drawText(TextRect, Qt::AlignVCenter | Qt::AlignLeft,
                     QFontMetrics(Font).elidedText(Text, Qt::ElideRight, TextRect.width()));
    if (Selected)
    {
        const QPointF Origin(Rect.right() - 21, Rect.center().y() - 5);
        QPainterPath Tick;
        Tick.moveTo(Origin + QPointF(0, 5));
        Tick.lineTo(Origin + QPointF(3, 8));
        Tick.lineTo(Origin + QPointF(10, 1));
        Painter.setPen(QPen(EditorStyle::Color("--text-primary"), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        Painter.drawPath(Tick);
    }
    Painter.restore();
}
}
