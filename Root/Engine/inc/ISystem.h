#pragma once

#include "Core/Subsystem.h"

#include <algorithm>
#include <cassert>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

/// Общий интерфейс для Engine и Editor.
/// Владеет набором подсистем, предоставляет шаблонные Create / Get / Destroy.
class ISystem
{
public:
    virtual ~ISystem();

    virtual void Initialize() = 0;
    virtual void Tick(float DeltaTime) = 0;
    virtual void Shutdown() = 0;

    // ----- subsystem management -----

    /// Создать подсистему типа T, зарегистрировать и вызвать Initialize().
    template<typename T, typename... Args>
    T* CreateSubsystem(Args&&... args)
    {
        static_assert(std::is_base_of_v<Subsystem, T>,
                      "T must derive from Subsystem");

        auto Key = std::type_index(typeid(T));
        assert(m_Subsystems.find(Key) == m_Subsystems.end()
               && "Subsystem of this type already exists");

        auto Ptr = std::make_unique<T>(std::forward<Args>(args)...);
        Ptr->m_Owner = this;

        T* Raw = Ptr.get();
        m_Subsystems[Key] = std::move(Ptr);
        m_TickOrder.push_back(Raw);

        Raw->Initialize();
        Raw->m_bInitialized = true;

        return Raw;
    }

    /// Получить подсистему по типу.  nullptr, если не создана.
    template<typename T>
    T* GetSubsystem() const
    {
        static_assert(std::is_base_of_v<Subsystem, T>,
                      "T must derive from Subsystem");

        auto It = m_Subsystems.find(std::type_index(typeid(T)));
        if (It != m_Subsystems.end())
            return static_cast<T*>(It->second.get());
        return nullptr;
    }

    /// Деинициализировать и удалить подсистему.
    template<typename T>
    void DestroySubsystem()
    {
        static_assert(std::is_base_of_v<Subsystem, T>,
                      "T must derive from Subsystem");

        auto Key = std::type_index(typeid(T));
        auto It  = m_Subsystems.find(Key);
        if (It == m_Subsystems.end()) return;

        Subsystem* Sub = It->second.get();

        auto TickIt = std::find(m_TickOrder.begin(), m_TickOrder.end(), Sub);
        if (TickIt != m_TickOrder.end())
            m_TickOrder.erase(TickIt);

        if (Sub->m_bInitialized)
        {
            Sub->Deinitialize();
            Sub->m_bInitialized = false;
        }

        m_Subsystems.erase(It);
    }

protected:
    /// Обновить все подсистемы в порядке создания.
    void TickSubsystems(float DeltaTime);

    /// Деинициализировать все подсистемы в обратном порядке и очистить списки.
    void ShutdownSubsystems();

    std::unordered_map<std::type_index, std::unique_ptr<Subsystem>> m_Subsystems;
    std::vector<Subsystem*> m_TickOrder;
};
