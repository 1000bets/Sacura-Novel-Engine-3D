#include "UI/Styles/StoryGraphStyle.h"
#include "UI/Presentation/StorySpeakerLabel.h"
#include "UI/Styles/StoryEventStyle.h"
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QTextLayout>
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace EditorUi
{
namespace
{
// Shared measuring/painting prevents clipped dialogue and drifting choice ports.
qreal WrappedText(QPainter* Painter, const QString& Text, const QRectF& Rect, const QFont& Font, qreal LineHeight)
{
    qreal Y = 0;
    for (const auto& Paragraph : Text.split('\n'))
    {
        QTextLayout Layout(Paragraph, Font);
        QTextOption Options;
        Options.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        Layout.setTextOption(Options);
        Layout.beginLayout();
        int Lines = 0;
        while (true)
        {
            QTextLine Line = Layout.createLine();
            if (!Line.isValid()) { break; }
            Line.setLineWidth(Rect.width());
            Line.setPosition({0, Y + (LineHeight - Line.height()) / 2});
            Y += LineHeight;
            ++Lines;
        }
        Layout.endLayout();
        if (Painter) { Layout.draw(Painter, Rect.topLeft()); }
        if (Lines == 0) { Y += LineHeight; }
    }
    return Y;
}

QColor DialogueColor()
{
    const QColor Panel = EditorStyle::Color("--bg-panel");
    const QColor Accent = EditorStyle::Color("--accent");
    return QColor::fromRgbF(Panel.redF() * .92 + Accent.redF() * .08,
                           Panel.greenF() * .92 + Accent.greenF() * .08,
                           Panel.blueF() * .92 + Accent.blueF() * .08);
}

void Label(QPainter& Painter, const QRectF& Rect, const QString& Text, int Size, const char* Token,
           bool Strong = false, Qt::Alignment Alignment = Qt::AlignLeft | Qt::AlignVCenter)
{
    Painter.setFont(StoryGraphStyle::Font(Size, Strong));
    Painter.setPen(EditorStyle::Color(Token));
    Painter.drawText(Rect, Alignment, QFontMetricsF(Painter.font()).elidedText(Text, Qt::ElideRight, Rect.width()));
}

void Divider(QPainter& Painter, const QRectF& Rect)
{
    Painter.setPen(QPen(EditorStyle::Color("--border-subtle"), 1));
    Painter.drawLine(Rect.bottomLeft(), Rect.bottomRight());
}

void Phase(QPainter& Painter, const QRectF& Rect, const QString& Text)
{
    QFont Font = StoryGraphStyle::Font(10);
    Font.setLetterSpacing(QFont::AbsoluteSpacing, .6);
    Painter.setFont(Font);
    Painter.setPen(EditorStyle::Color("--text-secondary"));
    Painter.drawText(Rect.adjusted(12, 0, -12, 0), Qt::AlignVCenter, Text);
    Divider(Painter, Rect);
}

void HeaderGlyph(QPainter& Painter, QPointF Center, const StoryGraphNode& Node)
{
    Painter.save();
    Painter.translate(Center);
    Painter.setBrush(Qt::NoBrush);
    Painter.setPen(QPen(EditorStyle::Color("--text-secondary"), 1.05, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath Path;
    if (Node.bStart)
    {
        Path.moveTo(0, 7); Path.cubicTo(-8, -1, -5, -7, 0, -7); Path.cubicTo(5, -7, 8, -1, 0, 7);
        Painter.drawEllipse(QPointF(0, -2), 2, 2);
    }
    else if (Node.KindId == 3)
    {
        Path.moveTo(-5, 7); Path.lineTo(-5, -6); Path.lineTo(5, -6); Path.lineTo(3, -1); Path.lineTo(-5, -1);
    }
    else if (Node.KindId == 1)
    {
        Path.moveTo(-4, 6); Path.lineTo(-4, 1); Path.cubicTo(-4, -2, 4, 0, 4, -4);
        Path.moveTo(-4, 1); Path.lineTo(-4, -4);
        for (auto Point : {QPointF(-4, 6), QPointF(-4, -6), QPointF(4, -6)}) { Painter.drawEllipse(Point, 1.5, 1.5); }
    }
    else
    {
        Path.addRoundedRect(QRectF(-6, -5, 12, 9), 1, 1);
        Path.moveTo(-4, 4); Path.lineTo(-6, 7);
    }
    Painter.drawPath(Path);
    Painter.restore();
}
}

int StoryGraphStyle::OutputCount(const StoryGraphNode& Node)
{
    return Node.KindId == 3 ? 0 : Node.KindId == 1 ? Node.Choices.size() : 1;
}

StoryGraphStyle::CardLayout StoryGraphStyle::Layout(const StoryGraphNode& Node)
{
    CardLayout Result;
    qreal Y = 1;
    auto Row = [&](qreal Height)
    {
        const QRectF Rect(1, Y, NodeWidth - 2, Height);
        Y += Height;
        return Rect;
    };
    Result.Header = Row(HeaderHeight);
    Result.Execution = Row(ExecutionHeight);
    if (Node.bStart) { Result.Environment = Row(34); }
    auto EventPhase = [&](int Phase)
    {
        const auto Start = Y;
        Row(StoryEventStyle::PhaseHeader);
        for (const auto& Batch : Node.EventBatches)
        {
            if (Batch.Phase != Phase) { continue; }
            CardLayout::BatchRow Geometry;
            Geometry.Identity = Batch.Identity; Geometry.Phase = Phase; Geometry.bParallel = Batch.bParallel;
            Geometry.Order = Row(StoryEventStyle::BatchOrder).adjusted(20, 0, -12, 0);
            for (const auto& Binding : Batch.Bindings)
            {
                Y += StoryEventStyle::EventGap;
                Geometry.Events.push_back({Row(StoryEventStyle::EventHeight).adjusted(20, 0, -12, 0), Binding.Identity});
            }
            Y += 6;
            Result.Batches.push_back(std::move(Geometry));
        }
        return QRectF(1, Start, NodeWidth - 2, Y - Start);
    };
    Result.Before = EventPhase(0);
    if (Node.KindId == 2)
    {
        for (int Index = 0; Index < Node.Actions.size(); ++Index) { Result.Actions.push_back(Row(76)); }
    }
    const QString Text = Node.Text.isEmpty() ? QString::fromUtf8("Пустая реплика") : Node.Text;
    const qreal TextHeight = WrappedText(nullptr, Text, QRectF(0, 0, NodeWidth - 30, 0), Font(14), 22.4);
    const qreal SpeakerHeight = std::ceil(QFontMetricsF(Font(12, true)).height());
    if (Node.KindId != 2)
    {
        Result.Dialogue = Row(32 + SpeakerHeight + 8 + TextHeight);
        Result.Speaker = QRectF(15, Result.Dialogue.top() + 16, NodeWidth - 30, SpeakerHeight);
        Result.Text = QRectF(15, Result.Speaker.bottom() + 8, NodeWidth - 30, TextHeight);
        Result.During = EventPhase(1);
    }
    Result.Input = Row(33);
    if (Node.KindId != 2) { Result.After = EventPhase(2); }
    if (Node.KindId == 1)
    {
        Y += 10;
        for (const auto& Choice : Node.Choices)
        {
            const qreal Height = WrappedText(nullptr, Choice.Label, QRectF(0, 0, NodeWidth - 62, 0), Font(13), 19.5);
            Result.Choices.push_back(QRectF(11, Y, NodeWidth - 22, 24 + 6 + std::max(36.0, Height + 16) + 6 + 24 + 16));
            Y += Result.Choices.back().height() + 8;
        }
        Y += 2;
    }
    Result.Height = Y + 1;
    return Result;
}

qreal StoryGraphStyle::Height(const StoryGraphNode& Node) { return Layout(Node).Height; }

QPointF StoryGraphStyle::OutputAnchor(const StoryGraphNode& Node, int Port, const CardLayout* Geometry)
{
    if (Node.KindId == 1)
    {
        if (!Geometry)
        {
            const auto Measured = Layout(Node);
            return OutputAnchor(Node, Port, &Measured);
        }
        if (Port >= 0 && Port < Geometry->Choices.size())
        {
            return {NodeWidth + 4, Geometry->Choices[Port].bottom() - 20};
        }
    }
    return {NodeWidth + 4, InputAnchor().y()};
}

void StoryGraphStyle::PaintPort(QPainter& Painter, bool bOutput, bool bHovered)
{
    Q_UNUSED(bOutput);
    Painter.setRenderHint(QPainter::Antialiasing);
    if (bHovered)
    {
        QColor Halo = EditorStyle::Color("--accent");
        Halo.setAlphaF(.25);
        Painter.setPen(QPen(Halo, 10));
        Painter.setBrush(Qt::NoBrush);
        Painter.drawRoundedRect(QRectF(-8, -8, 16, 16), 3, 3);
    }
    Painter.setPen(QPen(EditorStyle::Color(bHovered ? "--accent" : "--text-secondary"), 2));
    Painter.setBrush(EditorStyle::Color("--bg-panel"));
    Painter.drawRoundedRect(QRectF(-7, -7, 14, 14), 2, 2);
    Painter.setPen(Qt::NoPen);
    Painter.setBrush(EditorStyle::Color("--text-secondary"));
    QPainterPath Arrow;
    Arrow.moveTo(-3, -3); Arrow.lineTo(3, 0); Arrow.lineTo(-3, 3); Arrow.closeSubpath();
    Painter.drawPath(Arrow);
}

QPen StoryGraphStyle::ConnectionPen(bool bSelected)
{
    QPen Pen(EditorStyle::Color(bSelected ? "--accent" : "--text-secondary"), bSelected ? 3 : 2,
             Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    Pen.setCosmetic(true);
    return Pen;
}

QPainterPath StoryGraphStyle::ConnectionPath(QPointF Start, QPointF Finish)
{
    QPainterPath Path(Start);
    const qreal Dx = Finish.x() - Start.x();
    const qreal Dy = Finish.y() - Start.y();
    // A continuous family of curves: crossing a routing threshold while dragging
    // must not replace a Bezier by a completely different orthogonal path.
    const qreal Reach = std::max(36.0, std::abs(Dx) * .5);
    const qreal Backward = std::clamp(-Dx / 140.0, 0.0, 1.0);
    const qreal Lift = -72.0 * Backward * std::exp(-Dy * Dy / (144.0 * 144.0));
    const QPointF Middle((Start.x() + Finish.x()) / 2, (Start.y() + Finish.y()) / 2 + Lift);
    // Split the ordinary cubic at t=.5; the same controls also form a soft loop
    // for a return connection whose endpoints share a row.
    const QPointF A = Start + QPointF(Reach, 0), B = Finish - QPointF(Reach, 0);
    const QPointF Tangent = (B - A + Finish - Start) / 8;
    Path.cubicTo((Start + A) / 2, Middle - Tangent, Middle);
    Path.cubicTo(Middle + Tangent, (B + Finish) / 2, Finish);
    return Path;
}

void StoryGraphStyle::PaintConnection(QPainter& Painter, const QPainterPath& Path, bool bSelected)
{
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.setBrush(Qt::NoBrush);
    QPen Halo = ConnectionPen(bSelected);
    Halo.setColor(EditorStyle::Color("--bg-app"));
    Halo.setWidthF(Halo.widthF() + 5);
    Painter.setPen(Halo);
    Painter.drawPath(Path);
    Painter.setPen(ConnectionPen(bSelected));
    Painter.drawPath(Path);
    const QPointF End = Path.pointAtPercent(1);
    const qreal Scale = std::max(.01, std::hypot(Painter.worldTransform().m11(), Painter.worldTransform().m12()));
    Painter.drawLine(End - QPointF(8, 4) / Scale, End);
    Painter.drawLine(End - QPointF(8, -4) / Scale, End);
}

void StoryGraphStyle::PaintCard(QPainter& Painter, const StoryGraphNode& Node, const QString& Title, bool bSelected,
                               const CardLayout* Geometry)
{
    if (!Geometry)
    {
        const auto Measured = Layout(Node);
        PaintCard(Painter, Node, Title, bSelected, &Measured);
        return;
    }
    const auto& G = *Geometry;
    const QRectF Bounds(.5, .5, NodeWidth - 1, G.Height - 1);
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.setBrush(EditorStyle::Color("--bg-panel"));
    Painter.setPen(QPen(EditorStyle::Color(bSelected ? "--accent" : "--border-subtle"), bSelected ? 2 : 1));
    Painter.drawRoundedRect(Bounds, 7, 7);
    Painter.save();
    QPainterPath Clip;
    Clip.addRoundedRect(Bounds, 7, 7);
    Painter.setClipPath(Clip);
    Painter.fillRect(G.Header, EditorStyle::Color("--border-subtle"));
    HeaderGlyph(Painter, QPointF(20.5, G.Header.center().y()), Node);
    const qreal KindWidth = QFontMetricsF(Font(10)).horizontalAdvance(Node.Kind);
    Label(Painter, QRectF(36, 1, NodeWidth - 60 - KindWidth, HeaderHeight), Title, 12, "--text-secondary", true);
    Label(Painter, QRectF(NodeWidth - 13 - KindWidth, 1, KindWidth, HeaderHeight), Node.Kind, 10, "--text-secondary");
    if (Node.KindId == 0 || Node.KindId == 2)
    {
        Label(Painter, G.Execution.adjusted(14, 0, -14, 0), QString::fromUtf8("Дальше"), 12,
              Node.Targets.value(0).isEmpty() ? "--warning" : "--text-secondary", false, Qt::AlignRight | Qt::AlignVCenter);
    }
    Divider(Painter, G.Execution);
    if (!G.Environment.isNull())
    {
        Painter.setPen(QPen(EditorStyle::Color("--accent"), 1));
        Painter.setBrush(Qt::NoBrush);
        Painter.drawEllipse(QPointF(19, G.Environment.center().y() - 2), 3, 3);
        Painter.drawArc(QRectF(14, G.Environment.center().y(), 11, 5), 0, 180 * 16);
        Label(Painter, G.Environment.adjusted(32, 0, -12, 0), QString::fromUtf8("Начало истории"), 11, "--accent");
        Divider(Painter, G.Environment);
    }
    auto DrawPhase = [&](const QRectF& Rect, int Index)
    {
        Phase(Painter, QRectF(Rect.left(), Rect.top(), Rect.width(), StoryEventStyle::PhaseHeader), StoryEventStyle::PhaseHeaders()[Index]);
        if (Node.KindId == 0 || Node.KindId == 1)
        { Label(Painter, QRectF(NodeWidth - 32, Rect.top() + 8, 20, 24), QStringLiteral("+"), 16, "--text-muted", false, Qt::AlignCenter); }
        Divider(Painter, Rect);
    };
    if (Node.KindId == 2) { Phase(Painter, G.Before, QString::fromUtf8("ДЕЙСТВИЯ")); }
    else { DrawPhase(G.Before, 0); }
    const QStringList Names = {QString::fromUtf8("Ожидание"), QString::fromUtf8("Перемещение"),
                              QString::fromUtf8("Смена камеры"), QString::fromUtf8("Движение по маршруту")};
    for (int Index = 0; Index < G.Actions.size(); ++Index)
    {
        const auto Rect = G.Actions[Index].adjusted(12, 5, -12, -5);
        Painter.fillRect(Rect, EditorStyle::Color("--bg-app"));
        Label(Painter, Rect.adjusted(8, 0, -8, -24), Names.value(Node.Actions[Index].Kind), 12, "--text-secondary", true);
        Label(Painter, QRectF(Rect.left() + 8, Rect.bottom() - 28, Rect.width() - 16, 20),
              QString::fromUtf8("%1 с").arg(Node.Actions[Index].Duration), 10, "--text-secondary");
    }
    if (!G.Dialogue.isNull())
    {
        Painter.fillRect(G.Dialogue, DialogueColor());
        QFont SpeakerFont = Font(12);
        SpeakerFont.setWeight(QFont::Bold);
        Painter.setFont(SpeakerFont);
        Painter.setPen(EditorStyle::Color("--accent"));
        const QString Speaker = IsNarrator(Node.Speaker) ? (Node.KindId == 1 ? QString::fromUtf8("Выбор игрока")
                                                                         : QString::fromUtf8("Рассказчик")) : Node.Speaker;
        Painter.drawText(G.Speaker, Qt::AlignVCenter,
                         QFontMetricsF(SpeakerFont).elidedText(Speaker, Qt::ElideRight, G.Speaker.width()));
        Painter.setPen(EditorStyle::Color("--text-secondary"));
        WrappedText(&Painter, Node.Text.isEmpty() ? QString::fromUtf8("Пустая реплика") : Node.Text, G.Text, Font(14), 22.4);
        DrawPhase(G.During, 1);
    }
    const QString InputText = Node.KindId == 3 ? QString::fromUtf8("Завершение истории")
                             : Node.KindId == 1 ? QString::fromUtf8("Игрок выбирает ответ")
                             : Node.KindId == 2 ? QString::fromUtf8("После выполнения действий")
                                               : QString::fromUtf8("Игрок нажимает «Дальше»");
    Painter.setPen(QPen(EditorStyle::Color("--text-secondary"), 1));
    Painter.setBrush(Qt::NoBrush);
    QPainterPath Pointer;
    Pointer.moveTo(13, G.Input.center().y() - 5);
    Pointer.lineTo(23, G.Input.center().y() - 1);
    Pointer.lineTo(19, G.Input.center().y() + 1);
    Pointer.lineTo(17, G.Input.center().y() + 5);
    Pointer.closeSubpath();
    Painter.drawPath(Pointer);
    Label(Painter, G.Input.adjusted(30, 0, -12, 0), InputText, 11, "--text-secondary");
    Divider(Painter, G.Input);
    if (!G.After.isNull()) { DrawPhase(G.After, 2); }
    for (const auto& BatchGeometry : G.Batches)
    {
        Painter.save();
        Painter.translate(BatchGeometry.Order.left() + 10, BatchGeometry.Order.center().y());
        Painter.setBrush(Qt::NoBrush); Painter.setPen(QPen(EditorStyle::Color("--text-muted"), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPainterPath OrderIcon;
        if (BatchGeometry.bParallel)
        {
            OrderIcon.moveTo(-3, -4); OrderIcon.lineTo(-3, 0); OrderIcon.quadTo(-3, 2, 0, 2);
            OrderIcon.moveTo(3, -4); OrderIcon.lineTo(3, 0); OrderIcon.quadTo(3, 2, 0, 2); OrderIcon.lineTo(0, 5);
        }
        else { OrderIcon.moveTo(0, -5); OrderIcon.lineTo(0, 5); OrderIcon.moveTo(-3, 2); OrderIcon.lineTo(0, 5); OrderIcon.lineTo(3, 2); }
        Painter.drawPath(OrderIcon); Painter.restore();
        Label(Painter, BatchGeometry.Order.adjusted(22, 0, -30, 0),
              BatchGeometry.bParallel ? QString::fromUtf8("Параллельно  ▾") : QString::fromUtf8("По очереди  ▾"), 10, "--text-muted");
        Label(Painter, BatchGeometry.Order.adjusted(BatchGeometry.Order.width() - 24, 0, 0, 0), QStringLiteral("+"), 14, "--text-muted", false, Qt::AlignCenter);
        const auto Batch = std::find_if(Node.EventBatches.begin(), Node.EventBatches.end(),
            [&](const auto& Value) { return Value.Identity == BatchGeometry.Identity; });
        if (Batch == Node.EventBatches.end()) { continue; }
        for (qsizetype Index = 0; Index < BatchGeometry.Events.size(); ++Index)
        {
            const auto& EventGeometry = BatchGeometry.Events[Index];
            const auto& Binding = Batch->Bindings[Index];
            const auto& Rect = EventGeometry.Rect;
            Painter.setBrush(EditorStyle::Color("--bg-app")); Painter.setPen(QPen(EditorStyle::Color(Binding.bSelected ? "--accent" : "--border-subtle"), Binding.bSelected ? 2 : 1));
            Painter.drawRoundedRect(Rect, 4, 4);
            Painter.drawLine(QPointF(Rect.left() - 8, BatchGeometry.Order.top()), QPointF(Rect.left() - 8, Rect.bottom()));
            Painter.save();
            Painter.translate(Rect.left() + 14, Rect.top() + 16); Painter.setBrush(Qt::NoBrush);
            Painter.setPen(QPen(EditorStyle::Color("--text-muted"), 1, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            QPainterPath Layers; Layers.moveTo(-5, -2); Layers.lineTo(0, -5); Layers.lineTo(5, -2); Layers.lineTo(0, 1); Layers.closeSubpath();
            Layers.moveTo(-5, 1); Layers.lineTo(0, 4); Layers.lineTo(5, 1); Painter.drawPath(Layers);
            Painter.translate(Rect.width() - 29, 0);
            QPainterPath Trash; Trash.addRoundedRect(QRectF(-3, -3, 6, 8), 1, 1);
            Trash.moveTo(-5, -5); Trash.lineTo(5, -5); Trash.moveTo(-1.5, -7); Trash.lineTo(1.5, -7); Painter.drawPath(Trash);
            Painter.restore();
            Label(Painter, QRectF(Rect.left() + 26, Rect.top() + 5, Rect.width() - 54, 22), Binding.Name, 12, "--text-secondary", true);
            Label(Painter, QRectF(Rect.left() + 8, Rect.top() + 30, Rect.width() - 16, 18), Binding.Summary, 10, "--text-muted");
            Label(Painter, QRectF(Rect.left() + 8, Rect.top() + 53, Rect.width() - 16, 18), StoryEventStyle::Joins().value(Binding.Join), 10, "--text-muted");
        }
    }
    for (int Index = 0; Index < G.Choices.size(); ++Index)
    {
        const auto& Rect = G.Choices[Index];
        Painter.setPen(QPen(EditorStyle::Color("--border-subtle"), 1));
        Painter.setBrush(EditorStyle::Color("--bg-panel"));
        Painter.drawRoundedRect(Rect, 8, 8);
        Label(Painter, QRectF(Rect.x() + 8, Rect.y() + 8, Rect.width() - 16, 24),
              QString::fromUtf8("Ответ %1").arg(Index + 1), 12, "--text-muted", true);
        const QRectF TextRect(Rect.x() + 8, Rect.y() + 38, Rect.width() - 16, Rect.height() - 78);
        Painter.fillRect(TextRect, EditorStyle::Color("--bg-input"));
        Painter.setPen(EditorStyle::Color("--text-primary"));
        WrappedText(&Painter, Node.Choices[Index].Label, TextRect.adjusted(8, 8, -8, -8), Font(13), 19.5);
        Label(Painter, QRectF(Rect.x() + 8, Rect.bottom() - 32, Rect.width() - 16, 24),
              QString::fromUtf8("Выход ответа"), 10, "--text-secondary");
    }
    Painter.restore();
    Painter.setBrush(Qt::NoBrush);
    Painter.setPen(QPen(EditorStyle::Color(bSelected ? "--accent" : "--border-subtle"), bSelected ? 2 : 1));
    Painter.drawRoundedRect(Bounds, 7, 7);
}

void StoryGraphStyle::PaintGrid(QPainter& Painter, const QRectF& Rect)
{
    Painter.fillRect(Rect, EditorStyle::Color("--bg-app"));
    Painter.setPen(Qt::NoPen);
    Painter.setBrush(EditorStyle::Color("--graph-grid"));
    const qreal Left = std::floor(Rect.left() / GridSpacing) * GridSpacing;
    const qreal Top = std::floor(Rect.top() / GridSpacing) * GridSpacing;
    for (qreal X = Left; X < Rect.right(); X += GridSpacing)
    {
        for (qreal Y = Top; Y < Rect.bottom(); Y += GridSpacing) { Painter.drawEllipse(QPointF(X, Y), .7, .7); }
    }
}
} // namespace EditorUi
