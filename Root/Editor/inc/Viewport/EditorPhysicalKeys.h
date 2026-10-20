#pragma once

#include <QKeyEvent>

namespace EditorPhysicalKeys
{
#ifdef Q_OS_MACOS
// Cocoa exposes NSEvent.keyCode in Qt's nativeVirtualKey field. These are
// physical keyboard positions and remain independent of the active layout.
constexpr quint32 Forward = 0x0d;
constexpr quint32 Backward = 0x01;
constexpr quint32 Left = 0x00;
constexpr quint32 Right = 0x02;
constexpr quint32 Down = 0x0c;
constexpr quint32 Up = 0x0e;
constexpr quint32 Space = 0x31;
constexpr quint32 Scale = 0x0f;
#else
constexpr quint32 Forward = 0x11;
constexpr quint32 Backward = 0x1f;
constexpr quint32 Left = 0x1e;
constexpr quint32 Right = 0x20;
constexpr quint32 Down = 0x10;
constexpr quint32 Up = 0x12;
constexpr quint32 Space = 0x39;
constexpr quint32 Scale = 0x13;
#endif

inline quint32 Read(const QKeyEvent& Event)
{
#ifdef Q_OS_MACOS
    return Event.nativeVirtualKey();
#else
    return Event.nativeScanCode();
#endif
}
}
