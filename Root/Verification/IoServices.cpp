#include "Core/IO/AtomicFileWriter.h"
#include "Core/IO/Base64.h"
#include "Core/IO/ImageBuffer.h"
#include "Assets/Guid.h"
#include <stb_image.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main()
{
    int Failures = 0;
    const auto Expect = [&](bool bCondition, const char* Message)
    {
        if (!bCondition)
        {
            ++Failures;
            std::cerr << Message << '\n';
        }
    };
    const std::string Plain[] = {"", "f", "fo", "foo", "foob", "fooba", "foobar"};
    const std::string Encoded[] = {"", "Zg==", "Zm8=", "Zm9v", "Zm9vYg==", "Zm9vYmE=", "Zm9vYmFy"};
    for (size_t Index = 0; Index < 7; ++Index)
    {
        Expect(Base64::Encode(reinterpret_cast<const uint8_t*>(Plain[Index].data()), Plain[Index].size()) == Encoded[Index],
            "Base64 encoding differs from the standard vector");
        std::vector<uint8_t> Decoded;
        Expect(Base64::Decode(Encoded[Index], Decoded) && std::string(Decoded.begin(), Decoded.end()) == Plain[Index],
            "Base64 decoding differs from the standard vector");
    }
    for (const auto* Invalid : {"Zg=", "=g==", "Z===", "Zg==Zg==", "Zg?=", "Zh==", "Zm9="})
    {
        std::vector<uint8_t> Decoded;
        Expect(!Base64::Decode(Invalid, Decoded) && Decoded.empty(), "Invalid Base64 was accepted");
    }
    const uint8_t Premultiplied[] = {64, 32, 16, 128, 77, 55, 33, 0};
    const auto Image = ImageBuffer::FromRgba(Premultiplied, 2, 1, 8, true);
    Expect(Image.Pixels == std::vector<uint8_t>({128, 64, 32, 128, 0, 0, 0, 0}),
        "Premultiplied pixels were not converted to straight alpha");
    const auto Resized = Image.ResizeToFit(1, 1);
    Expect(Resized.Width == 1 && Resized.Height == 1 && Resized.Pixels == std::vector<uint8_t>({128, 64, 32, 64}),
        "Resizing introduced a transparent-edge color fringe");
    const auto Png = Image.EncodePng();
    int Width = 0;
    int Height = 0;
    int Channels = 0;
    uint8_t* Pixels = stbi_load_from_memory(Png.data(), static_cast<int>(Png.size()), &Width, &Height, &Channels, 4);
    Expect(Pixels && Width == Image.Width && Height == Image.Height, "Encoded PNG cannot be decoded");
    if (Pixels)
    {
        Expect(std::vector<uint8_t>(Pixels, Pixels + Width * Height * 4) == Image.Pixels,
            "PNG round trip changed pixel values");
        stbi_image_free(Pixels);
    }
    const auto Directory = std::filesystem::temp_directory_path() / ("SakuraIo-" + Guid::Generate().ToString());
    const auto Filename = Directory / std::filesystem::u8path(u8"проверка-日本語.bin");
    std::string Error;
    const std::string Contents("before\0after", 12);
    Expect(AtomicFileWriter::WriteText(Filename, Contents, Error), "Atomic file creation failed");
    std::ifstream Input(Filename, std::ios::binary);
    const std::string Read((std::istreambuf_iterator<char>(Input)), std::istreambuf_iterator<char>());
    Input.close();
    Expect(Read == Contents, "Atomic writing changed binary data or Unicode path");
    Expect(AtomicFileWriter::WriteText(Filename, "replacement", Error), "Atomic replacement failed");
    Expect(AtomicFileWriter::WriteText(Filename, "", Error), "Empty file replacement failed");
    Expect(std::filesystem::file_size(Filename) == 0, "Empty replacement left old bytes");
    std::filesystem::remove_all(Directory);
    return Failures;
}
