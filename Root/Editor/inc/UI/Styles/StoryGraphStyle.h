#pragma once
#include "UI/Styles/EditorStyle.h"
#include <QFont>
#include <QPen>
#include "UI/Presentation/StoryGraphModel.h"
class QPainter;
class QRectF;
class QPainterPath;

namespace EditorUi
{
struct StoryGraphStyle
{
    static constexpr qreal NodeWidth = 284;
    static constexpr qreal NodeHeight = 246;
    static constexpr qreal ColumnSpacing = 394;
    static constexpr qreal RowSpacing = 292;
    static constexpr int GridSpacing = 24;
    static void PaintCard(QPainter& Painter, const StoryGraphNode& Node, const QString& Title, bool bSelected);
    static void PaintGrid(QPainter& Painter, const QRectF& Rect);
    static int OutputCount(const StoryGraphNode& Node);
    static qreal Height(const StoryGraphNode& Node);
    static QPointF InputAnchor() { return {4, 47}; }
    static QPointF OutputAnchor(const StoryGraphNode& Node, int Port);
    static void PaintPort(QPainter& Painter, bool bOutput);
    static QPen ConnectionPen(bool bSelected);
    static void PaintConnection(QPainter& Painter, const QPainterPath& Path, bool bSelected);
    static QFont Font(int Size = 11, bool bStrong = false)
    {
        QFont Result;
        Result.setFamilies({QStringLiteral("Inter"), QStringLiteral("Segoe UI")});
        Result.setPixelSize(Size);
        Result.setWeight(bStrong ? QFont::Medium : QFont::Normal);
        return Result;
    }
};
}
