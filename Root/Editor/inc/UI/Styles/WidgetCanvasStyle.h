#pragma once
#include <QJsonObject>
#include <QSize>
class QPainter;
namespace EditorUi::WidgetCanvasStyle
{
void Background(QPainter &P, QSize Frame);
void Selection(QPainter &P, const QJsonObject &Widget, QSize Frame, const QString &Id, double Fit);
} // namespace EditorUi::WidgetCanvasStyle
