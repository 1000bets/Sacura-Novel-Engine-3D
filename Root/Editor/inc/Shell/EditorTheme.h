#pragma once

#include "UI/Styles/EditorStyle.h"

// Compatibility facade for existing shell consumers. Styles are resource files.
namespace EditorTheme
{
inline QString Stylesheet()
{
    return EditorUi::EditorStyle::LoadStylesheet();
}
}
