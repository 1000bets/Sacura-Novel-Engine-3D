#include "Game/Scene.h"
#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"
#include "Core/MemorySubsystem.h"

#include <algorithm>

Scene::Scene() = default;

Scene::Scene(const std::string& InName)
    : Object(InName)
{
}

Scene::~Scene()
{
    m_PendingDestroy.clear();

    for (GameObject* ObjectInstance : m_Objects)
    {
        if (ObjectInstance == nullptr)
        {
            continue;
        }

        for (GameObject* Child : ObjectInstance->m_Children)
        {
            if (Child != nullptr)
            {
                Child->m_Parent = nullptr;
            }
        }

        ObjectInstance->m_Parent = nullptr;
        ObjectInstance->m_Children.clear();
        ObjectInstance->m_Scene = nullptr;
    }

    auto Copy = m_Objects;
    m_Objects.clear();
    for (GameObject* ObjectInstance : Copy)
    {
        delete ObjectInstance;
    }
}

GameObject* Scene::CreateGameObject(const std::string& Name)
{
    auto* Mem = MemorySubsystem::Get();
    if (Mem == nullptr)
    {
        return nullptr;
    }

    GameObject* ObjectInstance = Mem->NewObject<GameObject>(Name);
    ObjectInstance->m_Scene = this;
    ObjectInstance->SetOwner(this);
    m_Objects.push_back(ObjectInstance);
    return ObjectInstance;
}

void Scene::DetachHierarchy(GameObject* ObjectInstance)
{
    if (ObjectInstance == nullptr)
    {
        return;
    }

    if (ObjectInstance->m_Parent != nullptr)
    {
        ObjectInstance->m_Parent->RemoveChild(ObjectInstance);
        ObjectInstance->m_Parent = nullptr;
    }

    auto ChildrenCopy = ObjectInstance->m_Children;
    ObjectInstance->m_Children.clear();
    for (GameObject* Child : ChildrenCopy)
    {
        if (Child != nullptr)
        {
            Child->m_Parent = nullptr;
        }
    }
}

bool Scene::DestroyGameObjectInternal(GameObject* ObjectInstance)
{
    if (ObjectInstance == nullptr)
    {
        return false;
    }

    if (ObjectInstance->GetScene() != this)
    {
        return false;
    }

    {
        auto ChildrenCopy = ObjectInstance->GetChildren();
        for (GameObject* Child : ChildrenCopy)
        {
            DestroyGameObjectInternal(Child);
        }
    }

    DetachHierarchy(ObjectInstance);

    auto Iterator = std::find(m_Objects.begin(), m_Objects.end(), ObjectInstance);
    if (Iterator != m_Objects.end())
    {
        m_Objects.erase(Iterator);
    }

    ObjectInstance->m_Scene = nullptr;
    delete ObjectInstance;
    return true;
}

bool Scene::DestroyGameObject(GameObject* ObjectInstance)
{
    return DestroyGameObjectInternal(ObjectInstance);
}

void Scene::QueueDestroyGameObject(GameObject* ObjectInstance)
{
    if (ObjectInstance == nullptr || ObjectInstance->GetScene() != this)
    {
        return;
    }

    if (std::find(m_PendingDestroy.begin(), m_PendingDestroy.end(), ObjectInstance) == m_PendingDestroy.end())
    {
        m_PendingDestroy.push_back(ObjectInstance);
    }

    if (!m_bFlushingPendingDestroy)
    {
        return;
    }
}

void Scene::FlushPendingDestroys()
{
    if (m_bFlushingPendingDestroy)
    {
        return;
    }

    m_bFlushingPendingDestroy = true;
    while (!m_PendingDestroy.empty())
    {
        GameObject* ObjectInstance = m_PendingDestroy.back();
        m_PendingDestroy.pop_back();
        DestroyGameObjectInternal(ObjectInstance);
    }
    m_bFlushingPendingDestroy = false;
}

GameObject* Scene::FindByName(const std::string& Name) const
{
    for (GameObject* ObjectInstance : m_Objects)
    {
        if (ObjectInstance != nullptr && ObjectInstance->GetName() == Name)
        {
            return ObjectInstance;
        }
    }
    return nullptr;
}

GameObject* Scene::FindByHandle(ObjectHandle Handle) const
{
    if (!Handle.IsValid())
    {
        return nullptr;
    }

    for (GameObject* ObjectInstance : m_Objects)
    {
        if (ObjectInstance != nullptr && ObjectInstance->GetObjectHandle() == Handle)
        {
            return ObjectInstance;
        }
    }
    return nullptr;
}

std::vector<GameObject*> Scene::GetRootObjects() const
{
    std::vector<GameObject*> Roots;
    for (GameObject* ObjectInstance : m_Objects)
    {
        if (ObjectInstance != nullptr && ObjectInstance->GetParent() == nullptr)
        {
            Roots.push_back(ObjectInstance);
        }
    }
    return Roots;
}

void Scene::Tick(float DeltaTime)
{
    for (GameObject* ObjectInstance : m_Objects)
    {
        if (ObjectInstance == nullptr || !ObjectInstance->IsActiveInHierarchy())
        {
            continue;
        }

        for (Component* ComponentInstance : ObjectInstance->GetAllComponents())
        {
            if (ComponentInstance != nullptr && ComponentInstance->IsEnabled())
            {
                ComponentInstance->Tick(DeltaTime);
            }
        }
    }

    FlushPendingDestroys();
}

GameObject* Scene::FindByPersistentId(const std::string& PersistentId) const
{
    for (GameObject* ObjectInstance : m_Objects)
    {
        if (ObjectInstance->GetPersistentId() == PersistentId)
        {
            return ObjectInstance;
        }
    }
    return nullptr;
}
