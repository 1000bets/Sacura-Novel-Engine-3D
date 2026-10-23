#include "Automation/EditorMcpServer.h"
#include "Core/Threading/ThreadContext.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QUuid>
#include <QTimer>
#include <memory>

EditorMcpServer::EditorMcpServer(EditorToolset& InToolset, QObject* Parent)
    : QObject(Parent), Toolset(InToolset)
{
    EndpointName = "SakuraEditor-" + QUuid::createUuid().toString(QUuid::WithoutBraces);
    Server.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&Server, &QLocalServer::newConnection, this, [this]()
    {
        while (Server.hasPendingConnections())
        {
            QLocalSocket* Client = Server.nextPendingConnection();
            if (Clients.size() >= 8)
            {
                Client->abort();
                Client->deleteLater();
                continue;
            }
            Clients.insert(Client);
            Client->setReadBufferSize(1024 * 1024 + 1);
            auto Phase = std::make_shared<int>(0);
            connect(Client, &QLocalSocket::disconnected, this, [this, Client]()
            {
                Clients.remove(Client);
                Client->deleteLater();
            });
            connect(Client, &QLocalSocket::readyRead, this, [this, Client, Phase]()
            {
                if (Client->bytesAvailable() > 1024 * 1024)
                {
                    Client->abort();
                    return;
                }
                while (Client->canReadLine())
                {
                    QJsonParseError Error;
                    const QJsonDocument Message = QJsonDocument::fromJson(Client->readLine(), &Error);
                    QJsonObject Response;
                    if (Error.error != QJsonParseError::NoError || !Message.isObject())
                    {
                        Response = {{"jsonrpc", "2.0"}, {"id", QJsonValue::Null},
                                    {"error", QJsonObject{{"code", -32700}, {"message", "Invalid JSON-RPC message"}}}};
                    }
                    else
                    {
                        Response = Process(Message.object(), *Phase);
                    }
                    if (!Response.isEmpty())
                    {
                        Client->write(QJsonDocument(Response).toJson(QJsonDocument::Compact) + '\n');
                    }
                    if (Client->bytesToWrite() > 8 * 1024 * 1024)
                    {
                        Client->abort();
                        return;
                    }
                }
            });
        }
    });
}

EditorMcpServer::~EditorMcpServer()
{
    Stop();
}

bool EditorMcpServer::Start()
{
    if (Server.isListening())
    {
        return true;
    }
    const bool bStarted = Server.listen(EndpointName);
    if (bStarted)
    {
        PrintString("MCP server started: " + EndpointName.toStdString());
    }
    else
    {
        PrintString("MCP server failed: " + Server.errorString().toStdString());
    }
    return bStarted;
}

void EditorMcpServer::Stop()
{
    if (Server.isListening())
    {
        PrintString("MCP server stopped");
    }
    Server.close();
    const auto Connected = Clients.values();
    for (QLocalSocket* Client : Connected)
    {
        Client->abort();
    }
}

bool EditorMcpServer::IsRunning() const { return Server.isListening(); }
QString EditorMcpServer::Endpoint() const { return EndpointName; }
QString EditorMcpServer::Error() const { return Server.errorString(); }

QJsonObject EditorMcpServer::Configuration() const
{
    const QString Executable = QCoreApplication::applicationDirPath() + "/SakuraMcpBridge.exe";
    return {{"mcpServers", QJsonObject{{"sakura-editor", QJsonObject{{"command", Executable},
            {"args", QJsonArray{"--endpoint", EndpointName}}}}}}};
}

QJsonObject EditorMcpServer::Process(const QJsonObject& Request, int& Phase)
{
    QJsonValue Identifier = QJsonValue::Null;
    if (Request.contains("id"))
    {
        Identifier = Request.value("id");
    }
    auto Failure = [&Identifier](int Code, const QString& Message)
    {
        return QJsonObject{{"jsonrpc", "2.0"}, {"id", Identifier},
                           {"error", QJsonObject{{"code", Code}, {"message", Message}}}};
    };
    if ((!Identifier.isNull() && !Identifier.isString() && !Identifier.isDouble()) ||
        Request.value("jsonrpc") != "2.0" || !Request.value("method").isString() ||
        (Request.contains("params") && !Request.value("params").isObject()))
    {
        return Failure(-32600, "Invalid request");
    }
    const QString Method = Request.value("method").toString();
    if (!Request.contains("id"))
    {
        if (Method == "notifications/initialized" && Phase == 1)
        {
            Phase = 2;
        }
        return {};
    }
    const QJsonObject Parameters = Request.value("params").toObject();
    QJsonObject Result;
    if (Method == "initialize")
    {
        if (Phase != 0 || !Parameters.value("protocolVersion").isString())
        {
            return Failure(-32602, "Invalid initialization");
        }
        QString Version = Parameters.value("protocolVersion").toString();
        if (Version != "2025-11-25" && Version != "2025-06-18" && Version != "2025-03-26" && Version != "2024-11-05")
        {
            Version = "2025-11-25";
        }
        Result = {{"protocolVersion", Version}, {"capabilities", QJsonObject{{"tools", QJsonObject{}}}},
                  {"serverInfo", QJsonObject{{"name", "SakuraEditor"}, {"version", "0.1.0"}}},
                  {"instructions", "Tools operate on the connected editor. Use persistent object IDs. Scene edits support undo. Save explicitly."}};
        Phase = 1;
    }
    else if (Method == "ping")
    {
        Result = {};
    }
    else if (Method != "tools/list" && Method != "tools/call")
    {
        return Failure(-32601, "Method not found");
    }
    else if (Phase != 2)
    {
        return Failure(-32002, "Initialize the connection first");
    }
    else if (Method == "tools/list")
    {
        if (Parameters.contains("cursor"))
        {
            return Failure(-32602, "This toolset does not use pagination cursors");
        }
        Result = {{"tools", Toolset.List()}};
    }
    else if (Method == "tools/call")
    {
        if (!Parameters.value("name").isString() ||
            (Parameters.contains("arguments") && !Parameters.value("arguments").isObject()))
        {
            return Failure(-32602, "Expected tool name and object arguments");
        }
        Result = Toolset.Call(Parameters.value("name").toString(), Parameters.value("arguments").toObject());
    }
    else
    {
        return Failure(-32601, "Method not found");
    }
    return {{"jsonrpc", "2.0"}, {"id", Identifier}, {"result", Result}};
}
