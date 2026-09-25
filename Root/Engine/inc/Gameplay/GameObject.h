#pragma once

#include "Gameplay/Object.h"
#include "Gameplay/Component.h"
#include "Core/MemorySubsystem.h"
#include "Core/Transform.h"
#include "Reflection/ReflectionSubsystem.h"
#include "Reflection/TypeId.h"

#include <cassert>
#include <type_traits>
#include <utility>
#include <vector>

class Scene;

class GameObject : public Object
{
    SAKURA_OBJECT(GameObject)

public:
    ~GameObject() override;

    Transform& GetTransform() { return m_Transform; }
    const Transform& GetTransform() const { return m_Transform; }
    void SetTransform(const Transform& InTransform) { m_Transform = InTransform; }

    Matrix GetWorldMatrix() const;
    Transform GetWorldTransform() const;
    Vector3 GetWorldPosition() const;
    Quaternion GetWorldRotation() const;
    Vector3 GetWorldForward() const;
    Vector3 GetWorldUp() const;

    GameObject* GetParent() const { return m_Parent; }
    const std::vector<GameObject*>& GetChildren() const { return m_Children; }

    bool SetParent(GameObject* NewParent);
    bool WouldCreateParentCycle(GameObject* NewParent) const;

    bool IsActive() const { return m_bActive; }
    void SetActive(bool bActive) { m_bActive = bActive; }
    bool IsActiveInHierarchy() const;

    bool IsVisual() const { return m_bVisual; }
    void SetVisual(bool bVisual) { m_bVisual = bVisual; }

    template<typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");

        if constexpr (sizeof...(Args) == 0)
        {
            ReflectionSubsystem& Reflection = ReflectionSubsystem::Get();
            if (Reflection.IsInitialized())
            {
                Object* Created = Reflection.CreateInstance(TypeId{T::StaticReflectionTypeId()});
                if (Created != nullptr)
                {
                    T* Typed = dynamic_cast<T*>(Created);
                    if (Typed != nullptr)
                    {
                        return static_cast<T*>(AddExistingComponent(Typed));
                    }

                    if (MemorySubsystem* Memory = MemorySubsystem::Get())
                    {
                        Memory->DestroyObject(Created);
                    }
                    else
                    {
                        delete Created;
                    }
                }
            }
        }

        auto* Mem = MemorySubsystem::Get();
        assert(Mem && "MemorySubsystem not initialized");

        T* Comp = Mem->NewObject<T>(std::forward<Args>(args)...);
        return static_cast<T*>(AddExistingComponent(Comp));
    }

    Component* AddExistingComponent(Component* Comp);
    Component* AddExistingComponent(Component* Comp, bool bInvokeCreate);

    template<typename T>
    T* GetComponent() const
    {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");
        for (Component* ComponentInstance : m_Components)
        {
            if (T* Casted = dynamic_cast<T*>(ComponentInstance))
            {
                return Casted;
            }
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T*> GetComponents() const
    {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");
        std::vector<T*> Result;
        for (Component* ComponentInstance : m_Components)
        {
            if (T* Casted = dynamic_cast<T*>(ComponentInstance))
            {
                Result.push_back(Casted);
            }
        }
        return Result;
    }

    const std::vector<Component*>& GetAllComponents() const { return m_Components; }

    void RemoveComponent(Component* Comp);

    Scene* GetScene() const { return m_Scene; }

protected:
    GameObject();
    explicit GameObject(const std::string& InName);

private:
    friend class Scene;

    void AddChild(GameObject* Child);
    void RemoveChild(GameObject* Child);

    Transform m_Transform;
    GameObject* m_Parent = nullptr;
    std::vector<GameObject*> m_Children;
    std::vector<Component*> m_Components;
    Scene* m_Scene = nullptr;
    bool m_bActive = true;
    bool m_bVisual = true;
};
