#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class Base64
{
public:
    static std::string Encode(const uint8_t* Bytes, size_t Size)
    {
        constexpr char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string Result;
        Result.reserve((Size + 2) / 3 * 4);
        for (size_t Offset = 0; Offset < Size; Offset += 3)
        {
            uint32_t Value = static_cast<uint32_t>(Bytes[Offset]) << 16;
            if (Offset + 1 < Size)
            {
                Value |= static_cast<uint32_t>(Bytes[Offset + 1]) << 8;
            }
            if (Offset + 2 < Size)
            {
                Value |= Bytes[Offset + 2];
            }
            Result.push_back(Alphabet[(Value >> 18) & 63]);
            Result.push_back(Alphabet[(Value >> 12) & 63]);
            if (Offset + 1 < Size)
            {
                Result.push_back(Alphabet[(Value >> 6) & 63]);
            }
            else
            {
                Result.push_back('=');
            }
            if (Offset + 2 < Size)
            {
                Result.push_back(Alphabet[Value & 63]);
            }
            else
            {
                Result.push_back('=');
            }
        }
        return Result;
    }

    static bool Decode(std::string_view Text, std::vector<uint8_t>& Bytes)
    {
        Bytes.clear();
        if (Text.size() % 4 != 0)
        {
            return false;
        }
        constexpr std::string_view Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        Bytes.reserve(Text.size() / 4 * 3);
        for (size_t Offset = 0; Offset < Text.size(); Offset += 4)
        {
            uint32_t Value = 0;
            size_t Padding = 0;
            for (size_t Digit = 0; Digit < 4; ++Digit)
            {
                Value <<= 6;
                const char Character = Text[Offset + Digit];
                if (Character == '=')
                {
                    if (Digit < 2 || Offset + 4 != Text.size())
                    {
                        Bytes.clear();
                        return false;
                    }
                    ++Padding;
                }
                else
                {
                    const size_t Position = Alphabet.find(Character);
                    if (Padding != 0 || Position == std::string_view::npos)
                    {
                        Bytes.clear();
                        return false;
                    }
                    Value |= static_cast<uint32_t>(Position);
                }
            }
            if ((Padding == 1 && (Value & 255) != 0) || (Padding == 2 && (Value & 65535) != 0))
            {
                Bytes.clear();
                return false;
            }
            Bytes.push_back(static_cast<uint8_t>(Value >> 16));
            if (Padding < 2)
            {
                Bytes.push_back(static_cast<uint8_t>(Value >> 8));
            }
            if (Padding == 0)
            {
                Bytes.push_back(static_cast<uint8_t>(Value));
            }
        }
        return true;
    }
};
