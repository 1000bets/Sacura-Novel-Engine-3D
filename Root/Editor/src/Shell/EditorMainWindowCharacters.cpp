#include "UI/Adapters/StoryGraphController.h"
#include "Shell/EditorMainWindow.h"
#include "UI/Views/CharacterWorkspaceView.h"
#include "Automation/EditorToolset.h"
#include "Engine.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include "Panels/ContentBrowserWidget.h"
#include <QJsonDocument>
#include <QMessageBox>

CharacterWorkspaceView* EditorMainWindow::OpenCharacterWorkspace()
{
    if (!CharacterWorkspace)
    {
        CharacterWorkspace = new CharacterWorkspaceView(BoundEngine, Characters, this);
        CharacterWorkspace->Place = [this](const AssetRegistryEntry& Entry)
        {
            std::string Error;
            ObjectHandle Created;
            if (!PlaceCharacter(Entry, Created, Error))
            {
                QMessageBox::warning(this, tr("Персонаж"), QString::fromStdString(Error));
            }
        };
    }
    CharacterWorkspace->Refresh();
    CharacterWorkspace->show();
    CharacterWorkspace->raise();
    return CharacterWorkspace;
}

bool EditorMainWindow::PlaceCharacter(const AssetRegistryEntry& Entry, ObjectHandle& Created, std::string& Error)
{
    if (BoundEngine.GetPlaySession().IsSimulating() || BoundEngine.IsGameRunning())
    {
        Error = "Stop Play before placing a character";
        return false;
    }
    if (!InstantiateCharacter(GetEditScene(), CommandStack, Entry, Created, Error))
    {
        return false;
    }
    PopulateHierarchy();
    SelectObject(Created);
    UpdateWindowTitleDirty();
    return true;
}

void EditorMainWindow::RegisterCharacterTools()
{
    const QJsonObject Text{{"type", "string"}};
    const auto CanEdit = [this]()
    {
        return !BoundEngine.GetPlaySession().IsSimulating() && !BoundEngine.IsGameRunning()
            && !BoundEngine.GetAssetRegistry().GetGameContentRoot().empty();
    };
    Toolset->Register("character_list", "List character assets", {}, {}, true,
        [this](const QJsonObject&)
        {
            QJsonArray Entries;
            for (const auto& Entry : BoundEngine.GetAssetRegistry().FindByType(CharacterAssetType))
            {
                Entries.append(QJsonObject{{"path", QString::fromStdString(Entry.VirtualPath)},
                    {"asset", QString::fromStdString(Entry.Metadata.Guid.ToString())}});
            }
            return EditorToolset::Success(Entries);
        });
    Toolset->Register("character_create", "Create a character asset or duplicate an existing character",
        {{"name", Text}, {"source", Text}}, {"name"}, false, [this, CanEdit](const QJsonObject& Arguments)
        {
            if (!CanEdit())
            {
                return EditorToolset::Failure("Open a project and stop Play before editing characters");
            }
            auto Data = CharacterDefinition::Create(Arguments.value("name").toString().toStdString());
            std::string Error;
            AssetRegistryEntry Entry;
            if (Arguments.contains("source"))
            {
                if (!BoundEngine.GetAssetRegistry().TryGetByPath(Arguments.value("source").toString().toStdString(), Entry))
                {
                    return EditorToolset::Failure("Source character not found");
                }
                const auto Source = Characters.Open(BoundEngine.GetAssetRegistry(), Entry, Error);
                if (!Source)
                {
                    return EditorToolset::Failure(QString::fromStdString(Error));
                }
                Data = Source->GetData();
                Data.Name = Arguments.value("name").toString().toStdString();
            }
            if (!CharacterDocument::Create(BoundEngine.GetAssetRegistry(), Data, Entry, Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            if (CharacterWorkspace)
            {
                CharacterWorkspace->Refresh();
            }
            return EditorToolset::Success(QJsonObject{{"path", QString::fromStdString(Entry.VirtualPath)},
                {"asset", QString::fromStdString(Entry.Metadata.Guid.ToString())}});
        });
    Toolset->Register("character_document", "Inspect, edit, undo, redo, save, reload or import a character draft shared with the UI",
        {{"path", Text}, {"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{
            "inspect", "apply", "undo", "redo", "save", "reload", "import_model", "import_animations"}}}},
            {"document", QJsonObject{{"type", "object"}}}, {"source", Text}}, {"path", "operation"}, false,
        [this, CanEdit](const QJsonObject& Arguments)
        {
            const auto Operation = Arguments.value("operation").toString();
            if (Operation != "inspect" && !CanEdit())
            {
                return EditorToolset::Failure("Stop Play before editing characters");
            }
            AssetRegistryEntry Entry;
            if (!BoundEngine.GetAssetRegistry().TryGetByPath(Arguments.value("path").toString().toStdString(), Entry)
                || Entry.Metadata.Type != CharacterAssetType)
            {
                return EditorToolset::Failure("Character not found");
            }
            std::string Error;
            auto Draft = Characters.Open(BoundEngine.GetAssetRegistry(), Entry, Error);
            if (!Draft)
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            bool bSucceeded = true;
            if (Operation == "apply")
            {
                const auto Data = nlohmann::json::parse(QJsonDocument(Arguments.value("document").toObject()).toJson().toStdString());
                bSucceeded = Draft->Apply(Data, Error);
            }
            else if (Operation == "undo")
            {
                Draft->Undo();
            }
            else if (Operation == "redo")
            {
                Draft->Redo();
            }
            else if (Operation == "save")
            {
                bSucceeded = Draft->Save(BoundEngine.GetAssetManager(), Error);
            }
            else if (Operation == "reload")
            {
                bSucceeded = Draft->Open(BoundEngine.GetAssetRegistry(), Entry, Error);
            }
            else if (Operation == "import_model" || Operation == "import_animations")
            {
                bSucceeded = Draft->ImportModel(BoundEngine.GetAssetRegistry(),
                    std::filesystem::path(Arguments.value("source").toString().toStdWString()), Operation == "import_animations", Error);
            }
            if (!bSucceeded)
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            if (CharacterWorkspace && Operation != "inspect")
            {
                CharacterWorkspace->Refresh();
            }
            return EditorToolset::Success(QJsonObject{{"document", QJsonDocument::fromJson(
                QByteArray::fromStdString(Draft->GetData().ToJson().dump())).object()}, {"dirty", Draft->IsDirty()}});
        });
    Toolset->Register("character_delete", "Delete a project character asset and its metadata; scene and story references remain unresolved",
        {{"path", Text}}, {"path"}, false, [this, CanEdit](const QJsonObject& Arguments)
        {
            if (!CanEdit())
            {
                return EditorToolset::Failure("Stop Play before deleting characters");
            }
            AssetRegistryEntry Entry;
            std::string Error;
            if (!BoundEngine.GetAssetRegistry().TryGetByPath(Arguments.value("path").toString().toStdString(), Entry))
            {
                return EditorToolset::Failure("Character not found");
            }
            if (!Characters.Delete(BoundEngine.GetAssetRegistry(), BoundEngine.GetAssetManager(), Entry, Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            if (CharacterWorkspace)
            {
                CharacterWorkspace->Refresh();
            }
            return EditorToolset::Success(QJsonObject{{"deleted", true}});
        });
    Toolset->Register("character_story_action", "Append a character animation action to an action node in the open story; saving remains explicit",
        {{"node", Text}, {"targetObjectId", Text}, {"animationId", Text},
            {"durationSeconds", QJsonObject{{"type", "number"}, {"minimum", 0}}}},
        {"node", "targetObjectId", "animationId"}, false, [this, CanEdit](const QJsonObject& Arguments)
        {
            if (!CanEdit() || !StoryAuthoring)
            {
                return EditorToolset::Failure("Open a story and stop Play before editing actions");
            }
            std::string Error;
            const bool bChanged = StoryAuthoring->ApplyDocumentEdit([&](StoryGraphDocument& Document, std::string& Message)
            {
                const auto* Existing = Document.FindNode(Arguments.value("node").toString().toStdString());
                if (!Existing || Existing->Kind != StoryNodeKind::Action)
                {
                    Message = "An existing action node is required";
                    return false;
                }
                StoryNode Node = *Existing;
                StoryActionDefinition Action;
                Action.Id = Guid::Generate().ToString();
                Action.Kind = StoryActionKind::CharacterAnimation;
                Action.TargetObjectId = Arguments.value("targetObjectId").toString().toStdString();
                Action.AnimationId = Arguments.value("animationId").toString().toStdString();
                Action.DurationSeconds = static_cast<float>(Arguments.value("durationSeconds").toDouble());
                Node.Actions.push_back(std::move(Action));
                return Document.ReplaceNode(Node, Message);
            }, Error);
            if (!bChanged)
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            return EditorToolset::Success(QJsonObject{{"changed", true}});
        });
    Toolset->Register("character_instantiate", "Place a character in the edit scene through Undo history",
        {{"path", Text}}, {"path"}, false, [this, CanEdit](const QJsonObject& Arguments)
        {
            if (!CanEdit())
            {
                return EditorToolset::Failure("Stop Play before placing characters");
            }
            AssetRegistryEntry Entry;
            if (!BoundEngine.GetAssetRegistry().TryGetByPath(Arguments.value("path").toString().toStdString(), Entry))
            {
                return EditorToolset::Failure("Character not found");
            }
            ObjectHandle Created;
            std::string Error;
            if (!PlaceCharacter(Entry, Created, Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            return EditorToolset::Success(QJsonObject{{"objectId", QString::fromStdString(
                GetEditScene()->FindByHandle(Created)->GetPersistentId())}});
        });
}
