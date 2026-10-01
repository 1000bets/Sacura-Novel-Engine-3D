#pragma once

#include <filesystem>
#include <string>

class AtomicFileWriter
{
public:
    static bool WriteText(const std::filesystem::path& Destination, const std::string& Contents, std::string& OutError);
};
