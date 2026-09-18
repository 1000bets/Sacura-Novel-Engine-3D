#include "Assets/ContentHash.h"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

std::string ContentHash::ToHex() const
{
    std::ostringstream Stream;
    Stream << std::hex << std::setfill('0') << std::setw(16) << Value;
    return Stream.str();
}

ContentHash ContentHash::FromBytes(const void* Data, size_t ByteCount)
{
    ContentHash Hash{};
    Hash.Value = 1469598103934665603ull;
    const auto* Bytes = static_cast<const uint8_t*>(Data);
    for (size_t Index = 0; Index < ByteCount; ++Index)
    {
        Hash.Value ^= Bytes[Index];
        Hash.Value *= 1099511628211ull;
    }
    if (Hash.Value == 0)
    {
        Hash.Value = 1;
    }
    return Hash;
}

bool ContentHash::TryHashFile(const std::string& AbsolutePath, ContentHash& OutHash, AssetDiagnostic& OutError)
{
    std::ifstream Stream(AbsolutePath, std::ios::binary);
    if (!Stream)
    {
        OutError = AssetDiagnostic::Fail(AssetErrorCode::NotFound, "ContentHash", "Failed to open file for hashing", {}, AbsolutePath);
        return false;
    }

    std::vector<char> Buffer(1 << 16);
    ContentHash Hash = FromBytes(nullptr, 0);
    Hash.Value = 1469598103934665603ull;
    while (Stream)
    {
        Stream.read(Buffer.data(), static_cast<std::streamsize>(Buffer.size()));
        const std::streamsize Count = Stream.gcount();
        if (Count <= 0)
        {
            break;
        }
        for (std::streamsize Index = 0; Index < Count; ++Index)
        {
            Hash.Value ^= static_cast<uint8_t>(Buffer[static_cast<size_t>(Index)]);
            Hash.Value *= 1099511628211ull;
        }
    }
    if (Hash.Value == 0)
    {
        Hash.Value = 1;
    }
    OutHash = Hash;
    OutError = AssetDiagnostic::Ok();
    return true;
}
