#include "Core/IO/AtomicFileWriter.h"
#include "Assets/Guid.h"

#include <cerrno>
#include <cstdio>
#include <system_error>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

bool AtomicFileWriter::WriteText(
    const std::filesystem::path& Destination,
    const std::string& Contents,
    std::string& OutError)
{
    OutError.clear();
    if (Destination.empty())
    {
        OutError = "Output path is empty";
        return false;
    }
    std::error_code Error;
    const auto AbsoluteDestination = std::filesystem::absolute(Destination, Error);
    if (Error)
    {
        OutError = Error.message();
        return false;
    }
    std::filesystem::create_directories(AbsoluteDestination.parent_path(), Error);
    if (Error)
    {
        OutError = Error.message();
        return false;
    }
    auto Temporary = AbsoluteDestination;
    Temporary += "." + Guid::Generate().ToString() + ".tmp";
#ifdef _WIN32
    FILE* Output = nullptr;
    _wfopen_s(&Output, Temporary.c_str(), L"wbx");
#else
    FILE* Output = std::fopen(Temporary.c_str(), "wbx");
#endif
    if (!Output)
    {
        OutError = std::error_code(errno, std::generic_category()).message();
        return false;
    }
    bool bSuccess = std::fwrite(Contents.data(), 1, Contents.size(), Output) == Contents.size();
    if (bSuccess)
    {
        bSuccess = std::fflush(Output) == 0;
    }
    if (bSuccess)
    {
#ifdef _WIN32
        bSuccess = _commit(_fileno(Output)) == 0;
#else
        bSuccess = fsync(fileno(Output)) == 0;
#endif
    }
    if (!bSuccess)
    {
        OutError = std::error_code(errno, std::generic_category()).message();
    }
    if (std::fclose(Output) != 0 && bSuccess)
    {
        bSuccess = false;
        OutError = std::error_code(errno, std::generic_category()).message();
    }
    if (bSuccess)
    {
#ifdef _WIN32
        bSuccess = MoveFileExW(Temporary.c_str(), AbsoluteDestination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        if (!bSuccess)
        {
            OutError = std::error_code(GetLastError(), std::system_category()).message();
        }
#else
        std::filesystem::rename(Temporary, AbsoluteDestination, Error);
        bSuccess = !Error;
        if (!bSuccess)
        {
            OutError = Error.message();
        }
#endif
    }
    if (!bSuccess)
    {
        std::filesystem::remove(Temporary, Error);
    }
    return bSuccess;
}
