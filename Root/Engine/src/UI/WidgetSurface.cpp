#include "UI/WidgetSurface.h"
#include "UI/WidgetLayout.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QImage>
#include <QImageReader>
#include <QBuffer>
#include <algorithm>

namespace
{
QColor Color(QString Text)
{
    return Text == "transparent" ? QColor(Qt::transparent) : QColor(Text);
}
double Number(const QJsonObject &S, const char *Key, double Fallback, double Maximum)
{
    return std::clamp(S[Key].toDouble(Fallback), 0., Maximum);
}
QString Text(const QJsonObject &E, const WidgetContext &C)
{
    auto Role = E["role"].toString();
    if (Role == "speaker")
        return C.Speaker;
    if (Role == "dialogueText")
        return C.Text;
    if (Role == "status")
        return C.Status;
    if (Role == "title")
        return C.Title;
    return E["text"].toString();
}
} // namespace
bool WidgetPainting::Visible(const QJsonObject &E, const QJsonObject &W)
{
    if (!E["visible"].toBool(true))
        return false;
    auto Parent = E["parentId"].toString();
    int Depth = 0;
    while (!Parent.isEmpty() && ++Depth < 513)
    {
        bool Found = false;
        for (auto Value : W["elements"].toArray())
            if (Value.toObject()["id"] == Parent)
            {
                auto P = Value.toObject();
                if (!P["visible"].toBool(true))
                    return false;
                Parent = P["parentId"].toString();
                Found = true;
                break;
            }
        if (!Found)
            break;
    }
    return Depth < 513;
}
void WidgetPainting::Paint(QPainter &P, const QJsonObject &W, QSizeF Size, const WidgetContext &C)
{
    const auto Rects = WidgetLayout::Rects(W, Size);
    auto Ref = W["referenceSize"].toArray();
    double Scale = std::min(Size.width() / Ref[0].toDouble(1280), Size.height() / Ref[1].toDouble(720));
    P.save();
    P.setClipRect(QRectF(QPointF(), Size));
    P.setRenderHint(QPainter::Antialiasing);
    for (auto Value : W["elements"].toArray())
    {
        auto E = Value.toObject();
        if (!Visible(E, W))
            continue;
        const auto R = Rects.value(E["id"].toString());
        const auto S = E["style"].toObject();
        auto Type = E["type"].toString();
        P.save();
        P.setOpacity(Number(S, "opacity", 1, 1));
        auto Width = Number(S, "borderWidth", 0, 12) * Scale;
        P.setPen(Width > 0 ? QPen(Color(S["borderColor"].toString("#ad849d")), Width) : QPen(Qt::NoPen));
        P.setBrush(Color(S["background"].toString("transparent")));
        auto Radius = Number(S, "borderRadius", 0, 100) * Scale;
        P.drawRoundedRect(R, Radius, Radius);
        QFont Font(S["fontFamily"].toString("Segoe UI"));
        Font.setPixelSize(std::max(1, int(Number(S, "fontSize", 24, 144) * Scale)));
        Font.setWeight(static_cast<QFont::Weight>(std::clamp(S["fontWeight"].toInt(400), 100, 900)));
        P.setFont(Font);
        P.setPen(Color(S["color"].toString("#fff4f8")));
        const auto Align = S["textAlign"].toString();
        int Flags = Qt::TextWordWrap | (Align == "center"  ? Qt::AlignHCenter
                                        : Align == "right" ? Qt::AlignRight
                                                           : Qt::AlignLeft);
        auto Pad = Number(S, "padding", 0, 80) * Scale;
        auto Content = R.adjusted(Pad, Pad, -Pad, -Pad);
        P.setClipRect(R, Qt::IntersectClip);
        if (Type == "choices")
        {
            const auto Gap = Number(S, "gap", 8, 40) * Scale;
            double Y = Content.y();
            for (const auto &Choice : C.Choices)
            {
                const double H =
                    std::max(48. * Scale,
                             P.fontMetrics()
                                     .boundingRect(QRect(0, 0, int(Content.width() - 28 * Scale), 10000), Flags, Choice)
                                     .height() +
                                 16 * Scale);
                QRectF Row(Content.x(), Y, Content.width(), H);
                P.setPen(Qt::NoPen);
                P.setBrush(QColor("#3d2b48"));
                P.drawRoundedRect(Row, Radius, Radius);
                P.setPen(Color(S["color"].toString("#fff4f8")));
                P.drawText(Row.adjusted(14 * Scale, 0, -14 * Scale, 0), Flags | Qt::AlignVCenter, Choice);
                Y += H + Gap;
            }
        }
        else if (Type == "image")
        {
            auto Src = E["src"].toString();
            auto Bytes = QByteArray::fromBase64(Src.mid(Src.indexOf(',') + 1).toLatin1());
            QBuffer Buffer(&Bytes);
            Buffer.open(QIODevice::ReadOnly);
            QImageReader Reader(&Buffer);
            if (Reader.size().width() <= 8192 && Reader.size().height() <= 8192)
            {
                const auto Image = Reader.read();
                if (!Image.isNull())
                {
                    auto Fit = Image.size().scaled(Content.size().toSize(), Qt::KeepAspectRatio);
                    P.drawImage(QRectF(Content.center() - QPointF(Fit.width() / 2., Fit.height() / 2.), Fit), Image);
                }
            }
        }
        else if (Type != "panel")
            P.drawText(Content, Flags | (Type == "button" ? Qt::AlignVCenter : Qt::AlignTop), Text(E, C));
        P.restore();
    }
    P.restore();
}
WidgetSurface::WidgetSurface(QWidget *Parent) : QWidget(Parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumHeight(240);
}
void WidgetSurface::SetWidget(QJsonObject W)
{
    Data = std::move(W);
    update();
}
void WidgetSurface::SetContext(WidgetContext C)
{
    Context = std::move(C);
    update();
}
void WidgetSurface::paintEvent(QPaintEvent *)
{
    QPainter P(this);
    WidgetPainting::Paint(P, Data, size(), Context);
}
void WidgetSurface::mouseReleaseEvent(QMouseEvent *Event)
{
    if (Event->button() != Qt::LeftButton || !isEnabled() || !Activated)
        return;
    auto Rects = WidgetLayout::Rects(Data, size());
    auto Elements = Data["elements"].toArray();
    for (int I = Elements.size() - 1; I >= 0; --I)
    {
        auto E = Elements[I].toObject();
        const auto R = Rects[E["id"].toString()];
        if (!WidgetPainting::Visible(E, Data) || !R.contains(Event->position()))
            continue;
        if (E["type"] == "choices" && Context.CanChoose)
        {
            auto S = E["style"].toObject();
            auto Ref = Data["referenceSize"].toArray();
            double Scale = std::min(width() / Ref[0].toDouble(1280), height() / Ref[1].toDouble(720));
            double Pad = Number(S, "padding", 0, 80) * Scale, Gap = Number(S, "gap", 8, 40) * Scale, Y = R.y() + Pad;
            QFont Font(S["fontFamily"].toString("Segoe UI"));
            Font.setPixelSize(std::max(1, int(Number(S, "fontSize", 24, 144) * Scale)));
            Font.setWeight(static_cast<QFont::Weight>(std::clamp(S["fontWeight"].toInt(400), 100, 900)));
            QFontMetrics Metrics(Font);
            for (int Choice = 0; Choice < Context.Choices.size(); ++Choice)
            {
                double H = std::max(48. * Scale,
                                    Metrics.boundingRect(QRect(0, 0, int(R.width() - 2 * Pad - 28 * Scale), 10000),
                                                         Qt::TextWordWrap, Context.Choices[Choice])
                                            .height() +
                                        16 * Scale);
                if (QRectF(R.x() + Pad, Y, R.width() - 2 * Pad, H).contains(Event->position()))
                {
                    Activated("choice", Choice);
                    return;
                }
                Y += H + Gap;
            }
        }
        if (E["type"] == "button")
        {
            auto Action = E["action"].toString();
            if (Action != "none" && (Action != "advance" || Context.CanAdvance))
                Activated(Action, -1);
            return;
        }
        if (E["type"] == "panel" && E["parentId"].toString().isEmpty() &&
            Data["kind"] == "dialogue" && Context.CanAdvance)
        {
            Activated("advance", -1);
            return;
        }
    }
}
void WidgetSurface::keyPressEvent(QKeyEvent *Event)
{
    if (isEnabled() && Context.CanAdvance && Activated &&
        (Event->key() == Qt::Key_Space || Event->key() == Qt::Key_Return))
    {
        Activated("advance", -1);
        return;
    }
    QWidget::keyPressEvent(Event);
}
