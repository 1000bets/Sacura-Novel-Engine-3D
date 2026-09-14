#pragma once

class ISystem;

/// Базовый класс подсистемы.
/// Подсистема принадлежит одной из двух систем — Engine или Editor.
/// Жизненный цикл: ISystem::CreateSubsystem → Initialize → Tick* → Deinitialize.
class Subsystem
{
public:
    virtual ~Subsystem();

    virtual void Initialize() {}
    virtual void Deinitialize() {}
    virtual void Tick(float /*DeltaTime*/) {}

    ISystem* GetOwner() const { return m_Owner; }
    bool IsInitialized() const { return m_bInitialized; }


protected:
    friend class ISystem;

    ISystem* m_Owner = nullptr;
    bool m_bInitialized = false;
};
