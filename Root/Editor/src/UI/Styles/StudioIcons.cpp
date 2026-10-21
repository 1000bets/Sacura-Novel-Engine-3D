#include "UI/Styles/StudioIcons.h"
#include "UI/Styles/EditorStyle.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace EditorUi
{
QIcon MakeStudioIcon(StudioIcon Icon, const char* ColorToken)
{
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
    case StudioIcon::Flower:
        for (int Petal = 0; Petal < 5; ++Petal)
        {
            Painter.save(); Painter.translate(8, 7); Painter.rotate(Petal * 72); Painter.drawEllipse(QRectF(-2, -5, 4, 5)); Painter.restore();
        }
        Painter.drawEllipse(QRectF(6.5, 5.5, 3, 3)); Painter.drawLine(8, 11, 8, 15); break;
    }
    Painter.drawPath(Path);
    Painter.end();
    Image.setDevicePixelRatio(2);
    return QIcon(Image);
}
}
