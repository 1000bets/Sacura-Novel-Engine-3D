#include "Localization/EditorLocalization.h"

#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QRegularExpression>
#include <algorithm>

EditorLocalization& EditorLocalization::Get()
{
    static EditorLocalization Instance;
    return Instance;
}

EditorLocalization::EditorLocalization()
{
    Q_INIT_RESOURCE(editor_localization);
    QFile Catalog(":/editor-localization/Editor.json");
    if (Catalog.open(QIODevice::ReadOnly))
    {
        Entries = QJsonDocument::fromJson(Catalog.readAll()).object().value("entries").toObject();
        for (auto Entry = Entries.begin(); Entry != Entries.end(); ++Entry)
        {
            for (const auto& Alias : Entry.value().toObject().value("aliases").toArray())
            {
                Aliases.insert(Alias.toString(), Entry.key());
            }
        }
    }
    QSettings Settings("SakuraNovel", "SakuraNovel Studio");
    CurrentLanguage = Settings.value("Editor/Language", "en").toString();
    Overrides = QJsonDocument::fromJson(Settings.value("Editor/TranslationOverrides").toByteArray()).object();
    if (CurrentLanguage != "ru")
    {
        CurrentLanguage = "en";
    }
}

QString EditorLocalization::GetLanguage() const
{
    return CurrentLanguage;
}

bool EditorLocalization::SetLanguage(const QString& Language)
{
    QString Error;
    return ApplyPreferences({{"language", Language}}, Error);
}

QString EditorLocalization::Text(const QString& Identifier, const QString& Fallback) const
{
    const auto Entry = Entries.value(Identifier).toObject();
    if (Entry.isEmpty())
    {
        return Fallback;
    }
    const QString Source = Entry.value("source").toString();
    if (CurrentLanguage == "en")
    {
        return Source;
    }
    const auto Override = Overrides.value(Identifier).toObject();
    if (Override.value("source").toString() == Source && !Override.value("text").toString().isEmpty())
    {
        return Override.value("text").toString();
    }
    const QString Translation = Entry.value("translations").toObject().value(CurrentLanguage).toString();
    if (Translation.isEmpty())
    {
        return Source;
    }
    return Translation;
}

QString EditorLocalization::Caption(const QString& Source) const
{
    return Text(Aliases.value(Source), Source);
}

QString EditorLocalization::translate(const char*, const char* Source, const char*, int) const
{
    const QString Original = QString::fromUtf8(Source);
    const auto Found = Aliases.constFind(Original);
    if (Found == Aliases.constEnd())
    {
        return {};
    }
    return Text(Found.value(), Original);
}

bool EditorLocalization::isEmpty() const
{
    return Entries.isEmpty();
}

QJsonObject EditorLocalization::Preferences() const
{
    return {{"language", CurrentLanguage}, {"overrides", Overrides}};
}

bool EditorLocalization::ApplyPreferences(const QJsonObject& Values, QString& Error)
{
    const QString Language = Values.value("language").toString(CurrentLanguage);
    if ((Values.contains("language") && !Values.value("language").isString()) || (Language != "en" && Language != "ru"))
    {
        Error = "Language must be en or ru";
        return false;
    }
    QJsonObject Updated = Overrides;
    if (Values.contains("overrides"))
    {
        if (!Values.value("overrides").isObject())
        {
            Error = "Translation overrides must be an object";
            return false;
        }
        Updated = Values.value("overrides").toObject();
        auto Slots = [](const QString& Text)
        {
            QStringList Result;
            auto Matches = QRegularExpression("\\{[a-zA-Z_0-9]+\\}|%[1-9][0-9]*").globalMatch(Text);
            while (Matches.hasNext())
            {
                Result.push_back(Matches.next().captured());
            }
            Result.sort();
            return Result;
        };
        for (auto Entry = Updated.begin(); Entry != Updated.end(); ++Entry)
        {
            const auto Source = Entries.value(Entry.key()).toObject();
            const auto Value = Entry.value().toObject();
            const QString Original = Source.value("source").toString();
            const QString Translation = Value.value("text").toString();
            if (Source.isEmpty() || Value.value("source").toString() != Original || !Value.value("text").isString()
                || Translation.size() > 16384 || (!Translation.isEmpty() && Slots(Original) != Slots(Translation)))
            {
                Error = "Invalid or outdated translation entry: " + Entry.key();
                return false;
            }
        }
    }
    QSettings Settings("SakuraNovel", "SakuraNovel Studio");
    Settings.setValue("Editor/Language", Language);
    Settings.setValue("Editor/TranslationOverrides", QJsonDocument(Updated).toJson(QJsonDocument::Compact));
    Settings.sync();
    if (Settings.status() != QSettings::NoError)
    {
        Error = "Failed to save editor localization preferences";
        return false;
    }
    Overrides = Updated;
    CurrentLanguage = Language;
    RefreshBindings();
    if (QCoreApplication::instance())
    {
        QEvent Changed(QEvent::LanguageChange);
        QCoreApplication::sendEvent(QCoreApplication::instance(), &Changed);
    }
    return true;
}

void EditorLocalization::Bind(QObject* Owner, std::function<void()> Refresh)
{
    Bindings.erase(std::remove_if(Bindings.begin(), Bindings.end(), [](const Binding& Entry)
    {
        return Entry.Owner.isNull();
    }), Bindings.end());
    Refresh();
    Bindings.push_back({Owner, std::move(Refresh)});
}

void EditorLocalization::RefreshBindings()
{
    auto Current = std::move(Bindings);
    Bindings.clear();
    for (auto& Entry : Current)
    {
        if (Entry.Owner)
        {
            Entry.Refresh();
            Bindings.push_back(std::move(Entry));
        }
    }
}
