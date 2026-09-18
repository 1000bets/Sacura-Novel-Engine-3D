#pragma once

#include "Gameplay/Object.h"
#include "Gameplay/Component.h"
#include "Core/MemorySubsystem.h"
#include "Core/Transform.h"

#include <cassert>
#include <type_traits>
#include <vector>

class Scene;

/// Основа иерархии компонентов.
/// Единственная сущность, имеющая Transform.
/// Аналог Unity GameObject: контейнер компонентов + узел дерева сцены.
class GameObject : public Object
{
    SAKURA_OBJECT(GameObject)

public:
    ~GameObject() override;

    // ========================= Transform =========================

    Transform& GetTransform() { return m_Transform; }
    const Transform& GetTransform() const { return m_Transform; }
    void SetTransform(const Transform& InTransform) { m_Transform = InTransform; }

    // ========================= Hierarchy =========================

    GameObject* GetParent() const { return m_Parent; }
    const std::vector<GameObject*>& GetChildren() const { return m_Children; }

    void SetParent(GameObject* NewParent);

    // ========================= Active / Visual =========================

    bool IsActive() const { return m_bActive; }
    void SetActive(bool b) { m_bActive = b; }

    bool IsVisual() const { return m_bVisual; }
    void SetVisual(bool b) { m_bVisual = b; }

    // ========================= Components =========================

    /// Создать компонент типа T через MemorySubsystem, присоединить.
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args)
    {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");

        auto* Mem = MemorySubsystem::Get();
        assert(Mem && "MemorySubsystem not initialized");

        T* Comp = Mem->NewObject<T>(std::forward<Args>(args)...);
        Comp->m_GameObject = this;
        Comp->SetOwner(this);

        m_Components.push_back(Comp);
        Comp->OnCreate();
        return Comp;
    }

    template<typename T>
    T* GetComponent() const
    {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");
        for (Component* C : m_Components)
        {
            if (T* Casted = dynamic_cast<T*>(C))
                return Casted;
        }
        return nullptr;
    }

    template<typename T>
    std::vector<T*> GetComponents() const
    {
        static_assert(std::is_base_of_v<Component, T>,
                      "T must derive from Component");
        std::vector<T*> Result;
        for (Component* C : m_Components)
        {
            if (T* Casted = dynamic_cast<T*>(C))
                Result.push_back(Casted);
        }
        return Result;
    }

    void RemoveComponent(Component* Comp);

    // ========================= Scene =========================

    Scene* GetScene() const { return m_Scene; }

protected:
    GameObject();
    explicit GameObject(const std::string& InName);

private:
    friend class Scene;// устанавливает m_Scene

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
