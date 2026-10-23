#include "UI/GameInputAdapter.h"
#include "Engine.h"
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QWidget>
#include <SDL3/SDL.h>
#include <set>

GameInputAdapter::GameInputAdapter(Engine& InRuntime, QWidget* InSurface)
    : QObject(InSurface), Runtime(InRuntime), Surface(InSurface)
{
    qApp->installEventFilter(this);
    InSurface->setMouseTracking(true);
    bGamepadsInitialized = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
    PollTimer.setInterval(8);
    connect(&PollTimer, &QTimer::timeout, this, &GameInputAdapter::PollGamepads);
    PollTimer.start();
}

GameInputAdapter::~GameInputAdapter()
{
    for (const auto& Entry : Gamepads)
    {
        SDL_CloseGamepad(Entry.second);
    }
    if (bGamepadsInitialized)
    {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
    }
}

std::string GameInputAdapter::KeyControl(const QKeyEvent& Event)
{
#ifdef Q_OS_WIN
    static const std::map<quint32, std::string> Physical{
        {16,"KeyQ"},{17,"KeyW"},{18,"KeyE"},{19,"KeyR"},{20,"KeyT"},{21,"KeyY"},
        {22,"KeyU"},{23,"KeyI"},{24,"KeyO"},{25,"KeyP"},{30,"KeyA"},{31,"KeyS"},
        {32,"KeyD"},{33,"KeyF"},{34,"KeyG"},{35,"KeyH"},{36,"KeyJ"},{37,"KeyK"},
        {38,"KeyL"},{44,"KeyZ"},{45,"KeyX"},{46,"KeyC"},{47,"KeyV"},{48,"KeyB"},
        {49,"KeyN"},{50,"KeyM"}};
    const auto Found = Physical.find(Event.nativeScanCode());
    if (Found != Physical.end())
    {
        return Found->second;
    }
#endif
    if (Event.key() >= Qt::Key_A && Event.key() <= Qt::Key_Z)
    {
        return "Key" + std::string(1, static_cast<char>(Event.key()));
    }
    if (Event.key() >= Qt::Key_0 && Event.key() <= Qt::Key_9)
    {
        return "Digit" + std::string(1, static_cast<char>(Event.key()));
    }
    switch (Event.key())
    {
    case Qt::Key_Space: return "Space";
    case Qt::Key_Return: case Qt::Key_Enter: return "Enter";
    case Qt::Key_Escape: return "Escape";
    case Qt::Key_Shift: return "Shift";
    case Qt::Key_Control: return "Control";
    case Qt::Key_Alt: return "Alt";
    case Qt::Key_Up: return "ArrowUp";
    case Qt::Key_Down: return "ArrowDown";
    case Qt::Key_Left: return "ArrowLeft";
    case Qt::Key_Right: return "ArrowRight";
    default: return "KeyCode" + std::to_string(Event.key());
    }
}

bool GameInputAdapter::HasGameFocus() const
{
    if (!Surface || !Surface->isVisible() || !Surface->window()->isActiveWindow())
    {
        return false;
    }
    QWidget* Focus = QApplication::focusWidget();
    if (!Focus || (Focus != Surface && !Surface->isAncestorOf(Focus)))
    {
        return false;
    }
    for (QWidget* Current = Focus; Current != nullptr; Current = Current->parentWidget())
    {
        if (qobject_cast<QLineEdit*>(Current) || qobject_cast<QAbstractSpinBox*>(Current)
            || qobject_cast<QPlainTextEdit*>(Current) || qobject_cast<QTextEdit*>(Current))
        {
            return false;
        }
        if (Current == Surface)
        {
            break;
        }
    }
    return true;
}

bool GameInputAdapter::eventFilter(QObject* Watched, QEvent* Event)
{
    auto* Session = Runtime.GetInputSession();
    if (!Session)
    {
        return false;
    }
    if (Event->type() == QEvent::ApplicationDeactivate)
    {
        Session->Cancel();
        bHasMousePosition = false;
        return false;
    }
    auto* Widget = qobject_cast<QWidget*>(Watched);
    if (!Widget || !Surface || (Widget != Surface && !Surface->isAncestorOf(Widget)))
    {
        return false;
    }
    if (Event->type() == QEvent::MouseButtonPress && Widget == Surface)
    {
        Surface->setFocus(Qt::MouseFocusReason);
    }
    if (!HasGameFocus())
    {
        return false;
    }
    if (Event->type() == QEvent::KeyPress || Event->type() == QEvent::KeyRelease)
    {
        auto* Key = static_cast<QKeyEvent*>(Event);
        if (!Key->isAutoRepeat())
        {
            float Value = 0.f;
            if (Event->type() == QEvent::KeyPress)
            {
                Value = 1.f;
            }
            Session->Feed("QtKeyboard", "Keyboard", KeyControl(*Key), {Value});
        }
        return true;
    }
    if (Event->type() == QEvent::MouseButtonPress || Event->type() == QEvent::MouseButtonRelease)
    {
        auto* Mouse = static_cast<QMouseEvent*>(Event);
        std::string Control = "Primary";
        if (Mouse->button() == Qt::RightButton)
        {
            Control = "Secondary";
        }
        else if (Mouse->button() == Qt::MiddleButton)
        {
            Control = "Middle";
        }
        float Value = 0.f;
        if (Event->type() == QEvent::MouseButtonPress)
        {
            Value = 1.f;
        }
        Session->Feed("QtPointer", "Mouse", Control, {Value});
        return Widget == Surface;
    }
    if (Event->type() == QEvent::MouseMove)
    {
        const auto Position = static_cast<QMouseEvent*>(Event)->globalPosition();
        if (bHasMousePosition)
        {
            const auto Delta = Position - LastMousePosition;
            Session->Feed("QtPointer", "Mouse", "Delta", {static_cast<float>(Delta.x()), static_cast<float>(Delta.y())}, true);
        }
        LastMousePosition = Position;
        bHasMousePosition = true;
    }
    if (Event->type() == QEvent::Wheel)
    {
        Session->Feed("QtPointer", "Mouse", "Wheel", {static_cast<float>(static_cast<QWheelEvent*>(Event)->angleDelta().y()) / 120.f}, true);
        return true;
    }
    return false;
}

QStringList GameInputAdapter::GetDeviceNames() const
{
    QStringList Names;
    for (const auto& Entry : Gamepads)
    {
        Names.push_back(QString::fromUtf8(SDL_GetGamepadName(Entry.second)));
    }
    return Names;
}

void GameInputAdapter::PollGamepads()
{
    auto* Session = Runtime.GetInputSession();
    const bool bFocused = HasGameFocus();
    if (Session && bHadFocus && !bFocused)
    {
        Session->Cancel();
        bHasMousePosition = false;
    }
    bHadFocus = bFocused;
    if (!bGamepadsInitialized)
    {
        return;
    }
    SDL_UpdateGamepads();
    int Count = 0;
    auto* Identifiers = SDL_GetGamepads(&Count);
    std::set<unsigned int> Connected;
    for (int Index = 0; Index < Count; ++Index)
    {
        const auto Identifier = Identifiers[Index];
        Connected.insert(Identifier);
        if (!Gamepads.count(Identifier))
        {
            if (auto* Device = SDL_OpenGamepad(Identifier))
            {
                Gamepads.emplace(Identifier, Device);
            }
        }
    }
    SDL_free(Identifiers);
    for (auto Iterator = Gamepads.begin(); Iterator != Gamepads.end();)
    {
        const auto Source = "Gamepad" + std::to_string(Iterator->first);
        if (!Connected.count(Iterator->first))
        {
            if (Session)
            {
                Session->RemoveSource(Source);
            }
            SDL_CloseGamepad(Iterator->second);
            Iterator = Gamepads.erase(Iterator);
            continue;
        }
        if (Session && bFocused)
        {
            static const std::array<const char*, 6> Axes{"LeftX", "LeftY", "RightX", "RightY", "LeftTrigger", "RightTrigger"};
            static const std::array<const char*, 15> Buttons{"South", "East", "West", "North", "Back", "Guide", "Start", "LeftStick", "RightStick", "LeftShoulder", "RightShoulder", "DpadUp", "DpadDown", "DpadLeft", "DpadRight"};
            for (size_t Index = 0; Index < Axes.size(); ++Index)
            {
                Session->Feed(Source, "Gamepad", Axes[Index], {SDL_GetGamepadAxis(Iterator->second, static_cast<SDL_GamepadAxis>(Index)) / 32768.f});
            }
            for (size_t Index = 0; Index < Buttons.size(); ++Index)
            {
                Session->Feed(Source, "Gamepad", Buttons[Index], {static_cast<float>(SDL_GetGamepadButton(Iterator->second, static_cast<SDL_GamepadButton>(Index)))});
            }
        }
        ++Iterator;
    }
}
