#include "World/Scene.h"
#include "World/Components/CameraComponent.h"
#include "World/Components/Component.h"
#include "World/GameObject.h"
#include "Core/Object/MemorySubsystem.h"
#include "Physics/PhysicsWorld.h"
#include "Navigation/NavigationWorld.h"
#include "Authoring/Runtime.h"
#include "Core/Threading/ThreadContext.h"

#include <algorithm>

Scene::Scene() = default;

void Scene::SetAuthoringOperations(std::shared_ptr<const Authoring::OperationRegistry> Operations)
{
    AssertGameThread();
    if (AuthoringOperations == Operations || bPlaying)
    {
        return;
    }
    AuthoringRuntime.reset();
    AuthoringOperations = std::move(Operations);
    if (AuthoringOperations)
    {
        AuthoringRuntime = std::make_unique<Authoring::Runtime>(AuthoringOperations, GetObjectHandle());
    }
}

PhysicsWorld& Scene::GetPhysics()
{
    if (!Physics)
    {
        Physics = std::make_unique<PhysicsWorld>(*this);
    }
    return *Physics;
}

void Scene::SetAssetManager(AssetManager* InAssets)
{
    Assets = InAssets;
}

NavigationWorld& Scene::GetNavigation()
{
    if (!Navigation)
    {
        Navigation = std::make_unique<NavigationWorld>(*this);
        Navigation->SetJobSystem(Jobs);
    }
    return *Navigation;
}

void Scene::SetJobSystem(JobSystem* System)
{
    Jobs = System;
    if (Navigation)
    {
        Navigation->SetJobSystem(Jobs);
    }
}

Scene::Scene(const std::string& InName)
    : Object(InName)
{
}

Scene::~Scene()
{
    EndPlay();
    Navigation.reset();
    Physics.reset();
    m_PendingDestroy.clear();
    PendingComponentRemoval.clear();

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
    if (std::find(m_Objects.begin(), m_Objects.end(), ObjectInstance) == m_Objects.end())
    {
        return false;
    }
    QueueDestroyGameObject(ObjectInstance);
    FlushPendingDestroys();
    return true;
}

void Scene::QueueDestroyGameObject(GameObject* ObjectInstance)
{
    if (std::find(m_Objects.begin(), m_Objects.end(), ObjectInstance) == m_Objects.end()
        || ObjectInstance->bPendingDestroy)
    {
        return;
    }
    ObjectInstance->bPendingDestroy = true;
    m_PendingDestroy.push_back(ObjectInstance->GetObjectHandle());
}

void Scene::QueueRemoveComponent(Component* ComponentInstance)
{
    if (ComponentInstance == nullptr || ComponentInstance->bPendingRemoval)
    {
        return;
    }
    GameObject* Owner = ComponentInstance->GetGameObject();
    if (Owner == nullptr || Owner->GetScene() != this)
    {
        return;
    }
    ComponentInstance->bPendingRemoval = true;
    PendingComponentRemoval.push_back(ComponentInstance->GetObjectHandle());
}

void Scene::FlushPendingDestroys()
{
    if (m_bFlushingPendingDestroy || bUpdatingComponents)
    {
        return;
    }
    m_bFlushingPendingDestroy = true;
    MemorySubsystem* Memory = MemorySubsystem::Get();
    while (!m_PendingDestroy.empty() || !PendingComponentRemoval.empty())
    {
        while (!m_PendingDestroy.empty())
        {
            const ObjectHandle Identity = m_PendingDestroy.back();
            m_PendingDestroy.pop_back();
            GameObject* ObjectInstance = Memory->ResolveHandle<GameObject>(Identity);
            if (ObjectInstance != nullptr && ObjectInstance->GetScene() == this)
            {
                DestroyGameObjectInternal(ObjectInstance);
            }
        }
        while (!PendingComponentRemoval.empty())
        {
            const ObjectHandle Identity = PendingComponentRemoval.back();
            PendingComponentRemoval.pop_back();
            Component* ComponentInstance = Memory->ResolveHandle<Component>(Identity);
            if (ComponentInstance != nullptr)
            {
                GameObject* Owner = ComponentInstance->GetGameObject();
                if (Owner != nullptr && Owner->GetScene() == this)
                {
                    Owner->RemoveComponentInternal(ComponentInstance);
                }
            }
        }
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

GameObject* Scene::FindByHandle(ObjectHandle Identity) const
{
    MemorySubsystem* Memory = MemorySubsystem::Get();
    if (Memory == nullptr)
    {
        return nullptr;
    }
    GameObject* ObjectInstance = Memory->ResolveHandle<GameObject>(Identity);
    if (ObjectInstance != nullptr && ObjectInstance->GetScene() == this)
    {
        return ObjectInstance;
    }
    return nullptr;
}

CameraComponent* Scene::FindPrimaryCamera() const
{
    CameraComponent* Fallback = nullptr;
    for (GameObject* ObjectInstance : m_Objects)
    {
        if (!ObjectInstance->IsActiveInHierarchy())
        {
            continue;
        }
        for (Component* ComponentInstance : ObjectInstance->GetAllComponents())
        {
            CameraComponent* Camera = dynamic_cast<CameraComponent*>(ComponentInstance);
            if (Camera == nullptr || !Camera->IsEnabled() || Camera->IsPendingRemoval())
            {
                continue;
            }
            if (Camera->bPrimary)
            {
                return Camera;
            }
            if (Fallback == nullptr)
            {
                Fallback = Camera;
            }
        }
    }
    return Fallback;
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

void Scene::BuildComponentSnapshot()
{
    ComponentSnapshot.clear();
    for (GameObject* ObjectInstance : m_Objects)
    {
        for (Component* ComponentInstance : ObjectInstance->GetAllComponents())
        {
            ComponentSnapshot.push_back(ComponentInstance->GetObjectHandle());
        }
    }
}

bool Scene::BeginPlay()
{
    if (bPlaying)
    {
        return true;
    }
    if (!GetPhysics().Prepare(Assets) || !GetPhysics().Initialize())
    {
        return false;
    }
    bPlaying = true;
    if (AuthoringRuntime)
    {
        const Authoring::Diagnostic Started = AuthoringRuntime->Start();
        if (!Started.bOk)
        {
            bPlaying = false;
            Physics->Shutdown();
            PrintString("Scene: authoring session failed: " + Started.Message);
            return false;
        }
    }
    bUpdatingComponents = true;
    BuildComponentSnapshot();
    for (const ObjectHandle Identity : ComponentSnapshot)
    {
        Component* ComponentInstance = MemorySubsystem::Get()->ResolveHandle<Component>(Identity);
        if (ComponentInstance != nullptr && !ComponentInstance->IsPendingRemoval()
            && !ComponentInstance->GetGameObject()->IsPendingDestroy())
        {
            ComponentInstance->StartPlay();
        }
    }
    bUpdatingComponents = false;
    FlushPendingDestroys();
    return true;
}

void Scene::EndPlay()
{
    if (AuthoringRuntime)
    {
        AuthoringRuntime->Stop();
    }
    if (!bPlaying)
    {
        return;
    }
    bPlaying = false;
    bUpdatingComponents = true;
    BuildComponentSnapshot();
    for (const ObjectHandle Identity : ComponentSnapshot)
    {
        Component* ComponentInstance = MemorySubsystem::Get()->ResolveHandle<Component>(Identity);
        if (ComponentInstance != nullptr)
        {
            ComponentInstance->StopPlay();
        }
    }
    bUpdatingComponents = false;
    FlushPendingDestroys();
    if (Physics)
    {
        Physics->Shutdown();
    }
    if (Navigation)
    {
        Navigation->ResetCrowds();
    }
}

void Scene::Tick(float DeltaTime, bool bSinglePhysicsStep)
{
    if (bUpdatingComponents)
    {
        return;
    }
    if (bPlaying && AuthoringRuntime)
    {
        AuthoringRuntime->Tick(DeltaTime);
    }
    if (Navigation)
    {
        Navigation->Update();
        if (bPlaying)
        {
            Navigation->UpdateCrowds(DeltaTime);
        }
    }
    bUpdatingComponents = true;
    BuildComponentSnapshot();
    for (const ObjectHandle Identity : ComponentSnapshot)
    {
        Component* ComponentInstance = MemorySubsystem::Get()->ResolveHandle<Component>(Identity);
        if (ComponentInstance == nullptr || ComponentInstance->IsPendingRemoval())
        {
            continue;
        }
        GameObject* Owner = ComponentInstance->GetGameObject();
        if (Owner == nullptr || !Owner->IsActiveInHierarchy() || !ComponentInstance->IsEnabled())
        {
            continue;
        }
        if (bPlaying && !ComponentInstance->IsPlaying())
        {
            ComponentInstance->StartPlay();
        }
        if (!ComponentInstance->IsPendingRemoval() && Owner->IsActiveInHierarchy())
        {
            ComponentInstance->Tick(DeltaTime);
        }
    }
    bUpdatingComponents = false;
    FlushPendingDestroys();
    if (bPlaying && Physics)
    {
        if (bSinglePhysicsStep)
        {
            Physics->StepOnce();
        }
        else
        {
            Physics->Tick(DeltaTime);
        }
    }
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
