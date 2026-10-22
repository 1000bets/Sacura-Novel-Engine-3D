#pragma once
#include <QIcon>
#include <QString>
#include <QVector>
#include <functional>

namespace EditorUi
{
// Owned UI descriptions. Callers bind commands; the menu never edits a document.
struct StudioMenuItem
{
    QString Identity, Label, Group, SearchText, Description;
    QIcon Icon;
    bool bEnabled = true;
    QVector<StudioMenuItem> Children;
    std::function<void()> Invoke;
};
}
