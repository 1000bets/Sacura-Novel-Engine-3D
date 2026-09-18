#include "Assets/Guid.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace
{
int HexValue(char Character)
{
    if (Character >= '0' && Character <= '9')
    {
        return Character - '0';
    }
    if (Character >= 'a' && Character <= 'f')
    {
        return Character - 'a' + 10;
    }
    if (Character >= 'A' && Character <= 'F')
    {
        return Character - 'A' + 10;
    }
    return -1;
}
}

bool Guid::IsValid() const
{
    for (uint8_t Byte : Bytes)
    {
        if (Byte != 0)
        {
            return true;
        }
    }
    return false;
}

std::string Guid::ToString() const
{
    std::ostringstream Stream;
    Stream << std::hex << std::setfill('0');
    for (size_t Index = 0; Index < Bytes.size(); ++Index)
    {
        Stream << std::setw(2) << static_cast<int>(Bytes[Index]);
        if (Index == 3 || Index == 5 || Index == 7 || Index == 9)
        {
            Stream << '-';
        }
    }
    return Stream.str();
}

Guid Guid::Generate()
{
    Guid Result{};
    thread_local std::mt19937_64 Generator{std::random_device{}()};
    std::uniform_int_distribution<int> Distribution(0, 255);
    for (uint8_t& Byte : Result.Bytes)
    {
        Byte = static_cast<uint8_t>(Distribution(Generator));
    }
    Result.Bytes[6] = static_cast<uint8_t>((Result.Bytes[6] & 0x0F) | 0x40);
    Result.Bytes[8] = static_cast<uint8_t>((Result.Bytes[8] & 0x3F) | 0x80);
    return Result;
}

bool Guid::TryParse(const std::string& Text, Guid& OutGuid)
{
    Guid Parsed{};
    size_t ByteIndex = 0;
    for (size_t Index = 0; Index < Text.size(); ++Index)
    {
        if (Text[Index] == '-')
        {
            continue;
        }
        if (Index + 1 >= Text.size() || ByteIndex >= Parsed.Bytes.size())
        {
            return false;
        }
        const int High = HexValue(Text[Index]);
        const int Low = HexValue(Text[Index + 1]);
        if (High < 0 || Low < 0)
        {
            return false;
        }
        Parsed.Bytes[ByteIndex++] = static_cast<uint8_t>((High << 4) | Low);
        ++Index;
    }
    if (ByteIndex != Parsed.Bytes.size())
    {
        return false;
    }
    OutGuid = Parsed;
    return OutGuid.IsValid();
}

bool Guid::operator==(const Guid& Other) const
{
    return Bytes == Other.Bytes;
}

bool Guid::operator!=(const Guid& Other) const
{
    return !(*this == Other);
}

size_t GuidHash::operator()(const Guid& Value) const
{
    size_t Hash = 1469598103934665603ull;
    for (uint8_t Byte : Value.Bytes)
    {
        Hash ^= Byte;
        Hash *= 1099511628211ull;
    }
    return Hash;
}
