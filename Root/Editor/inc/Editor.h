#pragma once

#include "ISystem.h"

class Engine;

/// Редактор.  Второй владелец подсистем (Inspector, SceneHierarchy, и т.д.).

class Editor : public ISystem
{
public:
    Editor();
    ~Editor() override;

    /// Привязка к Engine — Editor знает о нём, чтобы обращаться
    /// к игровым подсистемам (WorldSubsystem, AssetSubsystem, …).
    void    SetEngine(Engine* InEngine) { m_Engine = InEngine; }
    Engine* GetEngine() const           { return m_Engine; }

    // ----- ISystem -----
    void Initialize() override;
    void Tick(float DeltaTime) override;
    void Shutdown() override;

private:
    Engine* m_Engine = nullptr;
};
