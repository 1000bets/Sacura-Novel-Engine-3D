#pragma once
#include "UI/Styles/EditorStyle.h"
#include "UI/Styles/ControlStyle.h"
#include <QFont>
#include <QPen>
#include <QRectF>
#include "UI/Presentation/StoryGraphModel.h"
class QPainter;
class QRectF;
class QPainterPath;

namespace EditorUi
{
struct StoryGraphStyle
{
    // StoryFlow.jsx / storyFlow.css, in CSS (Qt logical) pixels, before zoom.
    static constexpr qreal NodeWidth = 360;
    static constexpr qreal NodeHeight = 310;
    static constexpr qreal ColumnSpacing = NodeWidth + 150;
    static constexpr qreal RowSpacing = 530;
    static constexpr qreal HeaderHeight = 39;
    static constexpr qreal ExecutionHeight = 30;
    static constexpr qreal PortSize = 16;
    static constexpr int GridSpacing = 28;
    struct CardLayout
    {
        struct EventRow { QRectF Rect; QString Binding; };
        struct BatchRow { QRectF Order; QString Identity; int Phase = 1; bool bParallel = false; QVector<EventRow> Events; };
        QRectF Header, Execution, Environment, Before, Dialogue, Speaker, Text, During, Input, After;
        QVector<QRectF> Actions, Choices, DataInputs, DataOutputs;
        QVector<BatchRow> Batches;
        qreal Height = 0;
    };
    static CardLayout Layout(const StoryGraphNode& Node);
    static void PaintCard(QPainter& Painter, const StoryGraphNode& Node, const QString& Title, bool bSelected,
                          const CardLayout* Geometry = nullptr);
    static void PaintGrid(QPainter& Painter, const QRectF& Rect);
    static int OutputCount(const StoryGraphNode& Node);
    static qreal Height(const StoryGraphNode& Node);
    static QPointF InputAnchor() { return {-4, 1 + HeaderHeight + ExecutionHeight / 2}; }
    static QPointF OutputAnchor(const StoryGraphNode& Node, int Port, const CardLayout* Geometry = nullptr);
    static void PaintPort(QPainter& Painter, bool bOutput, bool bHovered = false);
    static QPen ConnectionPen(bool bSelected);
    static QPainterPath ConnectionPath(QPointF Start, QPointF Finish);
    static void PaintConnection(QPainter& Painter, const QPainterPath& Path, bool bSelected);
    static QFont Font(int Size = 11, bool bStrong = false)
    {
        return ControlStyle::Font(Size, bStrong);
    }
};
}
