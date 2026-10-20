#include "Rendering/RHI/NativeSurfaceMacOS.h"
#include "Core/Threading/ThreadContext.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

void PrepareMacOSRenderSurface(void* NativeView, double PixelRatio)
{
    AssertGameThread();
    NSView* View = (__bridge NSView*)NativeView;
    if (View == nil)
    {
        return;
    }
    if (![View.layer isKindOfClass:[CAMetalLayer class]])
    {
        View.wantsLayer = YES;
        View.layer = [CAMetalLayer layer];
    }
    View.layer.contentsScale = PixelRatio;
}
