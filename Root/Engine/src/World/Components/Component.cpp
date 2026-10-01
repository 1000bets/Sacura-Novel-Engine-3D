#include "World/Components/Component.h"
#include "World/GameObject.h"

Component::Component()  = default;
Component::~Component() = default;

void Component::StartPlay()
{
    if (!bPlaying)
    {
        bPlaying = true;
        BeginPlay();
    }
}

void Component::StopPlay()
{
    if (bPlaying)
    {
        bPlaying = false;
        EndPlay();
    }
}

const Transform& Component::GetTransform() const
{
    assert(m_GameObject && "Component has no owning GameObject");
    return m_GameObject->GetTransform();
}

void Component::SetTransform(const Transform& InTransform)
{
    assert(m_GameObject && "Component has no owning GameObject");
    m_GameObject->SetTransform(InTransform);
}
