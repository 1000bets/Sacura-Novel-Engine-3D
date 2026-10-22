#pragma once
#include <QString>

namespace EditorUi
{
inline bool IsNarrator(const QString& Name)
{
    const QString Value = Name.trimmed();
    return Value.isEmpty() || Value.compare(QStringLiteral("Narrator"), Qt::CaseInsensitive) == 0 ||
           Value.compare(QString::fromUtf8("Рассказчик"), Qt::CaseInsensitive) == 0;
}
}
