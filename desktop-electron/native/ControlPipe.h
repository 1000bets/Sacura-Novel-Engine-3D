#pragma once
#include <windows.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <cstring>
#include <stdexcept>
#include <algorithm>

// Independent, length-prefixed control channel; engine logs cannot corrupt replies.
class ControlPipe {
public:
    explicit ControlPipe(const char* Name) {
        Pipe=CreateFileA(Name,GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,0,nullptr);
        if(Pipe==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot connect Bridge pipe");
    }
    ~ControlPipe(){CloseHandle(Pipe);}
    bool Read(std::vector<nlohmann::json>& Out) {
        DWORD Available=0;if(!PeekNamedPipe(Pipe,nullptr,0,nullptr,&Available,nullptr))return false;
        if(Available) {
            char Chunk[16384];DWORD N=0;
            if(!ReadFile(Pipe,Chunk,std::min<DWORD>(Available,sizeof Chunk),&N,nullptr))return false;
            Pending.insert(Pending.end(),Chunk,Chunk+N);
        }
        size_t Offset=0;
        while(Pending.size()-Offset>=4 && Out.size()<16) {
            uint32_t N=0;std::memcpy(&N,Pending.data()+Offset,4);
            if(N>65536||N==0)throw std::runtime_error("Bridge request size limit");
            if(Pending.size()-Offset<4+N)break;
            // Syntax errors receive a protocol error; they do not terminate Engine.
            auto J=nlohmann::json::parse(Pending.begin()+Offset+4,Pending.begin()+Offset+4+N,nullptr,false);
            Out.push_back(J.is_discarded()?nlohmann::json(nullptr):std::move(J));Offset+=4+N;
        }
        Pending.erase(Pending.begin(),Pending.begin()+Offset);return true;
    }
    bool Send(const nlohmann::json& J) {
        auto Text=J.dump();if(Text.size()>8*1024*1024)throw std::runtime_error("Bridge response size limit");
        uint32_t N=static_cast<uint32_t>(Text.size());std::vector<char> Frame(4+N);
        std::memcpy(Frame.data(),&N,4);std::memcpy(Frame.data()+4,Text.data(),N);
        DWORD Written=0;return WriteFile(Pipe,Frame.data(),static_cast<DWORD>(Frame.size()),&Written,nullptr)&&Written==Frame.size();
    }
private:
    HANDLE Pipe=INVALID_HANDLE_VALUE;
    std::vector<char> Pending;
};
