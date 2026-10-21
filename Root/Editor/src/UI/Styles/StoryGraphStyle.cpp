#include "UI/Styles/StoryGraphStyle.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace EditorUi
{
int StoryGraphStyle::OutputCount(const StoryGraphNode& Node)
{
    return Node.KindId == 3 ? 0 : Node.KindId == 1 ? Node.Choices.size() : 1;
}
qreal StoryGraphStyle::Height(const StoryGraphNode& Node)
{
    if (Node.KindId == 3)
    {
        return 120;
    }
    return std::max(NodeHeight, Node.KindId == 1 ? 158.0 + Node.Choices.size() * 30 : 100.0 + Node.Actions.size() * 38);
}
QPointF StoryGraphStyle::OutputAnchor(const StoryGraphNode& Node, int Port)
{
    return {NodeWidth - 4, Node.KindId == 1 ? 136.0 + Port * 30 : 47.0};
}
void StoryGraphStyle::PaintPort(QPainter& Painter, bool bOutput)
{
    Painter.setPen(QPen(EditorStyle::Color("--graph-connection"), 1.3));
    Painter.setBrush(EditorStyle::Color("--bg-panel"));
    Painter.drawRoundedRect(QRectF(-4, -5, 8, 10), 1, 1);
    if (!bOutput)
    {
        Painter.drawLine(QPointF(-1, -2), QPointF(1, 0));
        Painter.drawLine(QPointF(1, 0), QPointF(-1, 2));
    }
}
QPen StoryGraphStyle::ConnectionPen(bool bSelected)
{
    return QPen(EditorStyle::Color(bSelected ? "--accent" : "--graph-connection"), bSelected ? 2 : 1.5);
}
void StoryGraphStyle::PaintConnection(QPainter& Painter, const QPainterPath& Path, bool bSelected)
{
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.setPen(ConnectionPen(bSelected));
    Painter.drawPath(Path);
    const QPointF End = Path.pointAtPercent(1);
    Painter.drawLine(End - QPointF(6, 3), End);
    Painter.drawLine(End - QPointF(6, -3), End);
}
void StoryGraphStyle::PaintCard(QPainter& Painter, const StoryGraphNode& Node, const QString& Title, bool bSelected)
{
    const QRectF Bounds(0, 0, NodeWidth, Height(Node));
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.setPen(Qt::NoPen);
    Painter.setBrush(EditorStyle::Color("--graph-node"));
    Painter.drawRoundedRect(Bounds, 3, 3);
    Painter.fillRect(QRectF(0, 34, Bounds.width(), 28), EditorStyle::Color("--bg-panel"));
    if (bSelected)
    {
        Painter.fillRect(QRectF(0, 0, 2, Bounds.height()), EditorStyle::Color("--graph-node-selected"));
    }
    Painter.setFont(Font(11, true));
    Painter.setPen(EditorStyle::Color("--text-secondary"));
    Painter.drawText(QRectF(12, 0, 180, 34), Qt::AlignVCenter, Title);
    Painter.setFont(Font(10));
    Painter.setPen(EditorStyle::Color("--text-muted"));
    Painter.drawText(QRectF(192, 0, 80, 34), Qt::AlignRight | Qt::AlignVCenter, Node.Kind);
    if (Node.bStart)
    {
        Painter.setPen(EditorStyle::Color("--accent"));
        Painter.drawText(QRectF(12, 34, 140, 28), Qt::AlignVCenter, QString::fromUtf8("Начало"));
    }
    if (Node.KindId == 0 || Node.KindId == 2)
    {
        Painter.setPen(EditorStyle::Color("--text-muted"));
        Painter.drawText(QRectF(160, 34, 110, 28), Qt::AlignRight | Qt::AlignVCenter, QString::fromUtf8("Дальше"));
    }
    if (Node.KindId == 0)
    {
        Painter.fillRect(QRectF(0, 62, Bounds.width(), 26), EditorStyle::Color("--bg-panel"));
        Painter.fillRect(QRectF(0, 178, Bounds.width(), 68), EditorStyle::Color("--bg-panel"));
        Painter.setPen(EditorStyle::Color("--text-muted"));
        Painter.drawText(QRectF(12, 62, 260, 26), Qt::AlignVCenter, QString::fromUtf8("ДО ТЕКСТА"));
        Painter.drawText(QRectF(12, 178, 260, 28), Qt::AlignVCenter, QString::fromUtf8("ВМЕСТЕ С ТЕКСТОМ"));
        Painter.drawText(QRectF(12, 218, 260, 28), Qt::AlignVCenter, QString::fromUtf8("ПОСЛЕ ОТВЕТА ИГРОКА"));
        Painter.setPen(EditorStyle::Color("--accent"));
        Painter.setFont(Font(11, true));
        Painter.drawText(QRectF(12, 96, 260, 20), Qt::AlignVCenter, Node.Speaker);
        Painter.setPen(EditorStyle::Color("--text-secondary"));
        Painter.setFont(Font(14));
        Painter.save();
        Painter.setClipRect(QRectF(12, 120, 260, 54));
        Painter.drawText(QRectF(12, 120, 260, 100), Qt::TextWordWrap, Node.Text);
        Painter.restore();
    }
    else if (Node.KindId == 1)
    {
        Painter.setFont(Font(13));
        Painter.setPen(EditorStyle::Color("--text-secondary"));
        Painter.drawText(QRectF(12, 70, 260, 48), Qt::TextWordWrap, Node.Text);
        for (int Index = 0; Index < Node.Choices.size(); ++Index)
        {
            Painter.drawText(QRectF(12, 122 + Index * 30, 250, 28), Qt::AlignVCenter, Node.Choices[Index].Label);
        }
    }
    else if (Node.KindId == 2)
    {
        const QStringList Names = {QString::fromUtf8("Ожидание"), QString::fromUtf8("Перемещение"),
                                   QString::fromUtf8("Смена камеры"), QString::fromUtf8("Движение по маршруту")};
        Painter.setFont(Font(12));
        Painter.setPen(EditorStyle::Color("--text-secondary"));
        for (int Index = 0; Index < Node.Actions.size(); ++Index)
        {
            const auto& Action = Node.Actions[Index];
            Painter.drawText(QRectF(12, 70 + Index * 38, 260, 30), Qt::AlignVCenter,
                             Names.value(Action.Kind) + QStringLiteral(" · %1 s").arg(Action.Duration));
        }
    }
    else
    {
        Painter.setFont(Font(12));
        Painter.drawText(QRectF(12, 65, 260, 30), Qt::AlignVCenter, QString::fromUtf8("Завершить историю"));
    }
}

void StoryGraphStyle::PaintGrid(QPainter& Painter, const QRectF& Rect)
{
    Painter.fillRect(Rect, EditorStyle::Color("--bg-app"));
    Painter.setPen(QPen(EditorStyle::Color("--graph-grid"), 1));
    const int Step = StoryGraphStyle::GridSpacing;
    const qreal Left = std::floor(Rect.left() / Step) * Step;
    const qreal Top = std::floor(Rect.top() / Step) * Step;
    for (qreal X = Left; X < Rect.right(); X += Step)
    {
        for (qreal Y = Top; Y < Rect.bottom(); Y += Step)
        {
            Painter.drawPoint(QPointF(X, Y));
        }
    }
}
} // namespace EditorUi
