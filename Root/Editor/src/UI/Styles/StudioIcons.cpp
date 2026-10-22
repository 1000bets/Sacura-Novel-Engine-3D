#include "UI/Styles/StudioIcons.h"
#include "UI/Styles/EditorStyle.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QIconEngine>
#include <algorithm>
#include <utility>

namespace
{
// Vector geometry from the frontend's lucide-react 0.468.0 Flower2 icon.
// ISC attribution: resources/ui/icons/LUCIDE-LICENSE. Draw at the destination
// scale instead of caching a small bitmap, including after a screen/DPI change.
class FlowerIconEngine final : public QIconEngine
{
public:
    explicit FlowerIconEngine(QColor InColor) : Color(std::move(InColor)) {}
    QIconEngine* clone() const override { return new FlowerIconEngine(Color); }
    void paint(QPainter* Painter, const QRect& Rect, QIcon::Mode, QIcon::State) override
    {
        Painter->save();
        Painter->setRenderHint(QPainter::Antialiasing);
        const qreal Side = std::min(Rect.width(), Rect.height());
        Painter->translate(Rect.x() + (Rect.width() - Side) / 2, Rect.y() + (Rect.height() - Side) / 2);
        Painter->scale(Side / 24, Side / 24);
        Painter->setPen(QPen(Color, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        Painter->setBrush(Qt::NoBrush);
        QPainterPath Path;
        Path.moveTo(12, 5); Path.arcTo(QRectF(12, 2, 6, 6), 180, -270);
        Path.moveTo(12, 5); Path.arcTo(QRectF(6, 2, 6, 6), 0, 270);
        Path.moveTo(12, 5); Path.lineTo(12, 6);
        Path.moveTo(9, 8); Path.arcTo(QRectF(6, 8, 6, 6), 90, 270);
        Path.moveTo(9, 8); Path.lineTo(10, 8);
        Path.moveTo(15, 8); Path.arcTo(QRectF(12, 8, 6, 6), 90, -270);
        Path.moveTo(15, 8); Path.lineTo(14, 8);
        Path.moveTo(12, 11); Path.lineTo(12, 10);
        Path.moveTo(12, 10); Path.lineTo(12, 22);
        Path.moveTo(12, 22); Path.cubicTo(16.2, 22, 19, 20.333, 19, 17);
        Path.cubicTo(14.8, 17, 12, 18.667, 12, 22); Path.closeSubpath();
        Path.moveTo(12, 22); Path.cubicTo(7.8, 22, 5, 20.333, 5, 17);
        Path.cubicTo(9.2, 17, 12, 18.667, 12, 22); Path.closeSubpath();
        Painter->drawPath(Path);
        Painter->drawEllipse(QPointF(12, 8), 2, 2);
        Painter->restore();
    }
    QPixmap pixmap(const QSize& Size, QIcon::Mode Mode, QIcon::State State) override
    {
        QPixmap Image(Size);
        Image.fill(Qt::transparent);
        QPainter Painter(&Image);
        paint(&Painter, QRect(QPoint(), Size), Mode, State);
        return Image;
    }
private:
    QColor Color;
};
}

namespace EditorUi
{
QIcon MakeStudioIcon(StudioIcon Icon, const char* ColorToken)
{
    if (Icon == StudioIcon::Flower) { return QIcon(new FlowerIconEngine(EditorStyle::Color(ColorToken))); }
    // A two-times raster preserves thin strokes on high-DPI Qt hosts without QtSvg.
    QPixmap Image(32, 32);
    Image.fill(Qt::transparent);
    QPainter Painter(&Image);
    Painter.setRenderHint(QPainter::Antialiasing);
    Painter.scale(2, 2);
    Painter.setPen(QPen(EditorStyle::Color(ColorToken), 1.15, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath Path;
    switch (Icon)
    {
    case StudioIcon::ExternalLink: Path.moveTo(10, 2); Path.lineTo(14, 2); Path.lineTo(14, 6); Path.moveTo(14, 2); Path.lineTo(7, 9); Path.moveTo(6, 3); Path.lineTo(3, 3); Path.lineTo(3, 13); Path.lineTo(13, 13); Path.lineTo(13, 10); break;
    case StudioIcon::Layers: Path.moveTo(2, 5); Path.lineTo(8, 2); Path.lineTo(14, 5); Path.lineTo(8, 8); Path.closeSubpath(); Path.moveTo(2, 8); Path.lineTo(8, 11); Path.lineTo(14, 8); Path.moveTo(2, 11); Path.lineTo(8, 14); Path.lineTo(14, 11); break;
    case StudioIcon::ChevronDown: Path.moveTo(5, 6); Path.lineTo(8, 9); Path.lineTo(11, 6); break;
    case StudioIcon::ChevronRight: Path.moveTo(6, 5); Path.lineTo(9, 8); Path.lineTo(6, 11); break;
    case StudioIcon::Play: Path.moveTo(5, 3); Path.lineTo(12, 8); Path.lineTo(5, 13); Path.closeSubpath(); break;
    case StudioIcon::Pause: Painter.drawLine(5, 4, 5, 12); Painter.drawLine(10, 4, 10, 12); break;
    case StudioIcon::Step: Path.moveTo(4, 4); Path.lineTo(10, 8); Path.lineTo(4, 12); Path.closeSubpath(); Painter.drawLine(12, 4, 12, 12); break;
    case StudioIcon::Folder: Path.moveTo(2, 5); Path.lineTo(2, 3); Path.lineTo(6, 3); Path.lineTo(8, 5); Path.lineTo(14, 5); Path.lineTo(12, 13); Path.lineTo(2, 13); Path.closeSubpath(); break;
    case StudioIcon::Move: Painter.drawLine(8, 2, 8, 14); Painter.drawLine(2, 8, 14, 8); Path.moveTo(5, 5); Path.lineTo(8, 2); Path.lineTo(11, 5); Path.moveTo(5, 11); Path.lineTo(8, 14); Path.lineTo(11, 11); Path.moveTo(5, 5); Path.lineTo(2, 8); Path.lineTo(5, 11); Path.moveTo(11, 5); Path.lineTo(14, 8); Path.lineTo(11, 11); break;
    case StudioIcon::Rotate: Painter.drawArc(QRectF(3, 3, 10, 10), 30 * 16, 290 * 16); Path.moveTo(10, 2); Path.lineTo(13, 3); Path.lineTo(12, 6); break;
    case StudioIcon::Scale: Painter.drawRect(QRectF(3, 6, 7, 7)); Path.moveTo(7, 3); Path.lineTo(13, 3); Path.lineTo(13, 9); Path.moveTo(13, 3); Path.lineTo(7, 9); break;
    case StudioIcon::Cube: Path.moveTo(8, 2); Path.lineTo(14, 5); Path.lineTo(14, 11); Path.lineTo(8, 14); Path.lineTo(2, 11); Path.lineTo(2, 5); Path.closeSubpath(); Path.moveTo(2, 5); Path.lineTo(8, 8); Path.lineTo(14, 5); Path.moveTo(8, 8); Path.lineTo(8, 14); break;
    case StudioIcon::Camera: Painter.drawRoundedRect(QRectF(2, 5, 8, 7), 1, 1); Path.moveTo(10, 7); Path.lineTo(14, 5); Path.lineTo(14, 12); Path.lineTo(10, 10); break;
    case StudioIcon::Light: Painter.drawEllipse(QRectF(5, 2, 6, 8)); Painter.drawLine(6, 12, 10, 12); Painter.drawLine(7, 14, 9, 14); break;
    case StudioIcon::Search: Painter.drawEllipse(QRectF(2, 2, 8, 8)); Painter.drawLine(9, 9, 14, 14); break;
    case StudioIcon::Expand: Path.moveTo(3, 6); Path.lineTo(3, 3); Path.lineTo(6, 3); Path.moveTo(10, 3); Path.lineTo(13, 3); Path.lineTo(13, 6); Path.moveTo(13, 10); Path.lineTo(13, 13); Path.lineTo(10, 13); Path.moveTo(6, 13); Path.lineTo(3, 13); Path.lineTo(3, 10); break;
    case StudioIcon::Panel: Painter.drawRoundedRect(QRectF(2, 3, 12, 10), 1, 1); Painter.drawLine(10, 3, 10, 13); break;
    case StudioIcon::Grid: Painter.drawRect(QRectF(2, 2, 12, 12)); Painter.drawLine(6, 2, 6, 14); Painter.drawLine(10, 2, 10, 14); Painter.drawLine(2, 6, 14, 6); Painter.drawLine(2, 10, 14, 10); break;
    case StudioIcon::Plus: Painter.drawLine(8, 3, 8, 13); Painter.drawLine(3, 8, 13, 8); break;
    case StudioIcon::Fit: Painter.drawRect(QRectF(5, 5, 6, 6)); Path.moveTo(2, 5); Path.lineTo(2, 2); Path.lineTo(5, 2); Path.moveTo(11, 2); Path.lineTo(14, 2); Path.lineTo(14, 5); Path.moveTo(14, 11); Path.lineTo(14, 14); Path.lineTo(11, 14); Path.moveTo(5, 14); Path.lineTo(2, 14); Path.lineTo(2, 11); break;
    case StudioIcon::Flower: break; // Scalable frontend vector handled above.
    case StudioIcon::Message:
        Path.moveTo(3, 2); Path.lineTo(13, 2); Path.lineTo(14, 3); Path.lineTo(14, 11);
        Path.lineTo(13, 12); Path.lineTo(5, 12); Path.lineTo(2, 15); Path.lineTo(2, 3); Path.closeSubpath(); break;
    }
    Painter.drawPath(Path);
    Painter.end();
    Image.setDevicePixelRatio(2);
    return QIcon(Image);
}
}
