#pragma once

#include <QIcon>

namespace EditorUi {
    enum class StudioIcon {
        Flower, Play, Pause, Step, Folder, Move, Rotate, Scale, Cube, Chivapi, Pleskavica, Camera, Light, Search,
        Expand, Panel,
        Grid, Plus, Fit, ChevronDown, ChevronRight, Message, Layers, ExternalLink, Copy, Trash, Star, Select, Magnet, Help, Count
    };

    QIcon MakeFrontendIcon(const QString& Name, const char *ColorToken = "--text-muted", const char *CheckedColorToken = "--accent");
    QIcon MakeStudioIcon(StudioIcon Icon, const char *ColorToken = "--text-muted", const char *CheckedColorToken = "--accent");
}
