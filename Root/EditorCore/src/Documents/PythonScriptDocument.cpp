#include "Documents/PythonScriptDocument.h"
#include "Core/IO/AtomicFileWriter.h"
#include <fstream>
#include <iterator>
#include <nlohmann/json.hpp>

bool PythonScriptDocument::Open(const AssetKey& Asset, std::string& Error)
{
    AssetRegistryEntry Resolved;
    if (Asset.HasSubAsset() || !Registry.TryResolveKey(Asset, Resolved)
        || Resolved.Metadata.Type != PythonScriptAssetType)
    {
        Error = "Python script asset not found";
        return false;
    }
    std::ifstream File(std::filesystem::u8path(Resolved.AbsolutePath), std::ios::binary);
    if (!File)
    {
        Error = "Failed to open Python source";
        return false;
    }
    const std::string Bytes((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    if (File.bad())
    {
        Error = "Failed to read Python source";
        return false;
    }
    try
    {
        nlohmann::json(Bytes).dump();
    }
    catch (const nlohmann::json::type_error&)
    {
        Error = "Python source must use UTF-8 encoding without null characters";
        return false;
    }
    if (Bytes.find('\0') != std::string::npos)
    {
        Error = "Python source must use UTF-8 encoding without null characters";
        return false;
    }
    Saved = Bytes;
    History = {Saved};
    Position = 0;
    Entry = std::move(Resolved);
    return true;
}

bool PythonScriptDocument::Edit(const std::string& Source, std::string& Error)
{
    if (Entry.Mount != AssetMount::Game)
    {
        Error = "Engine scripts are read-only";
        return false;
    }
    if (Source != GetSource())
    {
        History.resize(Position + 1);
        History.push_back(Source);
        ++Position;
    }
    return true;
}

void PythonScriptDocument::Undo()
{
    if (Position > 0)
    {
        --Position;
    }
}

void PythonScriptDocument::Redo()
{
    if (Position + 1 < History.size())
    {
        ++Position;
    }
}

bool PythonScriptDocument::Save(std::string& Error)
{
    AssetRegistryEntry Current;
    if (Entry.Mount != AssetMount::Game || !Registry.TryGetById(Entry.Metadata.Guid, Current))
    {
        Error = "Python script is unavailable or read-only";
        return false;
    }
    std::ifstream File(std::filesystem::u8path(Current.AbsolutePath), std::ios::binary);
    if (!File)
    {
        Error = "Failed to open Python source";
        return false;
    }
    const std::string Bytes((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
    if (File.bad() || Bytes != Saved)
    {
        Error = "Python script changed on disk; preserve your draft and reload before saving";
        return false;
    }
    File.close();
    if (!AtomicFileWriter::WriteText(std::filesystem::u8path(Current.AbsolutePath), GetSource(), Error))
    {
        return false;
    }
    Saved = GetSource();
    Entry = std::move(Current);
    const auto Result = Registry.UpdateSourceFingerprint(Entry.Metadata.Guid, ContentHash::FromBytes(Saved.data(), Saved.size()));
    if (Result.HasError())
    {
        Error = Result.Message;
        return false;
    }
    return true;
}
