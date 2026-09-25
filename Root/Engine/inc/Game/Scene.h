#pragma once

#include "Gameplay/Object.h"

#include <string>
#include <vector>

class GameObject;

class Scene : public Object
{
    SAKURA_OBJECT(Scene)

public:
    ~Scene() override;

    GameObject* CreateGameObject(const std::string& Name = "GameObject");
    bool DestroyGameObject(GameObject* GO);
    void QueueDestroyGameObject(GameObject* GO);
    void FlushPendingDestroys();

    GameObject* FindByPersistentId(const std::string& PersistentId) const;
    GameObject* FindByName(const std::string& Name) const;
    GameObject* FindByHandle(ObjectHandle Handle) const;

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

    std::vector<GameObject*> m_Objects;
    std::vector<GameObject*> m_PendingDestroy;
    bool m_bLoaded = false;
    bool m_bFlushingPendingDestroy = false;
};
