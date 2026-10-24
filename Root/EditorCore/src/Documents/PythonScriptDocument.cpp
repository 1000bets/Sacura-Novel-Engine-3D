#include "Documents/PythonScriptDocument.h"
#include "Core/IO/AtomicFileWriter.h"
#include <QFile>
#include <QStringDecoder>

bool PythonScriptDocument::Open(const AssetKey& Asset, std::string& Error)
{
    AssetRegistryEntry Resolved;
    if (Asset.HasSubAsset() || !Registry.TryResolveKey(Asset, Resolved)
        || Resolved.Metadata.Type != PythonScriptAssetType)
    {
        Error = "Python script asset not found";
        return false;
    }
    QFile File(QString::fromStdString(Resolved.AbsolutePath));
    if (!File.open(QIODevice::ReadOnly))
    {
        Error = File.errorString().toStdString();
        return false;
    }
    const QByteArray Bytes = File.readAll();
    if (File.error() != QFileDevice::NoError)
    {
        Error = File.errorString().toStdString();
        return false;
    }
    QStringDecoder Decoder(QStringDecoder::Utf8);
    const QString Decoded = Decoder.decode(Bytes);
    if (Decoder.hasError() || Decoded.contains(QChar::Null))
    {
        Error = "Python source must use UTF-8 encoding without null characters";
        return false;
    }
    Saved = Bytes.toStdString();
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
    QFile File(QString::fromStdString(Current.AbsolutePath));
    if (!File.open(QIODevice::ReadOnly))
    {
        Error = File.errorString().toStdString();
        return false;
    }
    const QByteArray Bytes = File.readAll();
    if (File.error() != QFileDevice::NoError || Bytes.toStdString() != Saved)
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
