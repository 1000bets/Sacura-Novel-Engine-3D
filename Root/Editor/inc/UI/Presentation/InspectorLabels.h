#pragma once
#include "Localization/EditorLocalization.h"
#include <QString>

namespace EditorUi
{
inline QString InspectorLabel(const QString& Caption)
{
    return EditorLocalization::Get().Caption(Caption);
}
}
