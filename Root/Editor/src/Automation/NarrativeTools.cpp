#include "Automation/NarrativeTools.h"
#include "Automation/EditorToolset.h"
#include "UI/Adapters/StoryGraphController.h"
#include "Engine.h"
#include "Story/Narrative.h"
#include "Story/StoryDocumentIO.h"
#include "World/Scene.h"
#include <QJsonDocument>
#include <algorithm>

static QJsonValue NarrativeToolValue(const nlohmann::json& Value)
{
    return QJsonDocument::fromJson(QByteArray::fromStdString(nlohmann::json{{"value", Value}}.dump()))
        .object().value("value");
}

void RegisterNarrativeTools(EditorToolset& Toolset, Engine& Runtime, EditorUi::StoryGraphController& Controller)
{
    const QJsonObject Text{{"type", "string"}};
    const QJsonObject Number{{"type", "number"}};
    const auto CanEdit = [&Runtime]()
    {
        return !Runtime.GetPlaySession().IsSimulating() && !Runtime.IsGameRunning();
    };
    const auto Apply = [&Controller, CanEdit](const std::function<bool(StoryGraphDocument&, std::string&)>& Edit)
    {
        if (!CanEdit())
        {
            return EditorToolset::Failure("Stop Play before editing Narrative");
        }
        std::string Error;
        if (!Controller.ApplyDocumentEdit(Edit, Error))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        return EditorToolset::Success(true);
    };
    Toolset.Register("narrative_catalog", "List registered Narrative classes and component calls with typed pins and exits",
        {}, {}, true, [](const QJsonObject&)
        {
            auto Definitions = nlohmann::json::array();
            for (const auto& Definition : GetNarrativeDefinitions())
            {
                nlohmann::json Entry{{"type", Definition.Type}, {"label", Definition.Label}, {"pure", Definition.bPure},
                    {"exits", Definition.Exits}, {"inputs", nlohmann::json::array()}, {"outputs", nlohmann::json::array()}};
                const auto AddPins = [&](const char* Name, const std::vector<NarrativePin>& Pins)
                {
                    for (const auto& Pin : Pins)
                    {
                        nlohmann::json Data{{"name", Pin.Property.Id.Value}, {"type", Pin.Property.ValueTypeId.Value}};
                        if (Pin.bHasDefault)
                        {
                            Data["default"] = Pin.Default;
                        }
                        Entry[Name].push_back(std::move(Data));
                    }
                };
                AddPins("inputs", Definition.Inputs);
                AddPins("outputs", Definition.Outputs);
                Definitions.push_back(std::move(Entry));
            }
            return EditorToolset::Success(NarrativeToolValue(Definitions));
        });
    Toolset.Register("narrative_graph", "Inspect the active authored story, node positions and command history state",
        {}, {}, true, [&Controller](const QJsonObject&)
        {
            const auto& Document = Controller.GetDocument();
            if (!Document.IsOpen())
            {
                return EditorToolset::Failure("No story is open");
            }
            std::string Text;
            const auto Result = StoryDocumentIO::SaveToJson(Document.GetStory(), Text);
            if (!Result.bOk)
            {
                return EditorToolset::Failure(QString::fromStdString(Result.Error));
            }
            auto Data = nlohmann::json::parse(Text);
            for (const auto& [Identity, Position] : Document.GetPositions())
            {
                Data["positions"][Identity] = {{"x", Position.X}, {"y", Position.Y}};
            }
            Data["dirty"] = Document.IsDirty();
            Data["canUndo"] = Document.CanUndo();
            Data["canRedo"] = Document.CanRedo();
            return EditorToolset::Success(NarrativeToolValue(Data));
        });
    Toolset.Register("narrative_add", "Add a registered Narrative node to the active story; supports undo",
        {{"type", Text}, {"x", Number}, {"y", Number}}, {"type", "x", "y"}, false,
        [Apply](const QJsonObject& Arguments)
        {
            std::string Created;
            auto Result = Apply([&](StoryGraphDocument& Document, std::string& Error)
            {
                return Document.AddNarrativeNode(Arguments["type"].toString().toStdString(),
                    {Arguments["x"].toDouble(), Arguments["y"].toDouble()}, Created, Error);
            });
            if (!Result["isError"].toBool())
            {
                return EditorToolset::Success(QString::fromStdString(Created));
            }
            return Result;
        });
    Toolset.Register("narrative_input", "Set a typed literal input and disconnect its data wire; supports undo",
        {{"node", Text}, {"input", Text}, {"value", QJsonObject{}}}, {"node", "input", "value"}, false,
        [Apply, &Runtime](const QJsonObject& Arguments)
        {
            const auto Wrapped = nlohmann::json::parse(QJsonDocument(QJsonObject{{"value", Arguments["value"]}}).toJson().toStdString());
            return Apply([&](StoryGraphDocument& Document, std::string& Error)
            {
                ReflectionJson::ObjectResolver Resolver;
                if (const auto* World = Runtime.GetEditScene())
                {
                    Resolver = ReflectionJson::MakeSceneResolver(*World);
                }
                return Document.SetNarrativeInput(Arguments["node"].toString().toStdString(),
                    Arguments["input"].toString().toStdString(), Wrapped.at("value"), Error, Resolver);
            });
        });
    Toolset.Register("narrative_connect_data", "Connect compatible data pins; empty source disconnects the input",
        {{"source", Text}, {"output", Text}, {"target", Text}, {"input", Text}},
        {"source", "output", "target", "input"}, false, [Apply](const QJsonObject& Arguments)
        {
            return Apply([&](StoryGraphDocument& Document, std::string& Error)
            {
                return Document.ConnectData(Arguments["source"].toString().toStdString(), Arguments["output"].toString().toStdString(),
                    Arguments["target"].toString().toStdString(), Arguments["input"].toString().toStdString(), Error);
            });
        });
    Toolset.Register("narrative_connect_exit", "Connect a named Narrative execution exit; empty target disconnects it",
        {{"source", Text}, {"exit", Text}, {"target", Text}}, {"source", "exit", "target"}, false,
        [Apply](const QJsonObject& Arguments)
        {
            return Apply([&](StoryGraphDocument& Document, std::string& Error)
            {
                const auto* Node = Document.FindNode(Arguments["source"].toString().toStdString());
                if (Node != nullptr && Node->Kind == StoryNodeKind::Narrative)
                {
                    for (size_t Index = 0; Index < Node->Choices.size(); ++Index)
                    {
                        if (Node->Choices[Index].Id == Arguments["exit"].toString().toStdString())
                        {
                            return Document.Connect(Node->Id, static_cast<int>(Index), Arguments["target"].toString().toStdString(), Error);
                        }
                    }
                }
                Error = "Narrative execution exit does not exist";
                return false;
            });
        });
    Toolset.Register("narrative_node", "Delete, move or set a Narrative node as story entry; supports undo",
        {{"node", Text}, {"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"delete", "move", "set_start"}}}},
            {"x", Number}, {"y", Number}}, {"node", "operation"}, false,
        [Apply](const QJsonObject& Arguments)
        {
            return Apply([&](StoryGraphDocument& Document, std::string& Error)
            {
                const auto Identity = Arguments["node"].toString().toStdString();
                const auto* Node = Document.FindNode(Identity);
                if (Node == nullptr || Node->Kind != StoryNodeKind::Narrative)
                {
                    Error = "Narrative node does not exist";
                    return false;
                }
                const auto Operation = Arguments["operation"].toString();
                if (Operation == "delete")
                {
                    return Document.DeleteNodes({Identity}, Error);
                }
                if (Operation == "set_start")
                {
                    return Document.SetStart(Identity, Error);
                }
                if (!Arguments.contains("x") || !Arguments.contains("y"))
                {
                    Error = "Moving a node requires x and y";
                    return false;
                }
                return Document.MoveNodes({{Identity, {Arguments["x"].toDouble(), Arguments["y"].toDouble()}}}, Error);
            });
        });
    Toolset.Register("narrative_document", "Undo, redo, explicitly save or validate the active authored story",
        {{"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"undo", "redo", "save", "validate"}}}}},
        {"operation"}, false, [Apply](const QJsonObject& Arguments)
        {
            return Apply([&](StoryGraphDocument& Document, std::string& Error)
            {
                const auto Operation = Arguments["operation"].toString();
                if (Operation == "save")
                {
                    return Document.Save(Error);
                }
                if (Operation == "validate")
                {
                    return Document.ValidateForPlayback(Error);
                }
                bool bSucceeded = false;
                if (Operation == "undo")
                {
                    bSucceeded = Document.Undo();
                }
                else
                {
                    bSucceeded = Document.Redo();
                }
                if (!bSucceeded)
                {
                    Error = "No history operation is available";
                }
                return bSucceeded;
            });
        });
}
