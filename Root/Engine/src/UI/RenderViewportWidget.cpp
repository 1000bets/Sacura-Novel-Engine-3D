#include "UI/RenderViewportWidget.h"

#include <QHideEvent>
#include <QPaintEngine>
#include <QPlatformSurfaceEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include <QtGlobal>

#ifdef Q_OS_WIN
#    include <windows.h>
#endif

RenderViewportWidget::RenderViewportWidget(QWidget* Parent)
    : QWidget(Parent)
{
    setAttribute(Qt::WA_NativeWindow, true);
    setAttribute(Qt::WA_PaintOnScreen, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAutoFillBackground(false);
    setMinimumSize(160, 120);
    setFocusPolicy(Qt::ClickFocus);

    ViewCamera.Position = Vector3(0.f, 1.6f, 4.f);
    ViewCamera.Target = Vector3::Zero;
}

void RenderViewportWidget::SetViewCamera(const RenderViewCamera& Camera)
{
    ViewCamera = Camera;
}

NativeWindowInfo RenderViewportWidget::BuildNativeWindowInfo() const
{
    NativeWindowInfo Info{};
    const qreal PixelRatio = devicePixelRatioF();
    Info.Width = static_cast<uint32_t>(qMax(1, static_cast<int>(width() * PixelRatio)));
    Info.Height = static_cast<uint32_t>(qMax(1, static_cast<int>(height() * PixelRatio)));
    if (CachedNativeHandle != 0)
    {
        Info.WindowHandle = reinterpret_cast<void*>(CachedNativeHandle);
    }
    if (!isVisible() || window()->isMinimized())
    {
        Info.Width = 0;
        Info.Height = 0;
    }
    return Info;
}

bool RenderViewportWidget::HasValidNativeHandle() const
{
    return CachedNativeHandle != 0;
}

QPaintEngine* RenderViewportWidget::paintEngine() const
{
    return nullptr;
}

void RenderViewportWidget::RefreshNativeHandle()
{
    const WId Current = winId();
    if (Current == 0)
    {
        return;
    }
    if (Current == CachedNativeHandle)
    {
        return;
    }
    CachedNativeHandle = Current;
    emit NativeSurfaceChanged(this);
}

void RenderViewportWidget::showEvent(QShowEvent* Event)
{
    QWidget::showEvent(Event);
    RefreshNativeHandle();
    emit ViewportResized(this);
}

void RenderViewportWidget::resizeEvent(QResizeEvent* Event)
{
    QWidget::resizeEvent(Event);
    RefreshNativeHandle();
    emit ViewportResized(this);
}

void RenderViewportWidget::hideEvent(QHideEvent* Event)
{
    QWidget::hideEvent(Event);
    emit ViewportResized(this);
}

bool RenderViewportWidget::nativeEvent(const QByteArray& EventType, void* Message, qintptr* Result)
{
#ifdef Q_OS_WIN
    if (EventType == "windows_generic_MSG" || EventType == "windows_dispatcher_MSG")
    {
        const MSG* WindowsMessage = static_cast<MSG*>(Message);
        if (WindowsMessage != nullptr && WindowsMessage->message == WM_DESTROY)
        {
            CachedNativeHandle = 0;
            emit NativeSurfaceChanged(this);
        }
    }
#else
    (void)EventType;
    (void)Message;
#endif
    return QWidget::nativeEvent(EventType, Message, Result);
}

bool RenderViewportWidget::event(QEvent* Event)
{
    if (Event->type() == QEvent::PlatformSurface)
    {
        auto* SurfaceEvent = static_cast<QPlatformSurfaceEvent*>(Event);
        if (SurfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
        {
            CachedNativeHandle = 0;
            emit NativeSurfaceChanged(this);
        }
    }
    const bool bResult = QWidget::event(Event);
    if (Event->type() == QEvent::DevicePixelRatioChange)
    {
        emit ViewportResized(this);
    }
    return bResult;
}
