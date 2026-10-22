#include "UI/Views/WidgetCanvasView.h"
#include "UI/Styles/WidgetCanvasStyle.h"
#include "UI/Styles/EditorStyle.h"
#include "UI/WidgetLayout.h"
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>
#include <cmath>
namespace EditorUi
{
WidgetCanvasView::WidgetCanvasView(QWidget *Parent) : QWidget(Parent)
{
    setObjectName("WidgetCanvas");
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(120, 120);
    Context = {"Рассказчик",
               "Так будет выглядеть диалог в вашей игре. Перетащите окно или настройте его анкеры.",
               "Нажмите, чтобы продолжить",
               "Моя игра",
               {"Остаться и поговорить", "Продолжить путь"},
               true,
               true};
}
double WidgetCanvasView::Fit() const
{
    return std::max(.01, std::min((width() - 30.) / Frame.width(), (height() - 30.) / Frame.height()));
}
QPointF WidgetCanvasView::Origin() const
{
    return {(width() - Frame.width() * Fit()) / 2., (height() - Frame.height() * Fit()) / 2.};
}
void WidgetCanvasView::SetWidget(QJsonObject W, QString Id)
{
    if (Dragging && W["id"] == Data["id"])
        return;
    Cancel();
    Data = std::move(W);
    Selection = std::move(Id);
    update();
}
void WidgetCanvasView::SetLocked(bool Value)
{
    if (Value)
        Cancel();
    Locked = Value;
}
void WidgetCanvasView::paintEvent(QPaintEvent *)
{
    QPainter P(this);
    P.fillRect(rect(), EditorStyle::Color("--bg-app"));
    P.translate(Origin());
    P.scale(Fit(), Fit());
    WidgetCanvasStyle::Background(P, Frame);
    WidgetPainting::Paint(P, Data, Frame, Context);
    WidgetCanvasStyle::Selection(P, Data, Frame, Selection, Fit());
}
void WidgetCanvasView::Cancel()
{
    if (!Dragging)
        return;
    Data = Before;
    Dragging = false;
    releaseMouse();
    update();
}
void WidgetCanvasView::mousePressEvent(QMouseEvent *E)
{
    if (E->button() != Qt::LeftButton)
        return;
    setFocus();
    auto Pos = (E->position() - Origin()) / Fit();
    auto Rects = WidgetLayout::Rects(Data, Frame);
    auto Elements = Data["elements"].toArray();
    QString Hit;
    auto SelectedRect = Rects.value(Selection);
    Resizing = !Selection.isEmpty() &&
               QRectF(SelectedRect.bottomRight() - QPointF(9 / Fit(), 9 / Fit()), QSizeF(18 / Fit(), 18 / Fit()))
                   .contains(Pos);
    if (Resizing)
        Hit = Selection;
    else
        for (int I = Elements.size() - 1; I >= 0; --I)
            if (Rects[Elements[I].toObject()["id"].toString()].contains(Pos))
            {
                Hit = Elements[I].toObject()["id"].toString();
                break;
            }
    Selection = Hit;
    if (Selected)
        Selected(Hit);
    if (!Hit.isEmpty() && !Locked)
    {
        Before = Data;
        Start = E->position();
        Dragging = true;
        grabMouse();
    }
    update();
}
void WidgetCanvasView::mouseMoveEvent(QMouseEvent *E)
{
    if (!Dragging)
        return;
    auto Ref = Before["referenceSize"].toArray();
    auto Scale = Fit() * std::min(Frame.width() / Ref[0].toDouble(), Frame.height() / Ref[1].toDouble());
    auto D = (E->position() - Start) / Scale;
    double Step = Snap ? 8 : 1;
    D = {std::round(D.x() / Step) * Step, std::round(D.y() / Step) * Step};
    auto Elements = Before["elements"].toArray();
    for (int I = 0; I < Elements.size(); ++I)
    {
        auto Item = Elements[I].toObject();
        if (Item["id"] != Selection)
            continue;
        auto L = Item["layout"].toObject();
        if (Resizing)
        {
            auto R = WidgetLayout::Rects(Before)[Selection];
            D = {std::max(20 - R.width(), D.x()), std::max(20 - R.height(), D.y())};
            L["width"] = L["width"].toDouble() + D.x();
            L["height"] = L["height"].toDouble() + D.y();
            auto Pivot = L["pivot"].toArray();
            D = {D.x() * Pivot[0].toDouble(), D.y() * Pivot[1].toDouble()};
        }
        L["x"] = L["x"].toDouble() + D.x();
        L["y"] = L["y"].toDouble() + D.y();
        Item["layout"] = L;
        Elements[I] = Item;
    }
    Data["elements"] = Elements;
    update();
}
void WidgetCanvasView::mouseReleaseEvent(QMouseEvent *E)
{
    if (!Dragging || E->button() != Qt::LeftButton)
        return;
    mouseMoveEvent(E);
    Dragging = false;
    releaseMouse();
    for (auto V : Data["elements"].toArray())
        if (V.toObject()["id"] == Selection && LayoutChanged)
            LayoutChanged(Selection, V.toObject()["layout"].toObject());
}
void WidgetCanvasView::keyPressEvent(QKeyEvent *E)
{
    if (E->key() == Qt::Key_Escape)
    {
        Cancel();
        return;
    }
    if (Locked)
        return;
    if (E->key() == Qt::Key_Delete && DeleteRequested)
    {
        DeleteRequested();
        return;
    }
    QPointF Delta;
    if (E->key() == Qt::Key_Left)
        Delta.setX(-1);
    else if (E->key() == Qt::Key_Right)
        Delta.setX(1);
    else if (E->key() == Qt::Key_Up)
        Delta.setY(-1);
    else if (E->key() == Qt::Key_Down)
        Delta.setY(1);
    else
    {
        QWidget::keyPressEvent(E);
        return;
    }
    if (E->modifiers() & Qt::ShiftModifier)
        Delta *= 10;
    for (auto V : Data["elements"].toArray())
        if (V.toObject()["id"] == Selection && LayoutChanged)
        {
            auto L = V.toObject()["layout"].toObject();
            L["x"] = L["x"].toDouble() + Delta.x();
            L["y"] = L["y"].toDouble() + Delta.y();
            LayoutChanged(Selection, L);
        }
}
bool WidgetCanvasView::event(QEvent *E)
{
    if (E->type() == QEvent::WindowDeactivate || E->type() == QEvent::Hide || E->type() == QEvent::UngrabMouse)
        Cancel();
    return QWidget::event(E);
}
} // namespace EditorUi
