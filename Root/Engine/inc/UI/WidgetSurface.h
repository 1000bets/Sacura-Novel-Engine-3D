#pragma once
#include <QWidget>
#include <QJsonObject>
#include <QStringList>
#include <functional>
class QPainter;
struct WidgetContext
{
    QString Speaker, Text, Status, Title;
    QStringList Choices;
    bool CanAdvance = false, CanChoose = false;
};
namespace WidgetPainting
{
void Paint(QPainter &Painter, const QJsonObject &Widget, QSizeF Size, const WidgetContext &Context);
bool Visible(const QJsonObject &Element, const QJsonObject &Widget);
} // namespace WidgetPainting
class WidgetSurface : public QWidget
{
  public:
    explicit WidgetSurface(QWidget *Parent = nullptr);
    void SetWidget(QJsonObject Widget);
    void SetContext(WidgetContext Context);
    std::function<void(QString, int)> Activated;

  protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;

  private:
    QJsonObject Data;
    WidgetContext Context;
};
