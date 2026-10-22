#include "Shell/EditorMainWindow.h"
#include "UI/Adapters/AudioEnvironmentAdapter.h"
#include "UI/Adapters/StoryGraphController.h"
#include "UI/Adapters/WorkspaceDockController.h"
#include "UI/Views/AudioEnvironmentView.h"
#include "UI/Views/StoryWorkspaceView.h"
#include "UI/Views/StoryInspectorView.h"
#include "Engine.h"
#include "World/Simulation/PlaySession.h"
#include <QStackedWidget>
#include <QTabWidget>

void EditorMainWindow::BuildAudioEnvironmentDockPage()
{
    AudioEnvironment = new EditorUi::AudioEnvironmentView(DockTabs);
    DockTabs->addTab(AudioEnvironment, tr("Звук и окружение"));
    AudioEnvironment->OpenStoryRequested = [this]()
    {
        Docking->Open(*StoryGraph);
        InspectorPages->setCurrentWidget(StoryInspector);
        bStoryEditingContext = true;
        bWidgetEditingContext = false;
    };
    AudioEnvironment->PauseRequested = [this](uint64_t Identity, bool bPaused)
    {
        auto& Session = BoundEngine.GetPlaySession();
        if (Session.IsSimulating() && Session.GetState() != PlaySessionState::Paused)
        {
            StoryPlayback.SetEventPaused(Identity, bPaused);
            RefreshAudioEnvironment();
        }
    };
    AudioEnvironment->StopRequested = [this](uint64_t Identity)
    {
        if (BoundEngine.GetPlaySession().IsSimulating())
        {
            StoryPlayback.StopEvent(Identity);
            RefreshAudioEnvironment();
        }
    };
    RefreshAudioEnvironment();
}

void EditorMainWindow::RefreshAudioEnvironment()
{
    if (!AudioEnvironment)
    {
        return;
    }
    const auto& Session = BoundEngine.GetPlaySession();
    const auto* Story = StoryAuthoring && StoryAuthoring->GetDocument().IsOpen()
        ? &StoryAuthoring->GetDocument().GetStory() : nullptr;
    AudioEnvironment->SetModel(EditorUi::BuildAudioEnvironmentModel(Story, StoryPlayback.GetEventSnapshots(),
        Session.IsSimulating(), Session.GetState() == PlaySessionState::Paused, StoryPlayback.GetLastError()));
}
