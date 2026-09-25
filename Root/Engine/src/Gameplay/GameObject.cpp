#include "Gameplay/GameObject.h"
#include "Game/Scene.h"

#include <algorithm>

GameObject::GameObject() = default;

GameObject::GameObject(const std::string& InName)
    : Object(InName)
{
}

GameObject::~GameObject()
{
    for (Component* Comp : m_Components)
    {
        Comp->OnDestroy();
        Comp->m_GameObject = nullptr;
        delete Comp;
    }
    m_Components.clear();

    for (GameObject* Child : m_Children)
    {
        if (Child != nullptr)
        {
            Child->m_Parent = nullptr;
        }
    }
    m_Children.clear();
}

Matrix GameObject::GetWorldMatrix() const
{
    std::vector<const GameObject*> Chain;
    const GameObject* Current = this;
    while (Current != nullptr)
    {
        Chain.push_back(Current);
        Current = Current->GetParent();
    }

    Matrix World = Matrix::Identity;
    for (auto Iterator = Chain.rbegin(); Iterator != Chain.rend(); ++Iterator)
    {
        World = (*Iterator)->GetTransform().GetMatrix() * World;
    }
    return World;
}

Transform GameObject::GetWorldTransform() const
{
    Transform WorldTransform{};
    Vector3 Scale = Vector3::One;
    Quaternion Rotation = Quaternion::Identity;
    Vector3 Position = Vector3::Zero;
    GetWorldMatrix().Decompose(Scale, Rotation, Position);
    WorldTransform.Position = Position;
    WorldTransform.Rotation = Rotation;
    WorldTransform.Scale = Scale;
    return WorldTransform;
}

Vector3 GameObject::GetWorldPosition() const
{
    return GetWorldTransform().Position;
}

Quaternion GameObject::GetWorldRotation() const
{
    return GetWorldTransform().Rotation;
}

Vector3 GameObject::GetWorldForward() const
{
    return Vector3::Transform(Vector3::Forward, GetWorldRotation());
}

Vector3 GameObject::GetWorldUp() const
{
    return Vector3::Transform(Vector3::Up, GetWorldRotation());
}

bool GameObject::IsActiveInHierarchy() const
{
    const GameObject* Current = this;
    while (Current != nullptr)
    {
        if (!Current->m_bActive)
        {
            return false;
        }
        Current = Current->m_Parent;
    }
    return true;
}

bool GameObject::WouldCreateParentCycle(GameObject* NewParent) const
{
    for (GameObject* Ancestor = NewParent; Ancestor != nullptr; Ancestor = Ancestor->m_Parent)
    {
        if (Ancestor == this)
        {
            return true;
        }
    }
    return false;
}

bool GameObject::SetParent(GameObject* NewParent)
{
    if (m_Parent == NewParent)
    {
        return true;
    }

    if (NewParent != nullptr)
    {
        if (NewParent == this)
        {
            return false;
        }
        if (NewParent->GetScene() != m_Scene)
        {
            return false;
        }
        if (WouldCreateParentCycle(NewParent))
        {
            return false;
        }
    }

    if (m_Parent != nullptr)
    {
        m_Parent->RemoveChild(this);
    }

    m_Parent = NewParent;

    if (m_Parent != nullptr)
    {
        m_Parent->AddChild(this);
    }

    return true;
}

Component* GameObject::AddExistingComponent(Component* Comp)
{
    return AddExistingComponent(Comp, true);
}

Component* GameObject::AddExistingComponent(Component* Comp, bool bInvokeCreate)
{
    if (Comp == nullptr)
    {
        return nullptr;
    }

    if (Comp->GetGameObject() != nullptr)
    {
        return nullptr;
    }

    if (std::find(m_Components.begin(), m_Components.end(), Comp) != m_Components.end())
    {
        return nullptr;
    }

    Comp->m_GameObject = this;
    Comp->SetOwner(this);
    m_Components.push_back(Comp);
    if (bInvokeCreate)
    {
        Comp->OnCreate();
    }
    return Comp;
}

void GameObject::RemoveComponent(Component* Comp)
{
    auto Iterator = std::find(m_Components.begin(), m_Components.end(), Comp);
    if (Iterator != m_Components.end())
    {
        Comp->OnDestroy();
        Comp->m_GameObject = nullptr;
        m_Components.erase(Iterator);
        delete Comp;
    }
}

void GameObject::AddChild(GameObject* Child)
{
    if (Child == nullptr)
    {
        return;
    }
    if (std::find(m_Children.begin(), m_Children.end(), Child) != m_Children.end())
    {
        return;
    }
    m_Children.push_back(Child);
}

void GameObject::RemoveChild(GameObject* Child)
{
    auto Iterator = std::find(m_Children.begin(), m_Children.end(), Child);
    if (Iterator != m_Children.end())
    {
        m_Children.erase(Iterator);
    }
}
