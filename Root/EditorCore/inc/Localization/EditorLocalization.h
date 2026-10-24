#pragma once

#include <QJsonObject>
#include <QHash>
#include <QString>
#include <QTranslator>
#include <QPointer>
#include <functional>
#include <vector>

class EditorLocalization : public QTranslator
{
public:
    static EditorLocalization& Get();
    QString GetLanguage() const;
    bool SetLanguage(const QString& Language);
    QString Text(const QString& Identifier, const QString& Fallback = {}) const;
    QString Caption(const QString& Source) const;
    QJsonObject Preferences() const;
    bool ApplyPreferences(const QJsonObject& Values, QString& Error);
    void Bind(QObject* Owner, std::function<void()> Refresh);
    QString translate(const char* Context, const char* Source, const char* Disambiguation = nullptr, int Count = -1) const override;
    bool isEmpty() const override;

private:
    EditorLocalization();
    QJsonObject Entries;
    QHash<QString, QString> Aliases;
    QString CurrentLanguage;
    QJsonObject Overrides;
    struct Binding
    {
        QPointer<QObject> Owner;
        std::function<void()> Refresh;
    };
    std::vector<Binding> Bindings;
    void RefreshBindings();
};
