#pragma once

#include "Core/Subsystem.h"
#include "Gameplay/Object.h"

#include <cassert>
#include <cstddef>
#include <type_traits>
#include <unordered_map>

/// Центральный реестр всех Object.
///
/// Зачем нужен:
///   1. Детект утечек — при Deinitialize() отчёт обо всех неудалённых объектах.
///   2. Lookup по ObjectID — мост C++↔Python (pybind11 передаёт ID,
///      С++ резолвит в указатель).
///   3. Статистика: сколько создано, уничтожено, пик одновременно живых.
///
/// Как работает:
///   Object::Object() вызывает Register(this).
///   Object::~Object() вызывает Unregister(this).
///   Любой способ создания (new, make_unique, NewObject<T>) автоматически
///   проходит через реестр.
///
/// Порядок жизни:
///   Создаётся ПЕРВОЙ среди подсистем Engine.
///   Уничтожается ПОСЛЕДНЕЙ (ShutdownSubsystems идёт в обратном порядке).
///
class MemorySubsystem : public Subsystem
{
public:
    MemorySubsystem();
    ~MemorySubsystem() override;

    void Initialize()   override;
    void Deinitialize() override;

    // ========================= Registry =========================

    /// Зарегистрировать объект (вызывается из Object::Object).
    void Register(Object* Obj);

    /// Снять с учёта (вызывается из Object::~Object).
    void Unregister(Object* Obj);

    // ========================= Lookup =========================

    /// Найти живой объект по ID.  nullptr, если не найден / уже уничтожен.
    Object* FindByID(ObjectID ID) const;

    /// Шаблонная версия с кастом.
    template<typename T>
    T* FindByID(ObjectID ID) const
    {
        static_assert(std::is_base_of_v<Object, T>,
                      "T must derive from Object");
        return dynamic_cast<T*>(FindByID(ID));
    }

    // ========================= Creation / Destruction =========================

    /// Создать объект типа T, зарегистрировать (Object::Object делает это сам).
    /// Владение за вызывающим — MemorySubsystem только следит.
    template<typename T, typename... Args>
    T* NewObject(Args&&... args)
    {
        static_assert(std::is_base_of_v<Object, T>,
                      "T must derive from Object");
        // new → Object() → Register(this)
        return new T(std::forward<Args>(args)...);
    }

    /// Уничтожить объект.  delete → ~Object() → Unregister(this).
    void DestroyObject(Object* Obj);

    // ========================= Diagnostics =========================

    /// Сколько объектов живо прямо сейчас.
    size_t GetAliveCount() const { return m_Registry.size(); }

    /// Всего создано за сессию.
    size_t GetTotalAllocated() const { return m_TotalAllocated; }

    /// Всего уничтожено за сессию.
    size_t GetTotalFreed() const { return m_TotalFreed; }

    /// Пиковое количество одновременно живых.
    size_t GetPeakAliveCount() const { return m_PeakAlive; }

    /// Вывести в stderr все объекты, которые ещё живы.
    void ReportLeaks() const;

    // ========================= Global access =========================

    /// Глобальный указатель.  nullptr до Initialize / после Deinitialize.
    static MemorySubsystem* Get() { return s_Instance; }

private:
    void UpdatePeak();

    std::unordered_map<ObjectID, Object*> m_Registry;

    size_t m_TotalAllocated = 0;
    size_t m_TotalFreed = 0;
    size_t m_PeakAlive = 0;

    static MemorySubsystem* s_Instance;
};
