#include "Gameplay/GameObject.h"

#include <algorithm>

// ------------------------------------------------------------------
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
        Child->m_Parent = nullptr;
    m_Children.clear();
}

// ========================= Hierarchy =========================

void GameObject::SetParent(GameObject* NewParent)
{
    if (m_Parent == NewParent)
        return;

    // Нельзя поставить себя в собственного потомка.
    if (NewParent)
    {
        for (GameObject* P = NewParent; P != nullptr; P = P->m_Parent)
        {
            assert(P != this && "Cannot parent to own descendant");
        }
    }

    if (m_Parent)
        m_Parent->RemoveChild(this);

    m_Parent = NewParent;

    if (m_Parent)
        m_Parent->AddChild(this);
}

// ========================= Components =========================

void GameObject::RemoveComponent(Component* Comp)
{
    auto It = std::find(m_Components.begin(), m_Components.end(), Comp);
    if (It != m_Components.end())
    {
        Comp->OnDestroy();
        Comp->m_GameObject = nullptr;
        m_Components.erase(It);
        delete Comp;
    }
}

// ========================= Private helpers =========================

void GameObject::AddChild(GameObject* Child)
{
    m_Children.push_back(Child);
}

void GameObject::RemoveChild(GameObject* Child)
{
    auto It = std::find(m_Children.begin(), m_Children.end(), Child);
    if (It != m_Children.end())
        m_Children.erase(It);
}
