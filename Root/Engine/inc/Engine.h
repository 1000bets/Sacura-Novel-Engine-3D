#pragma once

#include "ISystem.h"

/// Верхний уровень приложения.
/// Владеет подсистемами ядра (Memory, Reflection, Asset, Input, World, …).
/// В рантайме существует один экземпляр.
class Engine : public ISystem
{
public:
    Engine();
    ~Engine() override;

    // ----- ISystem -----
    void Initialize() override;
    void Tick(float DeltaTime) override;
    void Shutdown() override;

    // ----- main loop -----
    void Run();
    void RequestShutdown();

    bool IsRunning() const { return m_bRunning; }

private:
    bool m_bRunning = false;
};
