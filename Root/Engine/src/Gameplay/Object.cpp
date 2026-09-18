#include "Gameplay/Object.h"
#include "Core/MemorySubsystem.h"
#include <sstream>

// ------------------------------------------------------------------
// Static ID generator.
// ID остаётся в Object (атомарный счётчик), MemorySubsystem только следит.
// ------------------------------------------------------------------
std::atomic<ObjectID> Object::s_NextID{ 1 };

ObjectID Object::GenerateID()
{
    return s_NextID.fetch_add(1, std::memory_order_relaxed);
}

// ------------------------------------------------------------------
Object::Object()
    : m_ObjectID(GenerateID())
{
    if (auto* Mem = MemorySubsystem::Get())
        Mem->Register(this);
}

Object::Object(const std::string& InName)
    : m_ObjectID(GenerateID())
    , m_Name(InName)
{
    if (auto* Mem = MemorySubsystem::Get())
        Mem->Register(this);
}

Object::~Object()
{
    if (auto* Mem = MemorySubsystem::Get())
        Mem->Unregister(this);
}

std::string Object::ToString() const
{
    std::ostringstream ss;
    ss << "Object[" << m_ObjectID << "] \"" << m_Name << "\"";
    return ss.str();
}
