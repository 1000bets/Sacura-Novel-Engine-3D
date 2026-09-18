#pragma once

#include "Core/Threading/ThreadRole.h"

#include <cstdint>
#include <string>

void PrintString(const std::string& Message);

void SetCurrentThreadRole(ThreadRole Role);
ThreadRole GetCurrentThreadRole();

bool IsGameThread();
bool IsRenderThread();
bool IsWorkerThread();

void AssertGameThread();
void AssertRenderThread();
void AssertWorkerThread();

void SetCurrentThreadDebugName(const std::string& Name);
const std::string& GetCurrentThreadDebugName();

void SetGameFrameIndex(uint64_t FrameIndex);
void SetRenderFrameIndex(uint64_t FrameIndex);
uint64_t GetGameFrameIndex();
uint64_t GetRenderFrameIndex();
