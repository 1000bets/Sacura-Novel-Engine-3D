#pragma once

#include "Core/Object/Object.h"

#include <string>
#include <vector>

class GameObject;
class Component;
class CameraComponent;

class Scene : public Object
{
    SAKURA_OBJECT(Scene)

public:
    ~Scene() override;

    GameObject* CreateGameObject(const std::string& Name = "GameObject");
    bool DestroyGameObject(GameObject* GO);
    void QueueDestroyGameObject(GameObject* GO);
    void FlushPendingDestroys();
    void QueueRemoveComponent(Component* ComponentInstance);
    bool IsUpdatingComponents() const { return bUpdatingComponents; }
    void BeginPlay();
    void EndPlay();
    bool IsPlaying() const { return bPlaying; }

    GameObject* FindByPersistentId(const std::string& PersistentId) const;
    GameObject* FindByName(const std::string& Name) const;
    GameObject* FindByHandle(ObjectHandle Handle) const;
    CameraComponent* FindPrimaryCamera() const;

    std::vector<GameObject*> GetRootObjects() const;
    const std::vector<GameObject*>& GetAllObjects() const { return m_Objects; }

    size_t GetObjectCount() const { return m_Objects.size(); }

    void Tick(float DeltaTime);

    bool IsLoaded() const { return m_bLoaded; }
    void SetLoaded(bool bLoaded) { m_bLoaded = bLoaded; }

protected:
    Scene();
    explicit Scene(const std::string& InName);

private:
    friend class GameObject;

    bool DestroyGameObjectInternal(GameObject* GO);
    void DetachHierarchy(GameObject* GO);
    void BuildComponentSnapshot();

    std::vector<GameObject*> m_Objects;
    std::vector<ObjectHandle> m_PendingDestroy;
    std::vector<ObjectHandle> PendingComponentRemoval;
    std::vector<ObjectHandle> ComponentSnapshot;
    bool bUpdatingComponents = false;
    bool bPlaying = false;
    bool m_bLoaded = false;
    bool m_bFlushingPendingDestroy = false;
};
