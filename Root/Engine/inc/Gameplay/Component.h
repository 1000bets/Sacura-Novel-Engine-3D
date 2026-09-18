#pragma once

#include "Gameplay/Object.h"

class GameObject;
struct Transform;

class Component : public Object
{
    SAKURA_OBJECT(Component)

public:
    ~Component() override;

    static constexpr const char* StaticReflectionTypeId() { return "engine.Component"; }

    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void Tick(float /*DeltaTime*/) {}

    GameObject* GetGameObject() const { return m_GameObject; }
    const Transform& GetTransform() const;
    Transform& GetTransform();

    bool IsEnabled() const { return m_bEnabled; }
    void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

protected:
    friend class GameObject;

    Component();

    GameObject* m_GameObject = nullptr;
    bool m_bEnabled = true;
};
