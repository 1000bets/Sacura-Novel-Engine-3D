#include "Platform/WindowSubsystem.h"
#include "Core/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"

#include <SDL3/SDL.h>

WindowSubsystem* WindowSubsystem::Instance = nullptr;

WindowSubsystem* WindowSubsystem::Get()
{
    return Instance;
}

void WindowSubsystem::Initialize()
{
    Instance = this;
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        PrintString(std::string("WindowSubsystem: SDL_Init failed: ") + SDL_GetError());
    }
}

void WindowSubsystem::Deinitialize()
{
    if (Window != nullptr)
    {
        SDL_DestroyWindow(Window);
        Window = nullptr;
    }

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    if (Instance == this)
    {
        Instance = nullptr;
    }
}

void WindowSubsystem::CreateMainWindow(const std::string& Title, uint32_t InWidth, uint32_t InHeight, bool bVisible)
{
    AssertGameThread();

    Width = InWidth;
    Height = InHeight;

    SDL_WindowFlags Flags = SDL_WINDOW_RESIZABLE;
    if (!bVisible)
    {
        Flags |= SDL_WINDOW_HIDDEN;
    }

    Window = SDL_CreateWindow(Title.c_str(), static_cast<int>(Width), static_cast<int>(Height), Flags);
    if (Window == nullptr)
    {
        PrintString(std::string("WindowSubsystem: SDL_CreateWindow failed: ") + SDL_GetError());
    }
}

NativeWindowInfo WindowSubsystem::GetNativeWindowInfo() const
{
    NativeWindowInfo Info;
    Info.Width = Width;
    Info.Height = Height;

    if (Window == nullptr)
    {
        return Info;
    }

#if defined(_WIN32)
    Info.WindowHandle = SDL_GetPointerProperty(
        SDL_GetWindowProperties(Window),
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr);
#endif

    return Info;
}

void WindowSubsystem::Tick(float)
{
    PumpEvents();
}

void WindowSubsystem::PumpEvents()
{
    AssertGameThread();

    SDL_Event Event;
    while (SDL_PollEvent(&Event))
    {
        if (Event.type == SDL_EVENT_QUIT)
        {
            bCloseRequested = true;
        }
        else if (Event.type == SDL_EVENT_WINDOW_RESIZED)
        {
            HandleResize(static_cast<uint32_t>(Event.window.data1), static_cast<uint32_t>(Event.window.data2));
        }
    }
}

void WindowSubsystem::HandleResize(uint32_t NewWidth, uint32_t NewHeight)
{
    if (NewWidth == 0 || NewHeight == 0)
    {
        return;
    }

    Width = NewWidth;
    Height = NewHeight;

    if (RenderThread* Render = RenderThread::Get())
    {
        Render->Enqueue([NewWidth, NewHeight]()
        {
            if (RenderThread* Current = RenderThread::Get())
            {
                Current->ResizeRenderer(NewWidth, NewHeight);
            }
        });
    }
}
