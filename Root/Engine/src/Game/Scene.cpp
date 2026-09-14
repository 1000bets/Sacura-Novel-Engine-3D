#include "Game/Scene.h"
#include "Gameplay/GameObject.h"
#include "Core/MemorySubsystem.h"

#include <algorithm>
#include <cassert>

Scene::Scene() = default;

Scene::Scene(const std::string& InName)
    : Object(InName)
{
}

Scene::~Scene()
{
    auto Copy = m_Objects;
    m_Objects.clear();
    for (GameObject* GO : Copy)
        delete GO;
}

GameObject* Scene::CreateGameObject(const std::string& Name)
{
    auto* Mem = MemorySubsystem::Get();
    assert(Mem && "MemorySubsystem not initialized");

    GameObject* GO = Mem->NewObject<GameObject>(Name);
    GO->m_Scene = this;
    GO->SetOwner(this);

    m_Objects.push_back(GO);
    return GO;
}

void Scene::DestroyGameObject(GameObject* GO)
{
    if (!GO) return;

    {
        auto ChildrenCopy = GO->GetChildren();
        for (GameObject* Child : ChildrenCopy)
            DestroyGameObject(Child);
    }

    if (GO->GetParent())
        GO->SetParent(nullptr);

    auto It = std::find(m_Objects.begin(), m_Objects.end(), GO);
    if (It != m_Objects.end())
        m_Objects.erase(It);

    delete GO;
}

GameObject* Scene::FindByName(const std::string& Name) const
{
    for (GameObject* GO : m_Objects)
    {
        if (GO->GetName() == Name)
            return GO;
    }
    return nullptr;
}

std::vector<GameObject*> Scene::GetRootObjects() const
{
    std::vector<GameObject*> Roots;
    for (GameObject* GO : m_Objects)
    {
        if (GO->GetParent() == nullptr)
            Roots.push_back(GO);
    }
    return Roots;
}
