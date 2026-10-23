#include "Automation/EditorToolset.h"
#include "Core/Threading/ThreadContext.h"
#include <QJsonDocument>
#include <exception>

bool EditorToolset::Register(const QString& Name, const QString& Description, const QJsonObject& Properties,
                            const QJsonArray& Required, bool bReadOnly, Handler Execute)
{
    if (Name.isEmpty() || Tools.contains(Name) || !Execute)
    {
        return false;
    }
    QJsonObject Schema{{"type", "object"}, {"properties", Properties}, {"required", Required}, {"additionalProperties", false}};
    Tools.insert(Name, {{ {"name", Name}, {"description", Description}, {"inputSchema", Schema},
                         {"annotations", QJsonObject{{"readOnlyHint", bReadOnly}, {"openWorldHint", false}}}}, std::move(Execute)});
    return true;
}

QJsonArray EditorToolset::List() const
{
    QJsonArray Result;
    for (const Tool& Entry : Tools)
    {
        Result.append(Entry.Definition);
    }
    return Result;
}

QJsonObject EditorToolset::Success(const QJsonValue& Value)
{
    QJsonObject Data{{"value", Value}};
    return {{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", QString::fromUtf8(QJsonDocument(Data).toJson(QJsonDocument::Compact))}}}},
            {"structuredContent", Data}, {"isError", false}};
}

QJsonObject EditorToolset::Failure(const QString& Message)
{
    return {{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", Message}}}}, {"isError", true}};
}

QJsonObject EditorToolset::Call(const QString& Name, const QJsonObject& Arguments) const
{
    AssertGameThread();
    const auto Found = Tools.constFind(Name);
    if (Found == Tools.cend())
    {
        return Failure("Unknown tool: " + Name);
    }
    const QJsonObject Schema = Found->Definition.value("inputSchema").toObject();
    const QJsonObject Properties = Schema.value("properties").toObject();
    for (const QJsonValue& Required : Schema.value("required").toArray())
    {
        if (!Arguments.contains(Required.toString()))
        {
            return Failure("Missing argument: " + Required.toString());
        }
    }
    for (auto Argument = Arguments.begin(); Argument != Arguments.end(); ++Argument)
    {
        if (!Properties.contains(Argument.key()))
        {
            return Failure("Unknown argument: " + Argument.key());
        }
        const QJsonObject Field = Properties.value(Argument.key()).toObject();
        const QString Type = Field.value("type").toString();
        const QJsonValue Value = Argument.value();
        if ((Type == "string" && !Value.isString()) || (Type == "boolean" && !Value.isBool()) ||
            (Type == "number" && !Value.isDouble()) || (Type == "object" && !Value.isObject()))
        {
            return Failure("Invalid type for argument: " + Argument.key());
        }
        if (Field.contains("enum") && !Field.value("enum").toArray().contains(Value))
        {
            return Failure("Invalid value for argument: " + Argument.key());
        }
    }
    if (Audit)
    {
        Audit("Tool call: " + Name + " " + QString::fromUtf8(QJsonDocument(Arguments).toJson(QJsonDocument::Compact)));
    }
    try
    {
        return Found->Execute(Arguments);
    }
    catch (const std::exception& Error)
    {
        return Failure(QString::fromUtf8(Error.what()));
    }
}
