#pragma once

#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QTimer>
#include <map>
#include <string>

class Engine;
class QWidget;
class QKeyEvent;
struct SDL_Gamepad;

class GameInputAdapter : public QObject
{
public:
    GameInputAdapter(Engine& Runtime, QWidget* Surface);
    ~GameInputAdapter() override;
    static std::string KeyControl(const QKeyEvent& Event);
    void PollGamepads();
    QStringList GetDeviceNames() const;

protected:
    bool eventFilter(QObject* Watched, QEvent* Event) override;

private:
    bool HasGameFocus() const;
    Engine& Runtime;
    QPointer<QWidget> Surface;
    QTimer PollTimer;
    QPointF LastMousePosition;
    bool bHasMousePosition = false;
    bool bHadFocus = false;
    bool bGamepadsInitialized = false;
    std::map<unsigned int, SDL_Gamepad*> Gamepads;
};
