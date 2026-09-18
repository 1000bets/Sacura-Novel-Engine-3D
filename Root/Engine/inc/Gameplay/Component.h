#pragma once

#include "Gameplay/Object.h"

class GameObject;
struct Transform;

/// Базовый компонент.
/// Основной элемент логики на сцене.  Не существует без GameObject.
/// Конкретные типы (Camera, Light, Mesh, Audio, Skeletal) наследуются отсюда.
class Component : public Object
{
    SAKURA_OBJECT(Component)

public:
    ~Component() override;

    // ----- lifecycle (виртуальные, переопределяются наследниками) -----
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void Tick(float /*DeltaTime*/) {}

    // ----- accessors -----
    GameObject* GetGameObject() const { return m_GameObject; }
    const Transform& GetTransform() const;
    Transform& GetTransform();

    bool IsEnabled() const { return m_bEnabled; }
    void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

protected:
    friend class GameObject;// устанавливает m_GameObject

    Component();

    GameObject* m_GameObject = nullptr;
    bool m_bEnabled   = true;
};
