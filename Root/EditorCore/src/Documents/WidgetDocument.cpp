#include "Documents/WidgetDocument.h"
#include "UI/WidgetLayout.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace
{
bool Write(const QString &Path, const QJsonObject &Data, QString &Error)
{
    QSaveFile File(Path);
    const auto Bytes = QJsonDocument(Data).toJson();
    if (!File.open(QIODevice::WriteOnly) || File.write(Bytes) != Bytes.size() || !File.commit())
    {
        Error = File.errorString();
        return false;
    }
    return true;
}
} // namespace
bool WidgetDocument::Open(const QString &File, QString &Error)
{
    auto Candidate = WidgetLayout::Defaults();
    const bool Exists = QFileInfo::exists(File);
    if (Exists && !WidgetLayout::Read(File, Candidate, Error))
        return false;
    QFile Input(File);
    QByteArray Bytes;
    if (Exists)
    {
        if (!Input.open(QIODevice::ReadOnly))
        {
            Error = Input.errorString();
            return false;
        }
        Bytes = Input.readAll();
    }
    Data = Saved = Candidate;
    Path = File;
    DiskContents = Bytes;
    Existed = Exists;
    Past.clear();
    Future.clear();
    if (Changed)
        Changed();
    return true;
}
bool WidgetDocument::Apply(QJsonObject Candidate, QString &Error)
{
    if (!IsOpen())
    {
        Error = "Библиотека UI не открыта.";
        return false;
    }
    if (!WidgetLayout::Validate(Candidate, Error))
        return false;
    if (Candidate == Data)
        return true;
    Past.append(Data);
    if (Past.size() > 100)
        Past.removeFirst();
    Future.clear();
    Data = Candidate;
    if (Changed)
        Changed();
    return true;
}
void WidgetDocument::Undo()
{
    if (Past.isEmpty())
        return;
    Future.append(Data);
    Data = Past.takeLast();
    if (Changed)
        Changed();
}
void WidgetDocument::Redo()
{
    if (Future.isEmpty())
        return;
    Past.append(Data);
    Data = Future.takeLast();
    if (Changed)
        Changed();
}
bool WidgetDocument::Save(QString &Error)
{
    if (!IsOpen() || !WidgetLayout::Validate(Data, Error))
        return false;
    QFile Existing(Path);
    if (QFileInfo::exists(Path) != Existed ||
        (Existed && (!Existing.open(QIODevice::ReadOnly) || Existing.readAll() != DiskContents)))
    {
        Error = "Файл UI изменён вне редактора. Экспортируйте виджеты перед повторным открытием проекта.";
        return false;
    }
    Existing.close();
    if (!QDir().mkpath(QFileInfo(Path).absolutePath()))
    {
        Error = "Не удалось создать папку UI.";
        return false;
    }
    if (!Write(Path, Data, Error))
        return false;
    Saved = Data;
    DiskContents = QJsonDocument(Data).toJson();
    Existed = true;
    if (Changed)
        Changed();
    return true;
}
bool WidgetDocument::ExportWidget(const QString &Id, const QString &File, QString &Error, QJsonObject Metadata) const
{
    for (const auto &Value : Data["widgets"].toArray())
        if (Value.toObject()["id"] == Id)
        {
            const auto W = Value.toObject();
            Metadata["id"] = Id;
            Metadata["version"] = "1.0.0";
            Metadata["name"] = W["name"];
            if (Metadata["author"].toString().trimmed().isEmpty())
                Metadata["author"] = "Не указан";
            if (Metadata["license"].toString().trimmed().isEmpty())
                Metadata["license"] = "Не указана";
            Metadata["sources"] = W["sources"].toArray();
            return Write(
                File,
                QJsonObject{{"format", "sacura-authoring-package"},
                            {"formatVersion", 1},
                            {"metadata", Metadata},
                            {"compatibility",
                             QJsonObject{{"projectSchemaVersion", 1},
                                         {"requiredCapabilities",
                                          QJsonArray{QJsonObject{{"id", "native.ui.widgets"}, {"version", 1}}}},
                                         {"requiredEngineModules", QJsonArray{}}}},
                            {"roots", QJsonObject{{"kind", "widget"}, {"entries", QJsonArray{"/content/widgets/0"}}}},
                            {"inputs", QJsonArray{}},
                            {"references", QJsonArray{}},
                            {"assets", QJsonArray{}},
                            {"content", QJsonObject{{"widgets", QJsonArray{W}}}}},
                Error);
        }
    Error = "Виджет не найден.";
    return false;
}
bool WidgetDocument::ReadPackage(const QString &Path, QJsonObject &Widget, QString &Error)
{
    QFile File(Path);
    if (!File.open(QIODevice::ReadOnly))
    {
        Error = File.errorString();
        return false;
    }
    if (File.size() > 32 * 1024 * 1024)
    {
        Error = "Пакет превышает 32 МБ.";
        return false;
    }
    const auto Package = QJsonDocument::fromJson(File.readAll()).object();
    const auto Content = Package["content"].toObject(), Compatibility = Package["compatibility"].toObject(),
               Metadata = Package["metadata"].toObject();
    if (Package["format"] != "sacura-authoring-package" || Package["formatVersion"] != 1 || Content.size() != 1 ||
        Content["widgets"].toArray().size() != 1 || Compatibility["projectSchemaVersion"] != 1 ||
        Compatibility["requiredCapabilities"] != QJsonArray{QJsonObject{{"id", "native.ui.widgets"}, {"version", 1}}} ||
        Compatibility["requiredEngineModules"] != QJsonArray{} || Package["inputs"] != QJsonArray{} ||
        Package["references"] != QJsonArray{} || Package["assets"] != QJsonArray{} ||
        Package["roots"] != QJsonObject{{"kind", "widget"}, {"entries", QJsonArray{"/content/widgets/0"}}} ||
        Metadata["id"].toString().isEmpty() || Metadata["name"].toString().isEmpty() ||
        Metadata["author"].toString().isEmpty() || Metadata["license"].toString().isEmpty() ||
        Metadata["version"] != "1.0.0" || !Metadata["sources"].isArray())
    {
        Error = "Пакет требует возможностей вне нативного профиля виджетов UI.";
        return false;
    }
    auto Candidate = Content["widgets"].toArray().first().toObject();
    if (Candidate["name"].toString().trimmed().isEmpty())
    {
        Error = "Укажите название виджета.";
        return false;
    }
    auto Library = WidgetLayout::Defaults();
    auto Ws = Library["widgets"].toArray();
    // Validate before showing the preview or changing the current document.
    Candidate["name"] = Candidate["name"].toString() + " · импорт";
    Ws.append(Candidate);
    Library["widgets"] = Ws;
    if (!WidgetLayout::Validate(Library, Error))
        return false;
    Widget = Content["widgets"].toArray().first().toObject();
    auto Sources = Metadata["sources"].toArray();
    auto Source = Metadata;
    Source.remove("sources");
    if (!Sources.contains(Source))
        Sources.append(Source);
    Widget["sources"] = Sources;
    return true;
}
bool WidgetDocument::ImportWidget(QJsonObject Widget, QString &Id, QString &Error)
{
    Widget = WidgetLayout::IndependentCopy(Widget);
    auto Candidate = Data;
    auto Ws = Candidate["widgets"].toArray();
    QStringList Names;
    for (auto W : Ws)
        Names.append(W.toObject()["name"].toString().toCaseFolded());
    auto Name = Widget["name"].toString();
    auto Unique = Name;
    int N = 2;
    while (Names.contains(Unique.toCaseFolded()))
        Unique = Name + QString(" (%1)").arg(N++);
    Widget["name"] = Unique;
    Id = Widget["id"].toString();
    Ws.append(Widget);
    Candidate["widgets"] = Ws;
    return Apply(Candidate, Error);
}
