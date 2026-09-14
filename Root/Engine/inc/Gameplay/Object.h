#pragma once

#include "Core/Types.h"
#include <string>
#include <atomic>

class MemorySubsystem;

/// Макрос для каждого наследника Object.
/// Открывает protected-конструкторы для MemorySubsystem::NewObject<T>().
///
/// class MyComponent : public Component {
///     SAKURA_OBJECT(MyComponent)
/// public:
///     void Tick(float DT) override;
/// };
///
#define SAKURA_OBJECT(ClassName)            \
    friend class MemorySubsystem;

/// Корень иерархии типов.
/// Создание только через MemorySubsystem::NewObject<T>().
/// Конструкторы protected — прямой new / стек запрещены снаружи.
class Object
{
public:
    virtual ~Object();

    // ------- identity -------
    ObjectID GetID() const { return m_ObjectID; }
    const std::string& GetName() const { return m_Name; }
    void SetName(const std::string& InName) { m_Name = InName; }

    // ------- ownership -------
    Object* GetOwner() const { return m_Owner; }
    void SetOwner(Object* InOwner) { m_Owner = InOwner; }

    // ------- debug -------
    virtual std::string ToString() const;

protected:
    friend class MemorySubsystem;

    Object();
    explicit Object(const std::string& InName);

    ObjectID m_ObjectID = INVALID_OBJECT_ID;
    std::string m_Name;
    Object* m_Owner = nullptr;

private:
    static std::atomic<ObjectID> s_NextID;
    static ObjectID GenerateID();
};
