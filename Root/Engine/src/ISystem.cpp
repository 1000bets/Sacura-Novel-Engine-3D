#include "ISystem.h"

ISystem::~ISystem() = default;

void ISystem::TickSubsystems(float DeltaTime)
{
    for (Subsystem* Sub : m_TickOrder)
    {
        if (Sub && Sub->IsInitialized())
            Sub->Tick(DeltaTime);
    }
}

void ISystem::ShutdownSubsystems()
{
    // Обратный порядок: кто создан последним — умирает первым.
    for (auto It = m_TickOrder.rbegin(); It != m_TickOrder.rend(); ++It)
    {
        Subsystem* Sub = *It;
        if (Sub && Sub->IsInitialized())
        {
            Sub->Deinitialize();
            Sub->m_bInitialized = false;
        }
    }
    m_TickOrder.clear();
    m_Subsystems.clear();
}
