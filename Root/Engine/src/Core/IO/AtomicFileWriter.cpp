#include "Core/IO/AtomicFileWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

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

    const QString Filename = QString::fromStdWString(Destination.wstring());
    if (!QDir().mkpath(QFileInfo(Filename).absolutePath()))
    {
        OutError = "Failed to create output directories";
        return false;
    }

    QSaveFile Output(Filename);
    Output.setDirectWriteFallback(false);
    if (!Output.open(QIODevice::WriteOnly))
    {
        OutError = Output.errorString().toStdString();
        return false;
    }

    if (Output.write(Contents.data(), static_cast<qint64>(Contents.size())) != static_cast<qint64>(Contents.size()))
    {
        OutError = Output.errorString().toStdString();
        Output.cancelWriting();
        return false;
    }

    if (!Output.commit())
    {
        OutError = Output.errorString().toStdString();
        return false;
    }

    return true;
}
