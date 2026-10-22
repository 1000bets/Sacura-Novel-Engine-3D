#include "UI/Styles/EditorStyle.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QResource>
#include <QWidget>

static void InitializeStyleResources()
{
    static const bool Initialized = []()
    {
        Q_INIT_RESOURCE(studio);
        return true;
    }();
    Q_UNUSED(Initialized);
}

namespace EditorUi
{
QColor EditorStyle::Color(const char* Token)
{
    InitializeStyleResources();
    static const QJsonObject Tokens = []()
    {
        QFile Palette(QStringLiteral(":/ui/styles/sakura.json"));
        Palette.open(QIODevice::ReadOnly);
        return QJsonDocument::fromJson(Palette.readAll()).object();
    }();
    return QColor(Tokens.value(QString::fromLatin1(Token)).toString());
}

QString EditorStyle::LoadStylesheet(const QString& Theme)
{
    InitializeStyleResources();
    QFile Stylesheet(QStringLiteral(":/ui/styles/editor.qss"));
    QFile Palette(QStringLiteral(":/ui/styles/%1.json").arg(Theme));
    if (!Stylesheet.open(QIODevice::ReadOnly) || !Palette.open(QIODevice::ReadOnly))
    {
        qWarning("EditorStyle: embedded stylesheet or palette is missing");
        return {};
    }
    QString Result = QString::fromUtf8(Stylesheet.readAll());
    QFile StoryStylesheet(QStringLiteral(":/ui/styles/story.qss"));
    if (!StoryStylesheet.open(QIODevice::ReadOnly))
    {
        qWarning("EditorStyle: embedded story stylesheet is missing");
        return {};
    }
    Result += QLatin1Char('\n') + QString::fromUtf8(StoryStylesheet.readAll());
    const QJsonObject Tokens = QJsonDocument::fromJson(Palette.readAll()).object();
    for (auto Iterator = Tokens.begin(); Iterator != Tokens.end(); ++Iterator)
    {
        Result.replace(QStringLiteral("@") + Iterator.key() + QStringLiteral("@"), Iterator.value().toString());
    }
    return Result;
}

void EditorStyle::Apply(QWidget& Root)
{
    Root.setStyleSheet(LoadStylesheet());
}
}
