#pragma once
#include <QHash>
#include <QObject>
#include <QString>

namespace EditorUi
{
// Presentation aliases never change property identities or stored values.
inline QString InspectorLabel(const QString& Caption)
{
    const QHash<QString, QString> Labels{
        {"Material Slots", QObject::tr("Материалы по слотам")},
        {"Visible", QObject::tr("Видимость")},
        {"Material", QObject::tr("Материал")},
        {"Mesh", QObject::tr("Меш")},
        {"Material Diagnostic", QObject::tr("Диагностика материала")},
        {"entries", QObject::tr("Элементы")}};
    return Labels.value(Caption, Caption);
}
}
