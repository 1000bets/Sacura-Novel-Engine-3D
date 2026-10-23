#pragma once

#include <QApplication>
#include <QEvent>
#include <QLayout>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QVariant>
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
                if (Area->property("inspectorScrollPending").toBool())
                {
                    ScrollPosition = Area->property("inspectorScrollVertical").toInt();
                    HorizontalPosition = Area->property("inspectorScrollHorizontal").toInt();
                }
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
            const auto Revision = Scroll->property("inspectorScrollRevision").toULongLong() + 1;
            Scroll->setProperty("inspectorScrollRevision", QVariant::fromValue(Revision));
            Scroll->setProperty("inspectorScrollPending", true);
            Scroll->setProperty("inspectorScrollVertical", ScrollPosition);
            Scroll->setProperty("inspectorScrollHorizontal", HorizontalPosition);
            QTimer::singleShot(0, Scroll.data(), [Area = Scroll, Revision,
                VerticalPosition = ScrollPosition, HorizontalPosition = HorizontalPosition]()
            {
                if (!Area || Area->property("inspectorScrollRevision").toULongLong() != Revision)
                {
                    return;
                }
                Area->setProperty("inspectorScrollPending", false);
                if (Area->verticalScrollBar()->isSliderDown() || Area->horizontalScrollBar()->isSliderDown())
                {
                    return;
                }
                Area->verticalScrollBar()->setValue(VerticalPosition);
                Area->horizontalScrollBar()->setValue(HorizontalPosition);
            });
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
