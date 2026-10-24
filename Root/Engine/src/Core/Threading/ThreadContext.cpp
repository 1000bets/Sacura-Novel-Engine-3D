#include "Core/Threading/ThreadContext.h"

#include <atomic>
#include <cassert>
#include <iostream>
#include <mutex>
#include <string>
#include <deque>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <thread>

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
std::deque<std::pair<uint64_t, std::string>> PrintHistory;
uint64_t PrintSequence = 0;
std::ofstream PrintLog;
std::filesystem::path PrintLogPath;
const std::string PrintSessionName = "Engine-" + std::to_string(
    std::chrono::system_clock::now().time_since_epoch().count()) + ".log";
}

void SetPrintLogDirectory(const std::filesystem::path& Directory)
{
    std::lock_guard<std::mutex> Lock(PrintMutex);
    const auto Destination = Directory / PrintSessionName;
    if (Destination == PrintLogPath)
    {
        return;
    }
    std::error_code Error;
    std::filesystem::create_directories(Directory, Error);
    if (!Error && !PrintLogPath.empty())
    {
        PrintLog.flush();
        std::filesystem::copy_file(PrintLogPath, Destination,
            std::filesystem::copy_options::none, Error);
    }
    if (Error)
    {
        std::cerr << "Logging: cannot prepare session log: " << Error.message() << std::endl;
        return;
    }
    std::ofstream DestinationLog(Destination, std::ios::app | std::ios::binary);
    if (!DestinationLog)
    {
        std::cerr << "Logging: cannot open session log" << std::endl;
        return;
    }
    PrintLog = std::move(DestinationLog);
    PrintLogPath = Destination;
}

void PrintString(const std::string& Message)
{
    std::lock_guard<std::mutex> Lock(PrintMutex);
    std::cout << Message << '\n';
    if (PrintLog.is_open() && PrintLog.good())
    {
        const auto Now = std::chrono::system_clock::now();
        const auto Milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch());
        const auto Seconds = std::chrono::system_clock::to_time_t(Now);
        std::tm Calendar{};
#if defined(_WIN32)
        gmtime_s(&Calendar, &Seconds);
#else
        gmtime_r(&Seconds, &Calendar);
#endif
        PrintLog << std::put_time(&Calendar, "%Y-%m-%dT%H:%M:%S") << '.'
                 << std::setfill('0') << std::setw(3) << Milliseconds.count() % 1000
                 << "Z [" << LocalThreadDebugName << ':' << std::this_thread::get_id()
                 << "] " << Message << std::endl;
        if (!PrintLog)
        {
            std::cerr << "Logging: session log write failed" << std::endl;
        }
    }
    PrintHistory.emplace_back(++PrintSequence, Message.substr(0, 16384));
    if (PrintHistory.size() > 1000)
    {
        PrintHistory.pop_front();
    }
}

std::vector<std::string> GetPrintMessages(uint64_t& AfterSequence)
{
    std::lock_guard<std::mutex> Lock(PrintMutex);
    std::vector<std::string> Messages;
    for (const auto& Entry : PrintHistory)
    {
        if (Entry.first > AfterSequence)
        {
            Messages.push_back(Entry.second);
        }
    }
    AfterSequence = PrintSequence;
    return Messages;
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
