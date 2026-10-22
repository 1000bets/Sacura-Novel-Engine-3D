#include "UI/Adapters/StoryGraphController.h"
#include "UI/Adapters/StoryGraphAdapter.h"
#include "UI/Views/StoryInspectorView.h"
#include "UI/Views/StoryEventInspectorView.h"
#include "UI/Views/StoryEventPackageDialog.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Story/StoryEvents.h"
#include <QFile>

namespace EditorUi
{
void StoryGraphController::ExportSelectedEvent()
{
    if (bLocked)
    {
        return;
    }
    FlushEdits();
    const auto* Node = Document.FindNode(Selected.toStdString());
    if (!Node)
    {
        return;
    }
    for (const auto& Batch : Node->EventBatches)
    {
        for (const auto& Binding : Batch.Bindings)
        {
            if (Binding.Id != SelectedBinding.toStdString())
            {
                continue;
            }
            const auto* Event = FindStoryEvent(Document.GetStory(), Binding.EventId);
            if (!Event)
            {
                return;
            }
            StoryEventPackageForm Form{QString::fromStdString(Event->Name), QString::fromStdString(Event->Description)};
            QString Path;
            if (!StoryEventPackageDialog::Export(&Inspector, Form, Path))
            {
                return;
            }
            std::map<std::string, std::string> Names;
            for (const auto& Object : Objects)
            {
                Names.emplace(Object.first.toStdString(), Object.second.toStdString());
            }
            std::string Text, Error;
            const bool Success = Document.ExportEventPackage(Event->Id,
                                                             Inspector.EventInspector().IsEditingSource()
                                                                 ? std::vector<StoryActionDefinition>{}
                                                                 : Binding.ActionOverrides,
                                                             {Form.Name.toStdString(), Form.Description.toStdString(),
                                                              Form.Author.toStdString(), Form.License.toStdString()},
                                                             Names, Text, Error) &&
                                 AtomicFileWriter::WriteText(std::filesystem::path(Path.toStdWString()), Text, Error);
            Finish(Success, Error);
            return;
        }
    }
}
void StoryGraphController::ImportEvent(int RequestedPhase)
{
    if (bLocked)
    {
        return;
    }
    FlushEdits();
    const auto* Node = Document.FindNode(Selected.toStdString());
    if (!Node || (Node->Kind != StoryNodeKind::Line && Node->Kind != StoryNodeKind::Choice))
    {
        return;
    }
    const auto Path = StoryEventPackageDialog::Open(&Inspector);
    if (Path.isEmpty())
    {
        return;
    }
    QFile File(Path);
    if (!File.open(QIODevice::ReadOnly))
    {
        Finish(false, "Cannot open event package");
        return;
    }
    const auto Text = File.read(4 * 1024 * 1024 + 1).toStdString();
    StoryEventPackagePreview Preview;
    std::string Error;
    if (!PreviewStoryEventPackage(Text, Preview, Error))
    {
        Finish(false, Error);
        return;
    }
    StoryEventPackageForm Form{
        QString::fromStdString(Preview.Metadata.Name), QString::fromStdString(Preview.Metadata.Description),
        QString::fromStdString(Preview.Metadata.Author), QString::fromStdString(Preview.Metadata.License)};
    Form.Groups = static_cast<int>(Preview.Groups);
    Form.Actions = static_cast<int>(Preview.Actions);
    for (const auto& Input : Preview.Inputs)
    {
        Form.Inputs.push_back({QString::fromStdString(Input.Id), QString::fromStdString(Input.Label)});
    }
    auto Model = StoryGraphAdapter::Build(Document);
    Model.Objects = Objects;
    QMap<QString, QString> SelectedObjects;
    if (!StoryEventPackageDialog::Import(&Inspector, Form, Model, SelectedObjects))
    {
        return;
    }
    std::map<std::string, std::string> Bindings;
    std::vector<std::string> Available;
    for (auto It = SelectedObjects.begin(); It != SelectedObjects.end(); ++It)
    {
        Bindings.emplace(It.key().toStdString(), It.value().toStdString());
    }
    for (const auto& Object : Objects)
    {
        Available.push_back(Object.first.toStdString());
    }
    StoryEventPhase Phase = StoryEventPhase::OnStart;
    for (const auto& Batch : Node->EventBatches)
    {
        for (const auto& Binding : Batch.Bindings)
        {
            if (Binding.Id == SelectedBinding.toStdString())
            {
                Phase = Batch.Phase;
            }
        }
    }
    if (RequestedPhase >= 0 && RequestedPhase < 3)
    {
        Phase = static_cast<StoryEventPhase>(RequestedPhase);
    }
    std::string Created;
    const bool Success =
        Document.ImportEventPackage(Text, Bindings, Available, Selected.toStdString(), Phase, Created, Error);
    if (Success)
    {
        SelectedBinding = QString::fromStdString(Created);
        bForceInspector = true;
    }
    Finish(Success, Error);
}
} // namespace EditorUi
