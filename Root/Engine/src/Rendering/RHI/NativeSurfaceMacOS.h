#pragma once

// Called by the Qt host on the Game thread before publishing an NSView.
void PrepareMacOSRenderSurface(void* NativeView, double PixelRatio);
