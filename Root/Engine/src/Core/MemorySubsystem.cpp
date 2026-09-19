#include "Core/MemorySubsystem.h"

#include "Gameplay/ObjectHandle.h"

#include <iostream>

// ------------------------------------------------------------------
MemorySubsystem* MemorySubsystem::s_Instance = nullptr;

// ------------------------------------------------------------------
MemorySubsystem::MemorySubsystem()  = default;
MemorySubsystem::~MemorySubsystem() = default;

// ------------------------------------------------------------------
void MemorySubsystem::Initialize()
{
    assert(s_Instance == nullptr
           && "MemorySubsystem already initialized (double init?)");
    s_Instance = this;
}

void MemorySubsystem::Deinitialize()
{
    ReportLeaks();

    // Не удаляем чужие объекты — мы не владелец.
    // Просто чистим реестр и сбрасываем глобальный указатель.
    m_Registry.clear();
    s_Instance = nullptr;
}

// ========================= Registry =========================

void MemorySubsystem::Register(Object* Obj)
{
    if (!Obj) return;

    ObjectID ID = Obj->GetID();
    assert(m_Registry.find(ID) == m_Registry.end()
           && "Object with this ID already registered (duplicate ID?)");

    m_Registry[ID] = Obj;
    ++m_TotalAllocated;
    UpdatePeak();
}

void MemorySubsystem::Unregister(Object* Obj)
{
    if (!Obj) return;

    auto It = m_Registry.find(Obj->GetID());
    if (It != m_Registry.end())
    {
        m_Registry.erase(It);
        ++m_TotalFreed;
    }
}

// ========================= Lookup =========================

Object* MemorySubsystem::FindByID(ObjectID ID) const
{
    auto It = m_Registry.find(ID);
    return (It != m_Registry.end()) ? It->second : nullptr;
}

Object* MemorySubsystem::ResolveHandle(ObjectHandle Handle) const
{
    if (!Handle.IsValid())
    {
        return nullptr;
    }

    Object* Found = FindByID(Handle.Id);
    if (Found == nullptr)
    {
        return nullptr;
    }
    if (Found->GetGeneration() != Handle.Generation)
    {
        return nullptr;
    }
    return Found;
}

// ========================= Creation / Destruction =========================

void MemorySubsystem::DestroyObject(Object* Obj)
{
    if (!Obj) return;
    // delete → ~Object() → Unregister()
    delete Obj;
}

// ========================= Diagnostics =========================

void MemorySubsystem::ReportLeaks() const
{
    if (m_Registry.empty())
    {
        std::cerr << "[MemorySubsystem] No leaks detected. "
                  << "Total allocated: " << m_TotalAllocated
                  << ", freed: " << m_TotalFreed
                  << ", peak: " << m_PeakAlive << "\n";
        return;
    }

    std::cerr << "[MemorySubsystem] === LEAK REPORT ===\n"
              << "  Alive: " << m_Registry.size()
              << "  (total allocated: " << m_TotalAllocated
              << ", freed: " << m_TotalFreed
              << ", peak: " << m_PeakAlive << ")\n";

    for (auto& [ID, Obj] : m_Registry)
    {
        std::cerr << "  LEAK: " << Obj->ToString() << "\n";
    }

    std::cerr << "[MemorySubsystem] === END REPORT ===\n";
}

// ------------------------------------------------------------------
void MemorySubsystem::UpdatePeak()
{
    if (m_Registry.size() > m_PeakAlive)
        m_PeakAlive = m_Registry.size();
}
