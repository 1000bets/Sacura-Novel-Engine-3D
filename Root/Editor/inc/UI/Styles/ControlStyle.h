#pragma once

#include <QFont>
#include <QRect>

class QPainter;
namespace EditorUi
{
// Native presentation of editorTheme.css and EditorSelect. All dimensions are logical pixels.
struct ControlStyle
{
    static QFont Font(int Pixels = 12, bool Strong = false);
    static void Check(QPainter& Painter, const QRectF& Box, qreal Progress, bool Enabled, bool Focused);
    static void Option(QPainter& Painter, const QRect& Rect, const QString& Text, bool Selected,
                       bool Active, bool Enabled, const QFont& Font);
    static constexpr int MotionDuration = 150;
    static constexpr int OptionHeight = 36;
};
}
