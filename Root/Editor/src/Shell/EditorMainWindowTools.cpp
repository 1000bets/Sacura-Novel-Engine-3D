#include "Assets/AssetOperations.h"
#include "Assets/AssetCreationFactory.h"
#include "UI/Views/AssetReferenceField.h"
#include "Panels/ContentBrowserWidget.h"
#include "UI/Adapters/WorkspaceDockController.h"
#include "Commands/StaticMeshCommands.h"
#include "World/Components/MeshRendererComponent.h"
#include "World/Components/PhysicsBodyComponent.h"
#include <cmath>
#include "Shell/EditorMainWindow.h"
#include "Automation/EditorToolset.h"
#include "Automation/InputTools.h"
#include "Automation/EditorMcpServer.h"
#include "Commands/EditorCommands.h"
#include "Core/Threading/ThreadContext.h"
#include "Engine.h"
#include "Navigation/NavigationWorld.h"
#include "Project/ProjectSession.h"
#include "Project/GameExporter.h"
#include "Core/EnginePaths.h"
#include <QCoreApplication>
#include <QFileDialog>
#include <QMessageBox>
#include "Reflection/Class.h"
#include "Reflection/Json/ReflectionJson.h"
#include "Reflection/ReflectionSubsystem.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
#include "World/Simulation/PlaySession.h"
#include <QCompleter>
#include <QDoubleSpinBox>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <algorithm>

EditorToolset& EditorMainWindow::GetToolset() { return *Toolset; }
EditorMcpServer& EditorMainWindow::GetMcpServer() { return *McpServer; }

void EditorMainWindow::InitializeTools()
{
    Toolset = std::make_unique<EditorToolset>();
    Toolset->Audit = [](const QString& Message) { PrintString(Message.toStdString()); };
    McpServer = std::make_unique<EditorMcpServer>(*Toolset, this);
    RegisterPrefabTools();
    RegisterInputTools(*Toolset, BoundEngine);
    RegisterMaterialTools();
    RegisterStaticMeshTools();
    RegisterCharacterTools();
    const QJsonObject Text{{"type", "string"}};
    const QJsonObject Number{{"type", "number"}};
    const QJsonObject Boolean{{"type", "boolean"}};
    Toolset->Register("asset_creation_types", "List reflected asset factories and their creation settings", {}, {}, true,
        [](const QJsonObject&)
        {
            return EditorToolset::Success(QJsonDocument::fromJson(QByteArray::fromStdString(AssetCreationFactory::DescribeFactories().dump())).array());
        });
    Toolset->Register("asset_create", "Create an asset using a reflected factory and its validated settings",
        {{"factoryId", Text}, {"path", Text}, {"settings", QJsonObject{{"type", "object"}}}}, {"factoryId", "path"}, false,
        [this](const QJsonObject& Arguments)
        {
            if (!BoundSession.GetProject() || BoundEngine.GetPlaySession().IsSimulating())
            { return EditorToolset::Failure("Open a project and stop Play before creating assets"); }
            const auto Settings = nlohmann::json::parse(QJsonDocument(Arguments.value("settings").toObject()).toJson().toStdString());
            AssetRegistryEntry Created;
            std::string Error;
            if (!AssetCreationFactory::CreateAsset(BoundEngine.GetAssetRegistry(),
                TypeId{Arguments.value("factoryId").toString().toStdString()}, Arguments.value("path").toString().toStdString(), Settings, Created, Error))
            { return EditorToolset::Failure(QString::fromStdString(Error)); }
            ContentBrowser->Refresh();
            return EditorToolset::Success(QJsonObject{{"id", QString::fromStdString(Created.Metadata.Guid.ToString())},
                {"path", QString::fromStdString(Created.VirtualPath)}, {"typeId", QString::fromStdString(Created.Metadata.Type.GetIdentifier())}});
        });
    for (const QString& Operation : {QString("asset_rename"), QString("asset_move"), QString("asset_delete"), QString("asset_duplicate"), QString("folder_create"), QString("folder_rename"), QString("folder_delete")})
    {
        QJsonObject Properties;
        QJsonArray Required;
        const bool bFolder = Operation.startsWith("folder_");
        QString Identity = "assetId";
        if (bFolder)
        {
            Identity = "folder";
        }
        Properties[Identity] = Text;
        Required.append(Identity);
        if (Operation.endsWith("rename") || Operation == "folder_create")
        {
            Properties["name"] = Text;
            Required.append("name");
        }
        if (Operation == "asset_move" || Operation == "asset_duplicate")
        {
            Properties["folder"] = Text;
            Required.append("folder");
        }
        Toolset->Register(Operation, "Modify Content assets through shared EditorCore asset operations", Properties, Required, false,
            [this, Operation, bFolder](const QJsonObject& Arguments)
            {
                if (!BoundSession.GetProject() || BoundEngine.GetPlaySession().IsSimulating())
                {
                    return EditorToolset::Failure("Open a project and stop Play before modifying assets");
                }
                AssetOperations Operations(BoundEngine);
                Operations.SetProjectContext(&BoundSession, &Workspace.GetDocument());
                const auto Folder = Arguments.value("folder").toString().toStdString();
                const auto Name = Arguments.value("name").toString().toStdString();
                if (Arguments.contains("name") && (Name.empty() || Name.find_first_of("./\\:*?\"<>|") != std::string::npos || Name.back() == ' '))
                {
                    return EditorToolset::Failure("Enter a single valid name without an extension");
                }
                AssetDiagnostic Result;
                if (bFolder)
                {
                    if (Operation == "folder_create")
                    {
                        Result = Operations.CreateFolder(Folder + "/" + Name);
                    }
                    else if (Operation == "folder_rename")
                    {
                        Result = Operations.RenameFolder(Folder, std::filesystem::u8path(Folder).parent_path().generic_u8string() + "/" + Name);
                    }
                    else
                    {
                        Result = Operations.DeleteFolder(Folder);
                    }
                }
                else
                {
                    AssetKey Key;
                    if (!Guid::TryParse(Arguments.value("assetId").toString().toStdString(), Key.Asset) || !Key.IsValid())
                    {
                        return EditorToolset::Failure("Invalid asset identifier");
                    }
                    AssetRegistryEntry Entry;
                    if (!BoundEngine.GetAssetRegistry().TryResolveKey(Key, Entry))
                    {
                        return EditorToolset::Failure("Asset is unavailable");
                    }
                    if (Operation == "asset_delete")
                    {
                        Result = Operations.DeleteAsset(Key);
                    }
                    else if (Operation == "asset_duplicate")
                    {
                        Result = Operations.DuplicateAsset(Key, Folder);
                    }
                    else
                    {
                        const auto Source = std::filesystem::u8path(Entry.VirtualPath);
                        std::string Destination = Folder + "/" + Source.filename().u8string();
                        if (Operation == "asset_rename")
                        {
                            Destination = (Source.parent_path() / std::filesystem::u8path(Name + Source.extension().u8string())).generic_u8string();
                        }
                        Result = Operations.RenameAsset(Entry.VirtualPath, Destination);
                    }
                }
                if (Result.HasError())
                {
                    return EditorToolset::Failure(QString::fromStdString(Result.Message));
                }
                ContentBrowser->Refresh();
                return EditorToolset::Success(QJsonObject{{"success", true}});
            });
    }
    AssetReferenceField::SetNavigation(*this, [this](const AssetKey& Key, bool bOpen)
    {
        QJsonObject Arguments{{"assetId", QString::fromStdString(Key.Asset.ToString())}};
        if (Key.HasSubAsset())
        {
            Arguments.insert("subAssetId", QString::fromStdString(Key.SubAsset->ToString()));
        }
        if (bOpen)
        {
            Toolset->Call("asset_open", Arguments);
        }
        else
        {
            Toolset->Call("asset_focus", Arguments);
            raise();
            activateWindow();
        }
    });
    Toolset->Register("asset_open", "Open an asset or subasset through its registered editor",
        {{"assetId", Text}, {"subAssetId", Text}}, {"assetId"}, false, [this](const QJsonObject& Arguments)
    {
        AssetKey Key;
        if (!Guid::TryParse(Arguments.value("assetId").toString().toStdString(), Key.Asset) || !Key.IsValid())
        {
            return EditorToolset::Failure("Invalid asset identifier");
        }
        const QString SubAssetIdentifier = Arguments.value("subAssetId").toString();
        if (!SubAssetIdentifier.isEmpty())
        {
            SubAssetId Identifier;
            if (!Guid::TryParse(SubAssetIdentifier.toStdString(), Identifier) || !Identifier.IsValid())
            {
                return EditorToolset::Failure("Invalid subasset identifier");
            }
            Key.SubAsset = Identifier;
        }
        AssetRegistryEntry Entry;
        const SubAssetRecord* SubAsset = nullptr;
        if (!BoundEngine.GetAssetRegistry().TryResolveKey(Key, Entry, &SubAsset))
        {
            return EditorToolset::Failure("Asset is unavailable");
        }
        AssetType Type = Entry.Metadata.Type;
        if (SubAsset)
        {
            Type = SubAsset->Type;
        }
        if (AssetEditors.find(Type) == AssetEditors.end())
        {
            return Toolset->Call("asset_focus", Arguments);
        }
        OnAssetActivated(Arguments.value("assetId").toString(), SubAssetIdentifier,
            QString::fromStdString(Type.GetIdentifier()), QString::fromStdString(Entry.VirtualPath));
        return EditorToolset::Success(true);
    });
    Toolset->Register("project_export_game", "Export saved project files and the installed Player into a new standalone Windows folder; does not save drafts",
        {{"destination", Text}}, {"destination"}, false, [this](const QJsonObject& Arguments)
    {
        const auto* Project = BoundSession.GetProject();
        if (Project == nullptr || BoundEngine.GetPlaySession().IsSimulating() || bPlayRequested)
        {
            return EditorToolset::Failure("Open a project and stop Play before exporting.");
        }
        std::string Error;
        const auto Destination = std::filesystem::path(Arguments.value("destination").toString().toStdWString());
        if (!GameExporter::Export(Project->ProjectFile,
            std::filesystem::path(QCoreApplication::applicationDirPath().toStdWString()), EnginePaths::Root(), Destination, Error))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        return EditorToolset::Success(QString::fromStdWString((Destination / "Game.exe").wstring()));
    });
    Toolset->Register("asset_focus", "Reveal and select an asset or subasset in the Content Browser; clears search and type filters",
        {{"assetId", Text}, {"subAssetId", Text}}, {"assetId"}, false, [this](const QJsonObject& Arguments)
    {
        AssetKey Key;
        if (!Guid::TryParse(Arguments.value("assetId").toString().toStdString(), Key.Asset) || !Key.IsValid())
        {
            return EditorToolset::Failure("Invalid asset identifier");
        }
        const auto SubAssetIdentifier = Arguments.value("subAssetId").toString();
        if (!SubAssetIdentifier.isEmpty())
        {
            SubAssetId Identifier;
            if (!Guid::TryParse(SubAssetIdentifier.toStdString(), Identifier) || !Identifier.IsValid())
            {
                return EditorToolset::Failure("Invalid subasset identifier");
            }
            Key.SubAsset = Identifier;
        }
        if (!ContentBrowser->FocusAsset(Key))
        {
            return EditorToolset::Failure("Asset is unavailable in the Content Browser");
        }
        Docking->Open(*ContentBrowser);
        return EditorToolset::Success(true);
    });
    QJsonArray VisualizationNames;
    for (const auto& Descriptor : RenderVisualizations)
    {
        VisualizationNames.append(Descriptor.Identifier);
    }
    Toolset->Register("viewport_visualization", "Inspect or change the primary viewport diagnostic mode; does not change scene data",
        {{"mode", QJsonObject{{"type", "string"}, {"enum", VisualizationNames}}},
         {"depthRange", QJsonObject{{"type", "number"}, {"minimum", 0.01}, {"maximum", 100000.0}}}}, {}, false,
        [this](const QJsonObject& Arguments)
    {
        RenderVisualization Mode = RenderConfiguration.Visualization;
        if (Arguments.contains("mode"))
        {
            const auto* Descriptor = FindRenderVisualization(Arguments.value("mode").toString().toStdString());
            if (!Descriptor)
            {
                return EditorToolset::Failure("Unknown viewport visualization mode");
            }
            Mode = Descriptor->Mode;
        }
        const double DepthRange = Arguments.value("depthRange").toDouble(RenderConfiguration.VisualizationDepthRange);
        if (!std::isfinite(DepthRange) || DepthRange < 0.01 || DepthRange > 100000.0)
        {
            return EditorToolset::Failure("Depth range must be between 0.01 and 100000 metres");
        }
        SetViewportVisualization(Mode, static_cast<float>(DepthRange));
        QJsonArray Catalog;
        for (const auto& Descriptor : RenderVisualizations)
        {
            Catalog.append(QJsonObject{{"mode", Descriptor.Identifier}, {"description", QString::fromUtf8(Descriptor.Description)}});
        }
        return EditorToolset::Success(QJsonObject{{"mode", RenderVisualizations[static_cast<size_t>(Mode)].Identifier},
            {"depthRange", DepthRange}, {"modes", Catalog}});
    });
    auto Outcome = [](bool bSucceeded)
    {
        if (!bSucceeded)
        {
            return EditorToolset::Failure("Operation rejected; check the editor state and log");
        }
        return EditorToolset::Success(true);
    };
    auto Resolve = [this](const QJsonObject& Arguments) -> GameObject*
    {
        Scene* World = GetEditScene();
        if (World == nullptr)
        {
            return nullptr;
        }
        return World->FindByPersistentId(Arguments.value("objectId").toString().toStdString());
    };
    Toolset->Register("editor_status", "Project, scene, dirty and play state", {}, {}, true, [this](const QJsonObject&)
    {
        return EditorToolset::Success(QJsonObject{{"project", ReadProjectSettings()},
            {"scene", QString::fromStdWString(Document.GetDocumentPath().wstring())}, {"dirty", Document.IsDirty()},
            {"playing", BoundEngine.GetPlaySession().IsSimulating()}, {"playPending", bPlayRequested},
            {"canUndo", CommandStack.CanUndo()}, {"canRedo", CommandStack.CanRedo()}});
    });
    Toolset->Register("scene_inspect", "Serialized edit scene with persistent object and component IDs", {}, {}, true, [this](const QJsonObject&)
    {
        if (GetEditScene() == nullptr)
        {
            return EditorToolset::Failure("No scene is open");
        }
        std::string Serialized;
        const auto Result = SceneSerializer::SerializeToJson(*GetEditScene(), Serialized);
        if (!Result.bOk)
        {
            return EditorToolset::Failure(QString::fromStdString(Result.Error));
        }
        return EditorToolset::Success(QJsonDocument::fromJson(QByteArray::fromStdString(Serialized)).object());
    });
    Toolset->Register("scene_save", "Save the current scene to its existing path", {}, {}, false, [this](const QJsonObject&)
    {
        std::string Error;
        if (!Document.Save(Error))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        UpdateWindowTitleDirty();
        return EditorToolset::Success(true);
    });
    Toolset->Register("object_create", "Create an empty scene object; supports undo", {{"name", Text}}, {"name"}, false, [this](const QJsonObject& Arguments)
    {
        const QString Name = Arguments.value("name").toString().trimmed();
        if (Name.isEmpty())
        {
            return EditorToolset::Failure("Object name cannot be empty");
        }
        ObjectHandle Created;
        if (!ExecuteCommand(MakeCreateObjectCommand(GetEditScene(), Name.toStdString(), {}, &Created)))
        {
            return EditorToolset::Failure("Cannot create object in the current editor state");
        }
        GameObject* ObjectInstance = GetEditScene()->FindByHandle(Created);
        return EditorToolset::Success(QString::fromStdString(ObjectInstance->GetPersistentId()));
    });
    auto ResolveMesh = [Resolve](const QJsonObject& Arguments) -> MeshRendererComponent*
    {
        auto* Object = Resolve(Arguments);
        if (!Object)
        {
            return nullptr;
        }
        return Object->GetComponent<MeshRendererComponent>();
    };
    Toolset->Register("mesh_material_assign", "Assign a Surface material to one slot or all slots; empty materialId restores mesh materials",
        {{"objectId", Text}, {"slot", QJsonObject{{"type", "integer"}, {"minimum", -1}, {"maximum", 2147483647}}}, {"materialId", Text}},
        {"objectId", "slot", "materialId"}, false, [this, ResolveMesh](const QJsonObject& Arguments)
    {
        auto* Mesh = ResolveMesh(Arguments);
        if (!Mesh || BoundEngine.GetPlaySession().IsSimulating() || bPlayRequested)
        {
            return EditorToolset::Failure("Select a mesh object and stop Play before assigning materials");
        }
        AssetKey Material;
        const auto Identifier = Arguments.value("materialId").toString().toStdString();
        if (!Identifier.empty() && !Guid::TryParse(Identifier, Material.Asset))
        {
            return EditorToolset::Failure("Invalid material asset ID");
        }
        std::string Error;
        auto Command = MakeAssignMeshMaterialCommand(GetEditScene(), BoundEngine.GetAssetRegistry(), Mesh->GetObjectHandle(),
            Arguments.value("slot").toInt(), Material, Error);
        if (!Command || !ExecuteCommand(std::move(Command)))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        return EditorToolset::Success(true);
    });
    Toolset->Register("mesh_physics_set", "Enable mesh collision and optionally simulate a dynamic body; preserves existing physics settings and supports undo",
        {{"objectId", Text}, {"enabled", Boolean}, {"simulate", Boolean}}, {"objectId", "enabled", "simulate"}, false,
        [this, ResolveMesh](const QJsonObject& Arguments)
    {
        auto* Mesh = ResolveMesh(Arguments);
        if (!Mesh || BoundEngine.GetPlaySession().IsSimulating() || bPlayRequested)
        {
            return EditorToolset::Failure("Select a mesh object and stop Play before changing physics");
        }
        const bool bEnabled = Arguments.value("enabled").toBool();
        const bool bSimulate = Arguments.value("simulate").toBool();
        if (!bEnabled && bSimulate)
        {
            return EditorToolset::Failure("Enable collision before simulating physics");
        }
        if (!bEnabled && !Mesh->GetGameObject()->GetComponent<PhysicsBodyComponent>())
        {
            return EditorToolset::Success(true);
        }
        std::string Error;
        auto Command = MakeSetMeshPhysicsCommand(GetEditScene(), BoundEngine.GetAssetRegistry(), Mesh->GetObjectHandle(), bEnabled, bSimulate, Error);
        if (!Command || !ExecuteCommand(std::move(Command)))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        return EditorToolset::Success(true);
    });
    Toolset->Register("object_rename", "Rename an object by persistent ID; supports undo", {{"objectId", Text}, {"name", Text}}, {"objectId", "name"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Target = Resolve(Arguments);
        if (Target == nullptr || Arguments.value("name").toString().trimmed().isEmpty())
        {
            return EditorToolset::Failure("Object not found or name is empty");
        }
        return Outcome(ExecuteCommand(MakeRenameObjectCommand(GetEditScene(), Target->GetObjectHandle(), Arguments.value("name").toString().toStdString())));
    });
    Toolset->Register("object_delete", "Delete an object subtree; supports undo", {{"objectId", Text}}, {"objectId"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Target = Resolve(Arguments);
        if (Target == nullptr)
        {
            return EditorToolset::Failure("Object not found");
        }
        return Outcome(ExecuteCommand(MakeDeleteObjectCommand(GetEditScene(), Target->GetObjectHandle())));
    });
    Toolset->Register("object_select", "Select an edit-world object", {{"objectId", Text}}, {"objectId"}, false,
        [this, Resolve](const QJsonObject& Arguments)
    {
        GameObject* Target = Resolve(Arguments);
        if (Target == nullptr || BoundEngine.GetPlaySession().IsSimulating())
        {
            return EditorToolset::Failure("Edit object selection is unavailable");
        }
        SelectObject(Target->GetObjectHandle());
        return EditorToolset::Success(true);
    });
    Toolset->Register("object_reparent", "Reparent an object; empty parentId means scene root", {{"objectId", Text}, {"parentId", Text}}, {"objectId", "parentId"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Target = Resolve(Arguments);
        const std::string ParentId = Arguments.value("parentId").toString().toStdString();
        if (Target == nullptr)
        {
            return EditorToolset::Failure("Object not found");
        }
        ObjectHandle Parent;
        if (!ParentId.empty())
        {
            GameObject* ParentObject = GetEditScene()->FindByPersistentId(ParentId);
            if (ParentObject == nullptr)
            {
                return EditorToolset::Failure("Parent object not found");
            }
            Parent = ParentObject->GetObjectHandle();
        }
        return Outcome(ExecuteCommand(MakeReparentObjectCommand(GetEditScene(), Target->GetObjectHandle(), Parent)));
    });
    Toolset->Register("object_position", "Set local position in metres; supports undo", {{"objectId", Text}, {"x", Number}, {"y", Number}, {"z", Number}}, {"objectId", "x", "y", "z"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Target = Resolve(Arguments);
        if (Target == nullptr)
        {
            return EditorToolset::Failure("Object not found");
        }
        for (const QString& Axis : {QString("x"), QString("y"), QString("z")})
        {
            if (std::abs(Arguments.value(Axis).toDouble()) > 1.0e8)
            {
                return EditorToolset::Failure("Position is outside the supported range");
            }
        }
        Transform Updated = Target->GetTransform();
        Updated.Position = Vector3(static_cast<float>(Arguments.value("x").toDouble()), static_cast<float>(Arguments.value("y").toDouble()), static_cast<float>(Arguments.value("z").toDouble()));
        return Outcome(ExecuteCommand(MakeSetTransformCommand(GetEditScene(), Target->GetObjectHandle(), Updated)));
    });
    Toolset->Register("component_add", "Add a reflected component by type ID; supports undo", {{"objectId", Text}, {"typeId", Text}}, {"objectId", "typeId"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Target = Resolve(Arguments);
        if (Target == nullptr)
        {
            return EditorToolset::Failure("Object not found");
        }
        return Outcome(ExecuteCommand(MakeAddComponentCommand(GetEditScene(), Target->GetObjectHandle(), TypeId{Arguments.value("typeId").toString().toStdString()})));
    });
    Toolset->Register("component_property_set", "Set a reflected component property using its serialized JSON value; supports undo",
        {{"objectId", Text}, {"componentId", Text}, {"property", Text}, {"value", QJsonObject{}}}, {"objectId", "componentId", "property", "value"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Owner = Resolve(Arguments);
        if (Owner == nullptr)
        {
            return EditorToolset::Failure("Object not found");
        }
        for (Component* Target : Owner->GetAllComponents())
        {
            if (Target->GetPersistentId() != Arguments.value("componentId").toString().toStdString())
            {
                continue;
            }
            const PropertyId Property{Arguments.value("property").toString().toStdString()};
            const PropertyDescriptor* Descriptor = Target->GetClass()->FindProperty(Property);
            if (Descriptor == nullptr || Descriptor->bReadOnly)
            {
                return EditorToolset::Failure("Writable property not found");
            }
            const auto Json = nlohmann::json::parse(QJsonDocument(Arguments).toJson(QJsonDocument::Compact).toStdString());
            ReflectedValue Value;
            const auto Converted = ReflectionJson::JsonToReflectedValue(Descriptor->ValueTypeId, Json.at("value"), Value, ReflectionJson::MakeSceneResolver(*GetEditScene()));
            if (!Converted.bOk)
            {
                return EditorToolset::Failure(QString::fromStdString(Converted.Message));
            }
            return Outcome(ExecuteCommand(MakeSetPropertyCommand(GetEditScene(), Target->GetObjectHandle(), Property, std::move(Value))));
        }
        return EditorToolset::Failure("Component not found");
    });
    Toolset->Register("component_remove", "Remove a component by persistent ID; supports undo", {{"objectId", Text}, {"componentId", Text}}, {"objectId", "componentId"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Owner = Resolve(Arguments);
        if (Owner == nullptr)
        {
            return EditorToolset::Failure("Object not found");
        }
        for (Component* Target : Owner->GetAllComponents())
        {
            if (Target->GetPersistentId() == Arguments.value("componentId").toString().toStdString())
            {
                return Outcome(ExecuteCommand(MakeRemoveComponentCommand(GetEditScene(), Owner->GetObjectHandle(), Target->GetObjectHandle())));
            }
        }
        return EditorToolset::Failure("Component not found");
    });
    Toolset->Register("object_active", "Set an object's active flag; supports undo", {{"objectId", Text}, {"active", Boolean}}, {"objectId", "active"}, false,
        [this, Resolve, Outcome](const QJsonObject& Arguments)
    {
        GameObject* Target = Resolve(Arguments);
        if (Target == nullptr)
        {
            return EditorToolset::Failure("Object not found");
        }
        return Outcome(ExecuteCommand(MakeSetObjectActiveCommand(GetEditScene(), Target->GetObjectHandle(), Arguments.value("active").toBool())));
    });
    Toolset->Register("actions_list", "Discover all published editor creation actions, including new registered actions", {}, {}, true, [this](const QJsonObject&)
    {
        QJsonArray Actions;
        const auto Context = MakeEditorActionContext();
        for (EditorAction* Action : EditorAction::CollectPublishedActions())
        {
            Actions.append(QJsonObject{{"typeId", QString::fromStdString(Action->GetClass()->GetTypeId().Value)},
                {"name", QString::fromUtf8(Action->GetDisplayName())}, {"enabled", Action->CanExecute(Context)}});
        }
        return EditorToolset::Success(Actions);
    });
    Toolset->Register("action_execute", "Execute a published editor action with the current selection", {{"typeId", Text}}, {"typeId"}, false,
        [this, Outcome](const QJsonObject& Arguments)
    {
        if (BoundEngine.GetPlaySession().IsSimulating())
        {
            return EditorToolset::Failure("Stop play before editing");
        }
        for (EditorAction* Action : EditorAction::CollectPublishedActions())
        {
            if (Action->GetClass()->GetTypeId().Value == Arguments.value("typeId").toString().toStdString())
            {
                auto Context = MakeEditorActionContext();
                return Outcome(Action->CanExecute(Context) && Action->Execute(Context));
            }
        }
        return EditorToolset::Failure("Published action not found");
    });
    Toolset->Register("components_list", "Discover reflected component types and inherited property schemas", {}, {}, true, [](const QJsonObject&)
    {
        QJsonArray Types;
        for (Class* Type : ReflectionSubsystem::Get().GetPublishedCatalog().Classes)
        {
            if (!Type->IsA(TypeId{"engine.Component"}) || !Type->IsConcrete())
            {
                continue;
            }
            QJsonArray Properties;
            for (const PropertyDescriptor* Property : Type->GetAllProperties())
            {
                Properties.append(QJsonObject{{"name", QString::fromStdString(Property->Id.Value)},
                    {"type", QString::fromStdString(Property->ValueTypeId.Value)}, {"readOnly", Property->bReadOnly}});
            }
            Types.append(QJsonObject{{"typeId", QString::fromStdString(Type->GetTypeId().Value)}, {"displayName", QString::fromStdString(Type->GetDisplayName())}, {"properties", Properties}});
        }
        return EditorToolset::Success(Types);
    });
    for (const QString& Operation : {QString("undo"), QString("redo")})
    {
        Toolset->Register("scene_" + Operation, "Apply scene command history: " + Operation, {}, {}, false, [this, Operation, Outcome](const QJsonObject&)
        {
            if (BoundEngine.GetPlaySession().IsSimulating())
            {
                return EditorToolset::Failure("Stop play before editing");
            }
            bool bSucceeded = false;
            if (Operation == "undo")
            {
                bSucceeded = CommandStack.Undo();
            }
            else
            {
                bSucceeded = CommandStack.Redo();
            }
            PopulateHierarchy();
            RefreshSelectionUi();
            UpdateWindowTitleDirty();
            UpdateStatus();
            return Outcome(bSucceeded);
        });
    }
    Toolset->Register("assets_list", "List registered assets in a virtual directory", {{"directory", Text}}, {}, true, [this](const QJsonObject& Arguments)
    {
        QString Directory = Arguments.value("directory").toString("/Game");
        QJsonArray Assets;
        for (const auto& Asset : BoundEngine.GetAssetRegistry().FindByDirectory(Directory.toStdString()))
        {
            Assets.append(QJsonObject{{"id", QString::fromStdString(Asset.Metadata.Guid.ToString())},
                {"path", QString::fromStdString(Asset.VirtualPath)}, {"type", QString::fromStdString(Asset.Metadata.Type.GetIdentifier())}});
        }
        return EditorToolset::Success(Assets);
    });
    Toolset->Register("navigation_build", "Request a navigation build in the edit scene", {}, {}, false, [this](const QJsonObject&)
    {
        if (GetEditScene() == nullptr || BoundEngine.GetPlaySession().IsSimulating())
        {
            return EditorToolset::Failure("Navigation build requires an edit scene");
        }
        GetEditScene()->GetNavigation().RequestBuild();
        Document.MarkDirty();
        UpdateWindowTitleDirty();
        return EditorToolset::Success("Build requested");
    });
    Toolset->Register("play_control", "Play, stop, pause, resume or single-step the editor simulation",
        {{"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"play", "stop", "pause", "resume", "step"}}}}}, {"operation"}, false,
        [this](const QJsonObject& Arguments)
    {
        const QString Operation = Arguments.value("operation").toString();
        auto& Session = BoundEngine.GetPlaySession();
        if (Operation == "play" && !Session.IsSimulating() && !bPlayRequested)
        {
            PlayButton->setChecked(true);
        }
        else if (Operation == "stop")
        {
            PlayButton->setChecked(false);
        }
        else if ((Operation == "pause" && Session.GetState() == PlaySessionState::Playing) ||
                 (Operation == "resume" && Session.GetState() == PlaySessionState::Paused))
        {
            OnPauseClicked();
        }
        else if (Operation == "step" && Session.GetState() == PlaySessionState::Paused)
        {
            OnStepClicked();
        }
        else
        {
            return EditorToolset::Failure("Operation is not available in the current play state");
        }
        if (Operation == "play" && !Session.IsSimulating() && !bPlayRequested)
        {
            return EditorToolset::Failure("Play failed; inspect logs_read for details");
        }
        return EditorToolset::Success(QJsonObject{{"playing", Session.IsSimulating()}, {"pending", bPlayRequested}});
    });
    Toolset->Register("story_status", "Read current story playback", {}, {}, true, [this](const QJsonObject&)
    {
        return EditorToolset::Success(QJsonObject{{"speaker", QString::fromStdString(StoryPlayback.GetDisplayedSpeaker())},
            {"line", QString::fromStdString(StoryPlayback.GetDisplayedLine())}, {"choices", static_cast<int>(StoryPlayback.GetChoiceCount())},
            {"phase", static_cast<int>(StoryPlayback.GetPlaybackPhase())}, {"error", QString::fromStdString(StoryPlayback.GetLastError())}});
    });
    Toolset->Register("story_advance", "Advance dialogue when awaiting input", {}, {}, false, [this, Outcome](const QJsonObject&)
    {
        return Outcome(StoryPlayback.AdvanceDialogue());
    });
    Toolset->Register("story_choose", "Choose a zero-based story option", {{"index", Number}}, {"index"}, false, [this, Outcome](const QJsonObject& Arguments)
    {
        const double Index = Arguments.value("index").toDouble();
        if (Index < 0 || Index >= static_cast<double>(StoryPlayback.GetChoiceCount()) || Index != std::floor(Index))
        {
            return EditorToolset::Failure("Choice index is out of range");
        }
        return Outcome(StoryPlayback.SelectChoice(static_cast<size_t>(Index)));
    });
    Toolset->Register("project_settings_get", "Read project settings", {}, {}, true, [this](const QJsonObject&) { return EditorToolset::Success(ReadProjectSettings()); });
    Toolset->Register("project_settings_set", "Persist project settings; startup changes apply at next open", {{"name", Text}, {"startupScene", Text}, {"startupStory", Text}}, {}, false,
        [this](const QJsonObject& Arguments) { return ApplyProjectSettings(Arguments); });
    Toolset->Register("editor_settings_get", "Read per-user editor preferences", {}, {}, true, [this](const QJsonObject&) { return EditorToolset::Success(ReadEditorSettings()); });
    Toolset->Register("editor_settings_set", "Persist per-user editor preferences", {{"cameraSpeed", Number}, {"consoleLines", Number}, {"mcpAutoStart", Boolean}}, {}, false,
        [this](const QJsonObject& Arguments) { return ApplyEditorSettings(Arguments); });
    Toolset->Register("logs_read", "Read the bounded recent engine log", {}, {}, true, [](const QJsonObject&)
    {
        uint64_t Sequence = 0;
        QJsonArray Messages;
        for (const auto& Message : GetPrintMessages(Sequence))
        {
            Messages.append(QString::fromStdString(Message));
        }
        return EditorToolset::Success(Messages);
    });
    const QJsonObject Preferences = ReadEditorSettings();
    CameraSpeedSpin->setValue(Preferences.value("cameraSpeed").toDouble());
    if (Preferences.value("mcpAutoStart").toBool() && !McpServer->Start())
    {
        PrintString("MCP startup failed: " + McpServer->Error().toStdString());
    }
}

void EditorMainWindow::BuildConsoleDockPage()
{
    ConsolePage = new QWidget(DockTabs);
    ConsolePage->setObjectName("EditorConsole");
    auto* Layout = new QVBoxLayout(ConsolePage);
    ConsoleOutput = new QPlainTextEdit(ConsolePage);
    ConsoleOutput->setObjectName("ConsoleOutput");
    ConsoleOutput->setReadOnly(true);
    ConsoleOutput->setMaximumBlockCount(ReadEditorSettings().value("consoleLines").toInt());
    Layout->addWidget(ConsoleOutput);
    auto* Input = new QLineEdit(ConsolePage);
    Input->setObjectName("ConsoleInput");
    Input->setPlaceholderText(tr("help или имя_инструмента {\"аргумент\":\"значение\"}"));
    QStringList Names{"help", "clear"};
    for (const QJsonValue& Tool : Toolset->List())
    {
        Names.append(Tool.toObject().value("name").toString());
    }
    Input->setCompleter(new QCompleter(Names, Input));
    Layout->addWidget(Input);
    auto History = std::make_shared<QStringList>();
    auto HistoryIndex = std::make_shared<int>(0);
    for (const auto& Direction : {std::make_pair(Qt::Key_Up, -1), std::make_pair(Qt::Key_Down, 1)})
    {
        auto* Shortcut = new QShortcut(QKeySequence(Direction.first), Input);
        Shortcut->setContext(Qt::WidgetShortcut);
        connect(Shortcut, &QShortcut::activated, Input, [Input, History, HistoryIndex, Direction]()
        {
            *HistoryIndex = std::clamp(*HistoryIndex + Direction.second, 0, static_cast<int>(History->size()));
            Input->setText(History->value(*HistoryIndex));
        });
    }
    connect(Input, &QLineEdit::returnPressed, this, [this, Input, History, HistoryIndex]()
    {
        const QString Command = Input->text().trimmed();
        if (Command.isEmpty())
        {
            return;
        }
        History->append(Command);
        if (History->size() > 100)
        {
            History->removeFirst();
        }
        *HistoryIndex = static_cast<int>(History->size());
        Input->clear();
        if (Command == "clear")
        {
            ConsoleOutput->clear();
            return;
        }
        ConsoleOutput->appendPlainText("> " + Command);
        if (Command == "help")
        {
            ConsoleOutput->appendPlainText(QString::fromUtf8(QJsonDocument(Toolset->List()).toJson(QJsonDocument::Indented)));
            return;
        }
        const int Separator = Command.indexOf(' ');
        QString Name = Command;
        QJsonObject Arguments;
        if (Separator >= 0)
        {
            Name = Command.left(Separator);
            QJsonParseError Error;
            const QJsonDocument Parsed = QJsonDocument::fromJson(Command.mid(Separator + 1).toUtf8(), &Error);
            if (Error.error != QJsonParseError::NoError || !Parsed.isObject())
            {
                ConsoleOutput->appendPlainText("Arguments must be a JSON object");
                return;
            }
            Arguments = Parsed.object();
        }
        ConsoleOutput->appendPlainText(QString::fromUtf8(QJsonDocument(Toolset->Call(Name, Arguments)).toJson(QJsonDocument::Indented)));
    });
    auto* Poll = new QTimer(ConsolePage);
    connect(Poll, &QTimer::timeout, ConsolePage, [this]()
    {
        for (const auto& Message : GetPrintMessages(ConsoleSequence))
        {
            ConsoleOutput->appendPlainText(QString::fromStdString(Message));
        }
    });
    Poll->start(250);
    DockTabs->addTab(ConsolePage, tr("Консоль"));
}

void EditorMainWindow::OnExportGame()
{
    const QString Destination = QFileDialog::getSaveFileName(this,
        tr("Новая папка игры — экспорт сохранённых файлов"), "Game-Windows", {}, nullptr,
        QFileDialog::DontConfirmOverwrite);
    if (Destination.isEmpty())
    {
        return;
    }
    const auto Result = Toolset->Call("project_export_game", {{"destination", Destination}});
    if (Result.value("isError").toBool())
    {
        QMessageBox::warning(this, tr("Экспорт игры"), Result.value("content").toArray().first().toObject().value("text").toString());
        return;
    }
    QMessageBox::information(this, tr("Экспорт игры"),
        tr("Игра экспортирована в %1. Для запуска откройте Game.exe. Передавайте всю папку целиком.").arg(Destination));
}
