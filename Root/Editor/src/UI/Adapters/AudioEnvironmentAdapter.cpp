#include "UI/Adapters/AudioEnvironmentAdapter.h"
#include "Story/StoryEvents.h"
#include <QCoreApplication>

namespace EditorUi
{
namespace
{
QString Text(const char* Value)
{
    return QCoreApplication::translate("AudioEnvironmentView", Value);
}
QString PhaseName(StoryEventPhase Phase)
{
    switch (Phase)
    {
    case StoryEventPhase::Before: return Text("До реплики");
    case StoryEventPhase::After: return Text("После ответа");
    default: return Text("Во время реплики");
    }
}
}

AudioEnvironmentModel BuildAudioEnvironmentModel(const StoryDocument* Story,
    const std::vector<StoryEventSnapshot>& Events, bool bPlaying, bool bPaused, const std::string& Error)
{
    AudioEnvironmentModel Model;
    Model.bCanOpenStory = Story != nullptr;
    Model.Notice = bPlaying
        ? (bPaused ? Text("Предпросмотр на паузе. Для продолжения нажмите «Пауза» на панели запуска.")
                   : Text("Управление ниже действует на текущий предпросмотр."))
        : Text("Предпросмотр остановлен. Ниже — события сценария; для проверки нажмите «Запуск».");
    Model.Footer = bPlaying ? Text("Показаны экземпляры событий текущего запуска. Остановка освобождает их действия.")
                           : Text("Начальные погода и время суток, музыка и озвучка пока не поддерживаются движком.");
    if (!Error.empty())
    {
        Model.Notice = Text("Ошибка предпросмотра: ") + QString::fromStdString(Error);
    }
    if (bPlaying)
    {
        for (const auto& Event : Events)
        {
            AudioEnvironmentRow Row;
            Row.InstanceId = Event.InstanceId;
            Row.Name = QString::fromStdString(Event.Name);
            Row.Origin = PhaseName(Event.Phase);
            Row.Scope = Text("Сценарий");
            Row.Icon = "Clapperboard";
            Row.bPaused = Event.State == StoryEventPlaybackState::Paused;
            const bool bActive = Event.State == StoryEventPlaybackState::Running || Row.bPaused;
            Row.bCanPause = bActive && Event.bCanPause && !bPaused;
            Row.bCanStop = bActive;
            Row.Status = bActive ? ((bPaused || Row.bPaused) ? Text("На паузе") : Text("Действует"))
                : Event.State == StoryEventPlaybackState::Cancelled ? Text("Остановлено") : Text("Завершено");
            Row.PauseHint = !bActive ? Text("Событие завершено")
                : !Event.bCanPause ? Text("Для событий с навигацией доступна остановка. Для паузы используйте паузу всего предпросмотра.")
                : bPaused ? Text("Сначала продолжите предпросмотр")
                : Row.bPaused ? Text("Продолжить событие") : Text("Приостановить событие");
            Model.Rows.push_back(std::move(Row));
        }
        if (Model.Rows.isEmpty() && Error.empty())
        {
            Model.Footer = Text("Сейчас нет запущенных событий. Продолжите сценарий в проигрывателе.");
        }
    }
    else if (Story)
    {
        for (const auto& Node : Story->Nodes)
        {
            for (const auto& Batch : Node.EventBatches)
            {
                for (const auto& Binding : Batch.Bindings)
                {
                    const auto* Event = FindStoryEvent(*Story, Binding.EventId);
                    if (!Event)
                    {
                        continue;
                    }
                    AudioEnvironmentRow Row;
                    Row.Name = QString::fromStdString(Event->Name);
                    Row.Origin = PhaseName(Batch.Phase);
                    const auto& Caption = Node.Kind == StoryNodeKind::Choice ? Node.Prompt : Node.Text;
                    if (!Caption.empty())
                    {
                        Row.Origin += " · " + QString::fromStdString(Caption);
                    }
                    Row.Status = Text("Задано в сценарии");
                    Row.Scope = Text("Сценарий");
                    Row.Icon = "Clapperboard";
                    Row.PauseHint = Text("Управление доступно во время запуска");
                    Model.Rows.push_back(std::move(Row));
                }
            }
        }
    }
    if (!bPlaying && Model.Rows.isEmpty())
    {
        Model.Notice = Story ? Text("Предпросмотр остановлен. В сценарии ещё нет событий.")
                             : Text("Откройте историю в Content, чтобы увидеть события сценария.");
    }
    return Model;
}
}
