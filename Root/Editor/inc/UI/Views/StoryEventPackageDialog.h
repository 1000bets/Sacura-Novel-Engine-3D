#pragma once
#include "UI/Presentation/StoryGraphModel.h"
#include <QMap>
class QWidget;
namespace EditorUi
{
struct StoryEventPackageForm
{
    QString Name, Description, Author, License;
    QVector<QPair<QString, QString>> Inputs;
    int Groups = 0, Actions = 0;
};
struct StoryEventPackageDialog
{
    static bool Export(QWidget* Parent, StoryEventPackageForm& Form, QString& Path);
    static QString Open(QWidget* Parent);
    static bool Import(QWidget* Parent, const StoryEventPackageForm& Form, const StoryGraphModel& Model,
                       QMap<QString, QString>& Bindings);
};
} // namespace EditorUi
