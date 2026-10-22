#include "UI/Styles/WidgetCanvasStyle.h"
#include "UI/WidgetLayout.h"
#include <QPainter>
#include <QLinearGradient>
namespace EditorUi::WidgetCanvasStyle
{
void Background(QPainter &P, QSize Frame)
{
    QLinearGradient Gradient(0, 0, Frame.width(), Frame.height());
    Gradient.setColorAt(0, QColor("#4b394b"));
    Gradient.setColorAt(.65, QColor("#292637"));
    Gradient.setColorAt(1, QColor("#453647"));
    P.fillRect(QRect(QPoint(), Frame), Gradient);
    P.setPen(QColor(255, 255, 255, 8));
    for (int X = 0; X < Frame.width(); X += 20)
        P.drawLine(X, 0, X, Frame.height());
    for (int Y = 0; Y < Frame.height(); Y += 20)
        P.drawLine(0, Y, Frame.width(), Y);
    P.setPen(QColor("#60465c"));
    P.setBrush(Qt::NoBrush);
    P.drawRect(QRect(QPoint(), Frame));
}
void Selection(QPainter &P, const QJsonObject &W, QSize Frame, const QString &Id, double Fit)
{
    const auto Rects = WidgetLayout::Rects(W, Frame);
    QJsonObject Selected;
    for (auto Value : W["elements"].toArray())
    {
        const auto E = Value.toObject();
        bool Active = E["id"] == Id;
        if (Active)
            Selected = E;
        P.setPen(QPen(Active ? QColor("#f0b5d3") : QColor(203, 178, 205, 65), (Active ? 2. : 1.) / Fit,
                      Active ? Qt::SolidLine : Qt::DashLine));
        P.setBrush(Qt::NoBrush);
        auto R = Rects.value(E["id"].toString());
        P.drawRect(R);
        if (Active)
        {
            P.setBrush(QColor("#f0b5d3"));
            P.drawRect(QRectF(R.bottomRight() - QPointF(5 / Fit, 5 / Fit), QSizeF(10 / Fit, 10 / Fit)));
        }
    }
    if (Selected.isEmpty())
        return;
    auto Parent = Selected["parentId"].toString();
    auto R = Parent.isEmpty() ? QRectF(QPointF(), Frame) : Rects.value(Parent);
    auto L = Selected["layout"].toObject();
    P.setPen(QPen(QColor("#83d2cf"), 1 / Fit));
    P.setBrush(QColor("#213b3b"));
    for (auto Key : {"anchorMin", "anchorMax"})
    {
        auto A = L[Key].toArray();
        QPointF Pos = R.topLeft() + QPointF(R.width() * A[0].toDouble(), R.height() * A[1].toDouble());
        P.drawRect(QRectF(Pos - QPointF(4 / Fit, 4 / Fit), QSizeF(8 / Fit, 8 / Fit)));
    }
}
} // namespace EditorUi::WidgetCanvasStyle
