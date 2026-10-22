#pragma once

#include <QApplication>
#include <QEvent>
#include <QLayout>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QWidget>

namespace EditorUi
{
// Settle the final layout before restoring scroll/painting. This also covers
// nested value editors that replace rows during an activation callback.
class InspectorUpdateGuard
{
public:
    explicit InspectorUpdateGuard(QWidget* Widget) : ChangedWidget(Widget), PaintRoot(Widget)
    {
        for (auto* Parent = Widget; Parent; Parent = Parent->parentWidget())
        {
            if (auto* Area = qobject_cast<QScrollArea*>(Parent))
            {
                Scroll = Area;
                PaintRoot = Area;
                ScrollPosition = Area->verticalScrollBar()->value();
                HorizontalPosition = Area->horizontalScrollBar()->value();
                break;
            }
        }
        WasUpdating = PaintRoot && PaintRoot->updatesEnabled();
        if (WasUpdating) { PaintRoot->setUpdatesEnabled(false); }
    }

    ~InspectorUpdateGuard()
    {
        if (Scroll && ChangedWidget)
        {
            for (auto* Widget = ChangedWidget.data(); Widget && Widget != Scroll; Widget = Widget->parentWidget())
            {
                if (Widget->layout()) { Widget->layout()->activate(); }
                Widget->updateGeometry();
            }
            QEvent LayoutRequest(QEvent::LayoutRequest);
            QApplication::sendEvent(Scroll, &LayoutRequest);
            Scroll->verticalScrollBar()->setValue(ScrollPosition);
            Scroll->horizontalScrollBar()->setValue(HorizontalPosition);
        }
        if (WasUpdating && PaintRoot) { PaintRoot->setUpdatesEnabled(true); }
    }

    InspectorUpdateGuard(const InspectorUpdateGuard&) = delete;
    InspectorUpdateGuard& operator=(const InspectorUpdateGuard&) = delete;
private:
    QPointer<QWidget> ChangedWidget;
    QPointer<QWidget> PaintRoot;
    QPointer<QScrollArea> Scroll;
    int ScrollPosition = 0;
    int HorizontalPosition = 0;
    bool WasUpdating = false;
};
}
