#include "Editor.h"
#include "Engine.h"

Editor::Editor() = default;

Editor::~Editor()
{
    Shutdown();
}

void Editor::Initialize()
{
    // Созданием Subsystem редактора
}

void Editor::Tick(float DeltaTime)
{
    TickSubsystems(DeltaTime);
}

void Editor::Shutdown()
{
    ShutdownSubsystems();
}
