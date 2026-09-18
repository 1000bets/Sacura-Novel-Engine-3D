#pragma once

#include "Core/Types.h"
#include "Gameplay/ObjectHandle.h"
#include "Reflection/TypeId.h"

#include <string>
#include <atomic>

class MemorySubsystem;
class Class;

#define SAKURA_OBJECT(ClassName)            \
    friend class MemorySubsystem;

class Object
{
public:
    virtual ~Object();

    ObjectID GetID() const { return m_ObjectID; }
    uint32_t GetGeneration() const { return m_Generation; }
    ObjectHandle GetObjectHandle() const { return ObjectHandle{m_ObjectID, m_Generation}; }

    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& InName) { m_Name = InName; }

    Object* GetOwner() const { return m_Owner; }
    void SetOwner(Object* InOwner) { m_Owner = InOwner; }

    Class* GetClass() const { return m_Class; }
    TypeId GetTypeId() const;
    void AssignClass(Class* InClass) { m_Class = InClass; }

    static constexpr const char* StaticReflectionTypeId() { return "engine.Object"; }

    virtual std::string ToString() const;

protected:
    friend class MemorySubsystem;

    Object();
    explicit Object(const std::string& InName);

    ObjectID m_ObjectID = INVALID_OBJECT_ID;
    uint32_t m_Generation = 1;
    std::string m_Name;
    Object* m_Owner = nullptr;
    Class* m_Class = nullptr;

private:
    static std::atomic<ObjectID> s_NextID;
    static ObjectID GenerateID();
};
