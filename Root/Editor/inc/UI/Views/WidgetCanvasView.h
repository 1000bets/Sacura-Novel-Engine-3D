#pragma once
#include "UI/WidgetSurface.h"
#include <QWidget>
#include <QJsonObject>
#include <functional>
namespace EditorUi
{
class WidgetCanvasView : public QWidget
{
  public:
    explicit WidgetCanvasView(QWidget *Parent = nullptr);
    void SetWidget(QJsonObject Widget, QString Selected);
    void SetFrame(QSize Size)
    {
        Frame = Size;
        update();
    }
    void SetSnap(bool Value)
    {
        Snap = Value;
    }
    void SetLocked(bool Value);
    void SetContext(WidgetContext Value)
    {
        Context = std::move(Value);
        update();
    }
    double Fit() const;
    std::function<void(QString)> Selected;
    std::function<void(QString, QJsonObject)> LayoutChanged;
    std::function<void()> DeleteRequested;

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    bool event(QEvent *) override;

  private:
    QPointF Origin() const;
    void Cancel();
    QJsonObject Data, Before;
    QString Selection;
    QSize Frame{1280, 720};
    WidgetContext Context;
    QPointF Start;
    bool Snap = true, Locked = false, Dragging = false, Resizing = false;
};
} // namespace EditorUi
