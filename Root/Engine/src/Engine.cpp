#include "Engine.h"
#include "Core/MemorySubsystem.h"

Engine::Engine() = default;

Engine::~Engine()
{
    if (m_bRunning)
        Shutdown();
}

void Engine::Initialize()
{
    // Порядок создания критичен.
    // MemorySubsystem — ПЕРВАЯ: все последующие Object-ы автоматически
    // попадают в реестр через Object::Object().
    CreateSubsystem<MemorySubsystem>();

    // Здесь будут создаваться остальные Subsystems-ы

    m_bRunning = true;
}

void Engine::Tick(float DeltaTime)
{
    TickSubsystems(DeltaTime);
}

void Engine::Shutdown()
{
    m_bRunning = false;
    ShutdownSubsystems();
}

void Engine::Run()
{
    Initialize();

    while (m_bRunning)
    {
        constexpr float tempDelta = 1.f / 60.f;
        Tick(tempDelta);
    }

    Shutdown();
}

void Engine::RequestShutdown()
{
    m_bRunning = false;
}
