#pragma once

#include <QIcon>

namespace EditorUi {
    enum class StudioIcon {
        Flower, Play, Pause, Step, Folder, Move, Rotate, Scale, Cube, Chivapi, Pleskavica, Camera, Light, Search,
        Expand, Panel,
        Grid, Plus, Fit, ChevronDown, ChevronRight
    };

    QIcon MakeStudioIcon(StudioIcon Icon, const char *ColorToken = "--text-muted");
}
