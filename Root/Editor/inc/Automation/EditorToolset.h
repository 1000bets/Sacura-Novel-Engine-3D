#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <functional>

class EditorToolset
{
public:
    using Handler = std::function<QJsonObject(const QJsonObject&)>;
    bool Register(const QString& Name, const QString& Description, const QJsonObject& Properties,
                  const QJsonArray& Required, bool bReadOnly, Handler Execute);
    QJsonArray List() const;
    QJsonObject Call(const QString& Name, const QJsonObject& Arguments) const;
    static QJsonObject Success(const QJsonValue& Value);
    static QJsonObject Failure(const QString& Message);
    std::function<void(const QString&)> Audit;

private:
    struct Tool
    {
        QJsonObject Definition;
        Handler Execute;
    };
    QMap<QString, Tool> Tools;
};
