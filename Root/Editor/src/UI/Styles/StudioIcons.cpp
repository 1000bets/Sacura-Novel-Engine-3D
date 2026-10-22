#include "UI/Styles/StudioIcons.h"
#include "UI/Styles/EditorStyle.h"

#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <algorithm>
#include <cmath>

namespace
{
#include "FrontendIconPaths.inc"

// Original Lucide geometry, painted at the requested device scale. No small
// raster cache, font glyphs or QtSvg plugin dependency.
class FrontendIconEngine final : public QIconEngine
{
public:
    FrontendIconEngine(QPainterPath Path, QColor Color, QColor CheckedColor)
        : Path(std::move(Path)), Color(Color), CheckedColor(CheckedColor) {}
    QIconEngine* clone() const override { return new FrontendIconEngine(Path, Color, CheckedColor); }
    void paint(QPainter* Painter, const QRect& Rect, QIcon::Mode Mode, QIcon::State State) override
    {
        Painter->save();
        Painter->setRenderHint(QPainter::Antialiasing);
        const qreal Side = std::min(Rect.width(), Rect.height());
        Painter->translate(Rect.x() + (Rect.width() - Side) / 2, Rect.y() + (Rect.height() - Side) / 2);
        Painter->scale(Side / 24, Side / 24);
        QColor Ink = Color;
        if (Mode == QIcon::Disabled) { Ink = EditorUi::EditorStyle::Color("--text-disabled"); }
        else if (State == QIcon::On) { Ink = CheckedColor; }
        else if (Mode == QIcon::Active || Mode == QIcon::Selected) { Ink = EditorUi::EditorStyle::Color("--text-primary"); }
        Painter->setPen(QPen(Ink, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        Painter->setBrush(Qt::NoBrush);
        Painter->drawPath(Path);
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
    QPixmap scaledPixmap(const QSize& Size, QIcon::Mode Mode, QIcon::State State, qreal Scale) override
    {
        // Qt 6.8 passes the logical size here.
        QPixmap Image = pixmap(QSize(qCeil(Size.width() * Scale), qCeil(Size.height() * Scale)), Mode, State);
        Image.setDevicePixelRatio(Scale);
        return Image;
    }
private:
    QPainterPath Path;
    QColor Color;
    QColor CheckedColor;
};
}

namespace EditorUi
{
QIcon MakeFrontendIcon(const QString& Name, const char* ColorToken, const char* CheckedColorToken)
{
    const QPainterPath Path = FrontendIconPath(Name);
    if (Path.isEmpty())
    {
        qWarning("Unknown frontend icon: %s", qPrintable(Name));
        return {};
    }
    return QIcon(new FrontendIconEngine(Path, EditorStyle::Color(ColorToken), EditorStyle::Color(CheckedColorToken)));
}

QIcon MakeStudioIcon(StudioIcon Icon, const char* ColorToken, const char* CheckedColorToken)
{
    // Keep existing semantic names for native callers; use the same sources
    // as <Icon name=...> in React throughout the shell and inspector.
    static const char* Names[] = {
        "Flower2", "Play", "Pause", "StepForward", "FolderOpen", "Move", "Rotate3D", "Scaling",
        "Box", "Sandwich", "Sandwich", "Video", "Lightbulb", "Search", "Maximize2", "PanelRightClose",
        "Grid3X3", "Plus", "Focus", "ChevronDown", "ChevronRight", "MessageSquare", "Layers",
        "ExternalLink", "Copy", "Trash2", "Star", "MousePointer2", "Magnet", "CircleHelp"
    };
    static_assert(std::size(Names) == static_cast<size_t>(StudioIcon::Count));
    const auto Index = static_cast<size_t>(Icon);
    return Index < std::size(Names) ? MakeFrontendIcon(QString::fromLatin1(Names[Index]), ColorToken, CheckedColorToken) : QIcon{};
}
}
