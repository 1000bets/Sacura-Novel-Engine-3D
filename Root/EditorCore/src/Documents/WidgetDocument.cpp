#include "Documents/WidgetDocument.h"
#include "UI/WidgetDefinition.h"
#include "UI/WidgetRuntime.h"
#include "Core/IO/AtomicFileWriter.h"
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <cmath>

using Json = nlohmann::json;

bool WidgetDocument::Validate(const Json& Document, std::string& Error)
{
    return WidgetDefinition::Validate(Document, Error);
}

bool WidgetDocument::Read(const std::filesystem::path& Path, Json& Document, std::string& Error)
{
    return WidgetDefinition::Read(Path, Document, Error);
}

bool WidgetDocument::Open(const AssetKey& Asset, std::string& Error)
{
    AssetRegistryEntry Resolved;
    if (Asset.HasSubAsset() || !Registry.TryResolveKey(Asset, Resolved) || Resolved.Metadata.Type != WidgetAssetType)
    {
        Error = "Widget asset not found";
        return false;
    }
    Json Document;
    ContentHash Current;
    AssetDiagnostic Diagnostic;
    if (!Read(std::filesystem::u8path(Resolved.AbsolutePath), Document, Error)
        || !ContentHash::TryHashFile(Resolved.AbsolutePath, Current, Diagnostic))
    {
        return false;
    }
    Entry = std::move(Resolved);
    Saved = std::move(Document);
    History = {Saved};
    Position = 0;
    Fingerprint = Current;
    return true;
}

bool WidgetDocument::Edit(const Json& Document, std::string& Error)
{
    if (Entry.Mount != AssetMount::Game)
    {
        Error = "Engine widgets are read-only";
        return false;
    }
    if (!Validate(Document, Error) || !WidgetRuntime::ValidateController(Document, Error))
    {
        return false;
    }
    if (Document != GetDocument())
    {
        History.resize(Position + 1);
        History.push_back(Document);
        ++Position;
    }
    return true;
}

void WidgetDocument::Undo()
{
    if (CanUndo())
    {
        --Position;
    }
}

void WidgetDocument::Redo()
{
    if (CanRedo())
    {
        ++Position;
    }
}

bool WidgetDocument::Save(std::string& Error)
{
    AssetRegistryEntry Current;
    ContentHash CurrentFingerprint;
    AssetDiagnostic Diagnostic;
    if (Entry.Mount != AssetMount::Game || !Registry.TryGetById(Entry.Metadata.Guid, Current))
    {
        Error = "Widget is unavailable or read-only";
        return false;
    }
    if (!ContentHash::TryHashFile(Current.AbsolutePath, CurrentFingerprint, Diagnostic) || CurrentFingerprint != Fingerprint)
    {
        Error = "Widget changed on disk; preserve your draft and reload before saving";
        return false;
    }
    const auto Bytes = GetDocument().dump(2) + "\n";
    if (Bytes.size() > 524288)
    {
        Error = "Widget exceeds 512 KiB";
        return false;
    }
    if (!AtomicFileWriter::WriteText(std::filesystem::u8path(Current.AbsolutePath), Bytes, Error))
    {
        return false;
    }
    Saved = GetDocument();
    Fingerprint = ContentHash::FromBytes(Bytes.data(), Bytes.size());
    Entry = std::move(Current);
    const auto Updated = Registry.UpdateSourceFingerprint(Entry.Metadata.Guid, Fingerprint);
    Error = Updated.Message;
    return !Updated.HasError();
}
