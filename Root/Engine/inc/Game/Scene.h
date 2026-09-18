#pragma once

#include "Gameplay/Object.h"

#include <string>
#include <vector>

class GameObject;

/// Контейнер объектов на сцене.
/// Владеет временем жизни ВСЕХ GameObject (плоский список raw-указателей).
/// Создание/удаление через MemorySubsystem.
class Scene : public Object
{
    SAKURA_OBJECT(Scene)

public:
    ~Scene() override;

    // ----- GameObject management -----

    GameObject* CreateGameObject(const std::string& Name = "GameObject");
    void DestroyGameObject(GameObject* GO);
    GameObject* FindByName(const std::string& Name) const;

    std::vector<GameObject*> GetRootObjects() const;
    const std::vector<GameObject*>& GetAllObjects() const { return m_Objects; }

    size_t GetObjectCount() const { return m_Objects.size(); }

    // ----- state -----

    bool IsLoaded() const { return m_bLoaded; }
    void SetLoaded(bool b) { m_bLoaded = b; }

protected:
    Scene();
    explicit Scene(const std::string& InName);

private:
    std::vector<GameObject*> m_Objects;// Scene владеет, delete в деструкторе
    bool m_bLoaded = false;
};
