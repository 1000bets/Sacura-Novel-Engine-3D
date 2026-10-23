#pragma once
#include "Rendering/SharedGpuFrame.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <unordered_map>
#include <vector>
#include <stdexcept>

// Full duplex named pipe: producer packets are 56 bytes; release ACK is uint64.
// No frame pixels, JSON serialization or graphics interfaces are used here.
class FramePipe
{
public:
    FramePipe(const char* Name, uint32_t Consumer) : Stream(std::make_shared<SharedGpuFrameStream>(Consumer))
    {
        Pipe = CreateFileA(Name, GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (Pipe == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot connect GPU frame pipe");
    }
    ~FramePipe() { Stream->Enabled = false; for (auto& Pair : InFlight) *Pair.second = true; CloseHandle(Pipe); }
    std::shared_ptr<SharedGpuFrameStream> Stream;
    uint64_t LastInputMicroseconds = 0;
    bool Pump()
    {
        DWORD Available = 0;
        if (!PeekNamedPipe(Pipe, nullptr, 0, nullptr, &Available, nullptr)) return false;
        if (Available) {
            std::array<char, 4096> Data; DWORD Read = 0;
            if (!ReadFile(Pipe, Data.data(), std::min<DWORD>(Available, Data.size()), &Read, nullptr)) return false;
            Acks.insert(Acks.end(), Data.data(), Data.data() + Read);
            size_t Consumed = 0;
            while (Acks.size() - Consumed >= 8) {
                uint64_t Token; std::memcpy(&Token, Acks.data() + Consumed, 8); Consumed += 8;
                const auto It = InFlight.find(Token);
                if (It != InFlight.end()) { It->second->store(true); InFlight.erase(It); }
            }
            Acks.erase(Acks.begin(), Acks.begin() + Consumed);
        }
        for (auto& Frame : Stream->Take()) {
            std::array<uint8_t, 56> Packet{};
            auto U32 = [&](size_t Offset, uint32_t Value) { std::memcpy(Packet.data() + Offset, &Value, 4); };
            auto U64 = [&](size_t Offset, uint64_t Value) { std::memcpy(Packet.data() + Offset, &Value, 8); };
            U32(0, 0x31464753); U32(4, Frame.Width); U32(8, Frame.Height);
            U64(16, Frame.Token); U64(24, Frame.WindowsNtHandle); U64(32, Frame.FrameIndex);
            U64(40, Frame.ReadyMicroseconds); U64(48, LastInputMicroseconds);
            InFlight[Frame.Token] = Frame.Released;
            DWORD Written = 0;
            if (!WriteFile(Pipe, Packet.data(), Packet.size(), &Written, nullptr) || Written != Packet.size()) return false;
        }
        return true;
    }
    size_t Outstanding() const { return InFlight.size(); }
private:
    HANDLE Pipe = INVALID_HANDLE_VALUE;
    std::vector<char> Acks;
    std::unordered_map<uint64_t, std::shared_ptr<std::atomic<bool>>> InFlight;
};
