#pragma once

#include "Core/Threading/ThreadContext.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <map>
#include <string>

class CpuProfileScope
{
public:
    explicit CpuProfileScope(const char* InName) : Name(InName)
    {
        static const bool bEnabled = []()
        {
#if defined(_MSC_VER)
            char* Value = nullptr;
            size_t Length = 0;
            _dupenv_s(&Value, &Length, "SACURA_CPU_PROFILE");
            const bool bRequested = Value && std::string(Value) == "1";
            std::free(Value);
            return bRequested;
#else
            const char* Value = std::getenv("SACURA_CPU_PROFILE");
            return Value && std::string(Value) == "1";
#endif
        }();
        bActive = bEnabled;
        if (bActive)
        {
            Start = std::chrono::steady_clock::now();
        }
    }

    ~CpuProfileScope()
    {
        if (!bActive)
        {
            return;
        }
        const double Milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
        struct Measurement
        {
            double Total = 0.0;
            double Maximum = 0.0;
            unsigned Count = 0;
        };
        static thread_local std::map<std::string, Measurement> Measurements;
        auto& Value = Measurements[Name];
        Value.Total += Milliseconds;
        Value.Maximum = std::max(Value.Maximum, Milliseconds);
        if (++Value.Count == 120)
        {
            PrintString(std::string("CPU profile: ") + Name + " meanMs=" + std::to_string(Value.Total / Value.Count)
                + " maxMs=" + std::to_string(Value.Maximum) + " calls=" + std::to_string(Value.Count));
            Value = {};
        }
    }

private:
    const char* Name;
    bool bActive = false;
    std::chrono::steady_clock::time_point Start;
};
