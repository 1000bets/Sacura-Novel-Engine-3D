#include "Core/Threading/ThreadContext.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace
{
thread_local ThreadRole LocalThreadRole = ThreadRole::Unknown;
thread_local std::string LocalThreadDebugName = "Unnamed";

std::atomic<uint64_t> GameFrameIndex{0};
std::atomic<uint64_t> RenderFrameIndex{0};
std::mutex PrintMutex;
}

void PrintString(const std::string& Message)
{
    std::lock_guard<std::mutex> Lock(PrintMutex);
    std::cout << Message << '\n';
}

void SetCurrentThreadRole(ThreadRole Role)
{
    LocalThreadRole = Role;
}

ThreadRole GetCurrentThreadRole()
{
    return LocalThreadRole;
}

bool IsGameThread()
{
    return LocalThreadRole == ThreadRole::Game;
}

bool IsRenderThread()
{
    return LocalThreadRole == ThreadRole::Render;
}

bool IsWorkerThread()
{
    return LocalThreadRole == ThreadRole::Worker;
}

void AssertGameThread()
{
    assert(IsGameThread() && "API requires Game Thread");
}

void AssertRenderThread()
{
    assert(IsRenderThread() && "API requires Render Thread");
}

void AssertWorkerThread()
{
    assert(IsWorkerThread() && "API requires Worker Thread");
}

void SetCurrentThreadDebugName(const std::string& Name)
{
    LocalThreadDebugName = Name;

#if defined(_WIN32)
    const int WideLength = MultiByteToWideChar(CP_UTF8, 0, Name.c_str(), -1, nullptr, 0);
    if (WideLength > 0)
    {
        std::wstring WideName(static_cast<size_t>(WideLength), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, Name.c_str(), -1, WideName.data(), WideLength);
        SetThreadDescription(GetCurrentThread(), WideName.c_str());
    }
#endif
}

const std::string& GetCurrentThreadDebugName()
{
    return LocalThreadDebugName;
}

void SetGameFrameIndex(uint64_t FrameIndex)
{
    GameFrameIndex.store(FrameIndex, std::memory_order_release);
}

void SetRenderFrameIndex(uint64_t FrameIndex)
{
    RenderFrameIndex.store(FrameIndex, std::memory_order_release);
}

uint64_t GetGameFrameIndex()
{
    return GameFrameIndex.load(std::memory_order_acquire);
}

uint64_t GetRenderFrameIndex()
{
    return RenderFrameIndex.load(std::memory_order_acquire);
}
