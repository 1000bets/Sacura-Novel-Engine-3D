#include "TemplateCardWidget.h"

#include <QGraphicsDropShadowEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace
{
QPixmap MakeRoundedThumbnail(const QPixmap& Source, const QSize& Size, int Radius)
{
    QPixmap Result(Size);
    Result.fill(Qt::transparent);

    QPainter Painter(&Result);
    Painter.setRenderHint(QPainter::Antialiasing, true);
    Painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    QPainterPath Clip;
    Clip.addRoundedRect(QRectF(QPointF(0, 0), Size), Radius, Radius);
    Painter.setClipPath(Clip);

    if (Source.isNull())
    {
        Painter.fillRect(Result.rect(), QColor("#16161F"));
        return Result;
    }

    const QPixmap Scaled = Source.scaled(Size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const int OffsetX = (Scaled.width() - Size.width()) / 2;
    const int OffsetY = (Scaled.height() - Size.height()) / 2;
    Painter.drawPixmap(0, 0, Scaled, OffsetX, OffsetY, Size.width(), Size.height());
    return Result;
}

QPixmap MakeEmptyDocumentThumb(const QSize& Size)
{
    QPixmap Result(Size);
    Result.fill(Qt::transparent);
    QPainter Painter(&Result);
    Painter.setRenderHint(QPainter::Antialiasing, true);
    Painter.fillRect(Result.rect(), QColor("#16161F"));

    const QRect Page(Size.width() / 2 - 28, Size.height() / 2 - 34, 56, 68);
    Painter.setBrush(QColor("#2A2A38"));
    Painter.setPen(QPen(QColor("#5A5A6A"), 1.5));
    Painter.drawRoundedRect(Page, 6, 6);
    Painter.setPen(QPen(QColor("#6A6A7A"), 2));
    Painter.drawLine(Page.left() + 12, Page.top() + 18, Page.right() - 12, Page.top() + 18);
    Painter.drawLine(Page.left() + 12, Page.top() + 28, Page.right() - 12, Page.top() + 28);
    Painter.drawLine(Page.left() + 12, Page.top() + 38, Page.right() - 18, Page.top() + 38);
    return Result;
}
}

TemplateCardWidget::TemplateCardWidget(
    const QString& InTemplateId,
    const QString& Title,
    const QString& Description,
    const QPixmap& Thumbnail,
    bool bEmptyStyle,
    QWidget* Parent)
    : QFrame(Parent)
    , CardTemplateId(InTemplateId)
    , bEmptyCard(bEmptyStyle)
{
    setObjectName("TemplateCard");
    setCursor(Qt::PointingHandCursor);
    setFixedSize(220, 210);

    auto* Layout = new QVBoxLayout(this);
    Layout->setContentsMargins(10, 10, 10, 10);
    Layout->setSpacing(8);

    ThumbnailLabel = new QLabel(this);
    ThumbnailLabel->setObjectName("TemplateThumb");
    ThumbnailLabel->setFixedSize(200, 112);
    ThumbnailLabel->setAlignment(Qt::AlignCenter);

    const QPixmap DisplayThumb = bEmptyCard
        ? MakeEmptyDocumentThumb(ThumbnailLabel->size())
        : MakeRoundedThumbnail(Thumbnail, ThumbnailLabel->size(), 10);
    ThumbnailLabel->setPixmap(DisplayThumb);
    ThumbnailLabel->setStyleSheet("QLabel#TemplateThumb { background: transparent; border-radius: 10px; }");

    TitleLabel = new QLabel(Title, this);
    TitleLabel->setObjectName("TemplateTitle");
    TitleLabel->setAlignment(Qt::AlignHCenter);
    TitleLabel->setStyleSheet("color: #F5F5FA; font-size: 14px; font-weight: 600;");

    DescriptionLabel = new QLabel(Description, this);
    DescriptionLabel->setObjectName("TemplateDescription");
    DescriptionLabel->setAlignment(Qt::AlignHCenter);
    DescriptionLabel->setWordWrap(true);
    DescriptionLabel->setStyleSheet("color: #9A9AAB; font-size: 11px;");

    CheckLabel = new QLabel(this);
    CheckLabel->setFixedSize(22, 22);
    CheckLabel->setAlignment(Qt::AlignCenter);
    CheckLabel->hide();
    CheckLabel->raise();

    Layout->addWidget(ThumbnailLabel);
    Layout->addWidget(TitleLabel);
    Layout->addWidget(DescriptionLabel);
    Layout->addStretch(1);

    RefreshStyle();
}

void TemplateCardWidget::SetSelected(bool bInSelected)
{
    if (bSelected == bInSelected)
    {
        return;
    }
    bSelected = bInSelected;
    RefreshStyle();
    update();
}

void TemplateCardWidget::mousePressEvent(QMouseEvent* Event)
{
    if (Event->button() == Qt::LeftButton)
    {
        emit Clicked(CardTemplateId);
    }
    QFrame::mousePressEvent(Event);
}

void TemplateCardWidget::paintEvent(QPaintEvent* Event)
{
    QFrame::paintEvent(Event);

    if (!bSelected)
    {
        return;
    }

    QPainter Painter(this);
    Painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect CheckRect(width() - 30, 12, 22, 22);
    Painter.setBrush(QColor("#FF7EB6"));
    Painter.setPen(Qt::NoPen);
    Painter.drawEllipse(CheckRect);

    Painter.setPen(QPen(QColor("#1A1A24"), 2.4));
    Painter.drawLine(CheckRect.left() + 6, CheckRect.center().y() + 1, CheckRect.center().x() - 1, CheckRect.bottom() - 6);
    Painter.drawLine(CheckRect.center().x() - 1, CheckRect.bottom() - 6, CheckRect.right() - 5, CheckRect.top() + 6);
}

void TemplateCardWidget::RefreshStyle()
{
    if (bSelected)
    {
        setStyleSheet(
            "QFrame#TemplateCard {"
            "  background: #1E1E2A;"
            "  border: 2px solid #FF7EB6;"
            "  border-radius: 14px;"
            "}");
        setGraphicsEffect(nullptr);
        auto* Glow = new QGraphicsDropShadowEffect(this);
        Glow->setBlurRadius(28);
        Glow->setColor(QColor(255, 126, 182, 160));
        Glow->setOffset(0, 0);
        setGraphicsEffect(Glow);
    }
    else
    {
        setGraphicsEffect(nullptr);
        setStyleSheet(
            "QFrame#TemplateCard {"
            "  background: #181822;"
            "  border: 1px solid #2A2A38;"
            "  border-radius: 14px;"
            "}");
    }
}
