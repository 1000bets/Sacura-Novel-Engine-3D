#pragma once
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// Host transport contract. No Diligent/native graphics interfaces cross RHI.
// WindowsNtHandle is duplicated into ConsumerProcess, not usable in the producer.
struct SharedGpuFrame
{
    uint64_t Token = 0, WindowsNtHandle = 0, FrameIndex = 0, ReadyMicroseconds = 0;
    uint32_t Width = 0, Height = 0;
    std::shared_ptr<std::atomic<bool>> Released;
};
class SharedGpuFrameStream
{
public:
    explicit SharedGpuFrameStream(uint32_t Process) : ConsumerProcess(Process) {}
    const uint32_t ConsumerProcess;
    std::atomic<bool> Enabled{true};
    std::atomic<uint64_t> Dropped{0};
    std::atomic<uint64_t> TextureCount{0}, TextureBytes{0};
    void Publish(SharedGpuFrame Frame) { std::lock_guard<std::mutex> Lock(Mutex); Pending.push_back(std::move(Frame)); }
    std::vector<SharedGpuFrame> Take() { std::lock_guard<std::mutex> Lock(Mutex); auto Result = std::move(Pending); Pending.clear(); return Result; }
    void Fail(std::string Message) { std::lock_guard<std::mutex> Lock(Mutex); Error = std::move(Message); Enabled = false; }
    std::string GetError() const { std::lock_guard<std::mutex> Lock(Mutex); return Error; }
private:
    mutable std::mutex Mutex;
    std::vector<SharedGpuFrame> Pending;
    std::string Error;
};
