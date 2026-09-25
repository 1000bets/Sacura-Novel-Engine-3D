#pragma once

#include "Platform/NativeWindowInfo.h"
#include "Rendering/RenderView.h"

#include <QWidget>

class RenderViewportWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RenderViewportWidget(QWidget* Parent = nullptr);

    void SetViewCamera(const RenderViewCamera& Camera);
    const RenderViewCamera& GetViewCamera() const { return ViewCamera; }

    RenderViewId GetViewId() const { return ViewId; }
    void SetViewId(RenderViewId Id) { ViewId = Id; }

    NativeWindowInfo BuildNativeWindowInfo() const;
    bool HasValidNativeHandle() const;

signals:
    void NativeSurfaceChanged(RenderViewportWidget* Viewport);
    void ViewportResized(RenderViewportWidget* Viewport);

protected:
    bool event(QEvent* Event) override;
    void showEvent(QShowEvent* Event) override;
    void resizeEvent(QResizeEvent* Event) override;
    void hideEvent(QHideEvent* Event) override;
    bool nativeEvent(const QByteArray& EventType, void* Message, qintptr* Result) override;
    QPaintEngine* paintEngine() const override;

private:
    void RefreshNativeHandle();

    RenderViewCamera ViewCamera{};
    RenderViewId ViewId{};
    WId CachedNativeHandle = 0;
};
