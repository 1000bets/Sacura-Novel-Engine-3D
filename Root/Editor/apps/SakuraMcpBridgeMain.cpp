#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <iostream>
#include <string>

int main(int ArgumentCount, char** Arguments)
{
    QCoreApplication Application(ArgumentCount, Arguments);
    const QStringList Options = Application.arguments();
    const int EndpointIndex = Options.indexOf("--endpoint");
    if (EndpointIndex < 0 || EndpointIndex + 1 >= Options.size())
    {
        std::cerr << "Usage: SakuraMcpBridge --endpoint NAME\n";
        return 1;
    }
    QLocalSocket Connection;
    Connection.connectToServer(Options.at(EndpointIndex + 1));
    if (!Connection.waitForConnected(5000))
    {
        std::cerr << "Cannot connect to SakuraEditor. Enable MCP and regenerate the configuration.\n";
        return 1;
    }
    std::string Line;
    while (std::getline(std::cin, Line))
    {
        if (Line.size() > 1024 * 1024)
        {
            std::cerr << "MCP request exceeds the 1 MiB limit.\n";
            return 1;
        }
        const QByteArray Message(Line.data(), static_cast<qsizetype>(Line.size()));
        QJsonParseError ParseError;
        const QJsonDocument Document = QJsonDocument::fromJson(Message, &ParseError);
        const qint64 Written = Connection.write(Message + '\n');
        if (Written < 0 || (Connection.bytesToWrite() > 0 && !Connection.waitForBytesWritten(5000)))
        {
            std::cerr << "Editor connection was closed.\n";
            return 1;
        }
        if (Document.isObject() && !Document.object().contains("id") && ParseError.error == QJsonParseError::NoError)
        {
            continue;
        }
        while (!Connection.canReadLine())
        {
            if (!Connection.waitForReadyRead(60000))
            {
                std::cerr << "Editor response timed out or connection was closed.\n";
                return 1;
            }
        }
        const QByteArray Response = Connection.readLine();
        std::cout.write(Response.constData(), Response.size());
        std::cout.flush();
    }
    Connection.disconnectFromServer();
    return 0;
}
