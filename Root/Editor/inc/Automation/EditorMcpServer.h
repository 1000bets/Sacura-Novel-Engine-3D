#pragma once

#include <QObject>
#include <QLocalServer>
#include <QSet>
#include "Automation/EditorToolset.h"

class QLocalSocket;

class EditorMcpServer : public QObject
{
public:
    EditorMcpServer(EditorToolset& InToolset, QObject* Parent);
    ~EditorMcpServer() override;
    bool Start();
    void Stop();
    bool IsRunning() const;
    QString Endpoint() const;
    QString Error() const;
    QJsonObject Configuration() const;
    QJsonObject Process(const QJsonObject& Request, int& Phase);

private:
    EditorToolset& Toolset;
    QLocalServer Server;
    QSet<QLocalSocket*> Clients;
    QString EndpointName;
};
