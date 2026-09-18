#pragma once

#include "Core/Subsystem.h"
#include "Platform/NativeWindowInfo.h"

#include <cstdint>
#include <string>

struct SDL_Window;

class WindowSubsystem : public Subsystem
{
public:
    void Initialize() override;
    void Deinitialize() override;
    void Tick(float DeltaTime) override;

    void CreateMainWindow(const std::string& Title, uint32_t Width, uint32_t Height, bool bVisible = true);
    void PumpEvents();

    NativeWindowInfo GetNativeWindowInfo() const;
    uint32_t GetWidth() const { return Width; }
    uint32_t GetHeight() const { return Height; }
    bool IsCloseRequested() const { return bCloseRequested; }

    static WindowSubsystem* Get();

private:
    void HandleResize(uint32_t NewWidth, uint32_t NewHeight);

    static WindowSubsystem* Instance;

    SDL_Window* Window = nullptr;
    uint32_t Width = 1280;
    uint32_t Height = 720;
    bool bCloseRequested = false;
};
