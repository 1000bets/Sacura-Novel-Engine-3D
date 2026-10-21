#pragma once

#include <QString>
#include <QColor>

class QWidget;

namespace EditorUi
{
class EditorStyle
{
public:
    static QString LoadStylesheet(const QString& Theme = QStringLiteral("sakura"));
    static QColor Color(const char* Token);
    static void Apply(QWidget& Root);
};
}
