# Keep the last presented image covering a growing HWND while the render thread
# prepares the next frame. Diligent 2.5.6 hardcodes SCALING_NONE, which fills the
# newly exposed area with a background colour. This is not configurable through
# its public SwapChainDesc. Apply reproducibly to fresh and existing checkouts.
set(ResizeSource "${DILIGENT_SOURCE_DIRECTORY}/Graphics/GraphicsEngineD3DBase/include/SwapChainD3DBase.hpp")
file(READ "${ResizeSource}" ResizeContents)
set(ResizeOriginal "        swapChainDesc.Scaling     = DXGI_SCALING_NONE;")
set(ResizeReplacement "        // Sakura: cover the HWND between asynchronous resize and present.\n        swapChainDesc.Scaling     = DXGI_SCALING_STRETCH;")
string(FIND "${ResizeContents}" "${ResizeReplacement}" ResizePatchedPosition)
if(ResizePatchedPosition EQUAL -1)
    string(FIND "${ResizeContents}" "${ResizeOriginal}" ResizeOriginalPosition)
    if(ResizeOriginalPosition EQUAL -1)
        message(FATAL_ERROR "Diligent window resize patch does not match the pinned source")
    endif()
    string(REPLACE "${ResizeOriginal}" "${ResizeReplacement}" ResizeContents "${ResizeContents}")
    file(WRITE "${ResizeSource}" "${ResizeContents}")
endif()
