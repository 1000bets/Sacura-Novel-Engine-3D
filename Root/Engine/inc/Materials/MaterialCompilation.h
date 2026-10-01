#pragma once
#include <atomic>
#include <mutex>
#include <string>

enum class MaterialCompilationState { Queued, Compiling, Ready, Failed, Cancelled };

struct MaterialCompilation
{
    std::atomic<MaterialCompilationState> State{MaterialCompilationState::Queued};
    mutable std::mutex Mutex;
    std::string Diagnostic;
    std::string GetDiagnostic() const
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        return Diagnostic;
    }
};
