#pragma once
#include <QSize>
#include <QString>
class QPainter;
class QIcon;
class QRect;
namespace EditorUi
{
struct ActionMenuStyle
{
    static constexpr int Width = 280;
    static constexpr int SubmenuWidth = 240;
    static constexpr int Inset = 9;
    static QSize RowSize(const QString& Text, int Width, bool Submenu, const QString& Shortcut = {});
    static void PaintRow(QPainter& Painter, const QRect& Rect, const QString& Text,
                         const QIcon& Icon, bool Submenu, bool Active, bool Enabled, const QString& Shortcut = {});
};
}
