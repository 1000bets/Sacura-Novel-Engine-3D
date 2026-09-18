#include "Gameplay/Component.h"
#include "Gameplay/GameObject.h"

Component::Component()  = default;
Component::~Component() = default;

const Transform& Component::GetTransform() const
{
    assert(m_GameObject && "Component has no owning GameObject");
    return m_GameObject->GetTransform();
}

Transform& Component::GetTransform()
{
    assert(m_GameObject && "Component has no owning GameObject");
    return m_GameObject->GetTransform();
}
