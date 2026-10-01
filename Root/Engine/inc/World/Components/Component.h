#pragma once

#include "Core/Object/Object.h"

class GameObject;
struct Transform;

class Component : public Object
{
    SAKURA_OBJECT(Component)

public:
    ~Component() override;

    static constexpr const char* StaticReflectionTypeId() { return "engine.Component"; }

    virtual std::string GetEditorIconPath() const { return {}; }
    virtual int GetEditorIconPriority() const { return 0; }

    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void Tick(float /*DeltaTime*/) {}
    void StartPlay();
    void StopPlay();
    bool IsPlaying() const { return bPlaying; }
    bool IsPendingRemoval() const { return bPendingRemoval; }

    GameObject* GetGameObject() const { return m_GameObject; }
    const Transform& GetTransform() const;
    void SetTransform(const Transform& InTransform);

    bool IsEnabled() const { return m_bEnabled; }
    void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }

protected:
    friend class GameObject;
    friend class Scene;

    Component();

    GameObject* m_GameObject = nullptr;
    bool m_bEnabled = true;
    bool bPlaying = false;
    bool bPendingRemoval = false;
};
