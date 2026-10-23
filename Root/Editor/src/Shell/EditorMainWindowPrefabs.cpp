#include "Automation/EditorToolset.h"
#include <QJsonDocument>
#include "Shell/EditorMainWindow.h"
#include "Commands/PrefabCommands.h"
#include "Prefab/PrefabEditorWindow.h"
#include "Prefab/PrefabOverridesPanel.h"
#include "Prefab/PrefabLibrary.h"
#include "Prefab/PrefabUtility.h"
#include "Panels/ContentBrowserWidget.h"
#include "Engine.h"
#include "World/GameObject.h"
#include "World/Scene.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>

void EditorMainWindow::OpenPrefabEditor(const AssetId& Prefab)
{
    for (PrefabEditorWindow* Window : findChildren<PrefabEditorWindow*>())
    {
        if (Window->GetPrefab() == Prefab)
        {
            connect(Window, &PrefabEditorWindow::AssetFocusRequested, this,
        [this](const QString& AssetIdentifier, const QString& SubAssetIdentifier)
    {
        Toolset->Call("asset_focus", {{"assetId", AssetIdentifier}, {"subAssetId", SubAssetIdentifier}});
        raise();
        activateWindow();
    });
    Window->show();
            Window->raise();
            Window->activateWindow();
            return;
        }
    }
    auto* Window = new PrefabEditorWindow(BoundEngine, Prefab, this);
    std::string Error;
    if (!Window->LoadPrefab(Error))
    {
        delete Window;
        QMessageBox::warning(this, tr("Префаб"), QString::fromStdString(Error));
        return;
    }
    connect(Window, &PrefabEditorWindow::OpenPrefabRequested, this, [this](const QString& Identifier)
    {
        AssetId Prefab;
        if (Guid::TryParse(Identifier.toStdString(), Prefab))
        {
            OpenPrefabEditor(Prefab);
        }
    });
    Window->show();
}

void EditorMainWindow::CreatePrefabFromSelection()
{
    GameObject* Selected = GetSelectedGameObject();
    if (Selected == nullptr || BoundEngine.GetPlaySession().IsSimulating())
    {
        return;
    }
    const auto DefaultPath = BoundEngine.GetAssetRegistry().GetGameContentRoot() / (Selected->GetName() + ".prefab");
    const QString Path = QFileDialog::getSaveFileName(this, tr("Создать префаб"),
        QString::fromStdString(DefaultPath.string()), tr("Prefab (*.prefab)"));
    if (Path.isEmpty())
    {
        return;
    }
    AssetId Created;
    std::string Error;
    const bool bAsVariant = PrefabUtility::IsOutermostInstanceRoot(Selected);
    ObjectHandle Updated;
    const bool bCreated = ExecuteCommand(MakePrefabSubtreeEditCommand(GetEditScene(), Selected->GetObjectHandle(),
        "Create Prefab", [Path, bAsVariant, &Created](Scene& World, GameObject& Root, std::string& Failure)
        {
            return PrefabUtility::CreatePrefabAsset(World, Root, Path.toStdString(), bAsVariant, Created, Failure);
        }, &Error, &Updated));
    if (!bCreated)
    {
        QMessageBox::warning(this, tr("Создать префаб"), QString::fromStdString(Error));
        return;
    }
    ContentBrowser->Refresh();
    SelectObject(Updated);
    OpenPrefabEditor(Created);
}

void EditorMainWindow::InitializePrefabPanel(QVBoxLayout* Layout)
{
    PrefabPanel = new PrefabOverridesPanel(this);
    Layout->addWidget(PrefabPanel);
    PrefabOverridesPanel::Callbacks Callbacks;
    Callbacks.OpenPrefab = [this](const AssetId& Prefab) { OpenPrefabEditor(Prefab); };
    Callbacks.Select = [this](ObjectHandle Object) { SelectObject(Object); };
    Callbacks.Revert = [this](ObjectHandle Root, const std::vector<PrefabOverride>& Selection)
    {
        ObjectHandle Updated;
        std::string Error;
        const bool bSucceeded = ExecuteCommand(MakePrefabSubtreeEditCommand(GetEditScene(), Root, "Revert Prefab",
            [Selection](Scene& World, GameObject& Object, std::string& Failure)
            {
                if (Selection.empty())
                {
                    return PrefabUtility::RevertAll(World, Object, Failure);
                }
                return PrefabUtility::RevertOverrides(World, Object, Selection, Failure);
            }, &Error, &Updated));
        if (bSucceeded)
        {
            SelectObject(Updated);
        }
        else
        {
            QMessageBox::warning(this, tr("Префаб"), QString::fromStdString(Error));
        }
        return bSucceeded;
    };
    Callbacks.Apply = [this](ObjectHandle Root, const std::vector<PrefabOverride>& Selection)
    {
        Scene* World = GetEditScene();
        GameObject* Object = World->FindByHandle(Root);
        std::string Error;
        if (!Object || !PrefabUtility::ApplyOverrides(*World, *Object, Selection, Error))
        {
            QMessageBox::warning(this, tr("Префаб"), QString::fromStdString(Error));
            return false;
        }
        CommandStack.Clear();
        Document.MarkDirty();
        PopulateHierarchy();
        RefreshSelectionUi();
        return true;
    };
    Callbacks.Unpack = [this](ObjectHandle Root, PrefabUnpackMode Mode)
    {
        ObjectHandle Updated;
        const bool bSucceeded = ExecuteCommand(MakePrefabSubtreeEditCommand(GetEditScene(), Root, "Unpack Prefab",
            [Mode](Scene& World, GameObject& Object, std::string& Error)
            {
                return PrefabUtility::Unpack(World, Object, Mode, Error);
            }, nullptr, &Updated));
        if (bSucceeded)
        {
            SelectObject(Updated);
        }
        return bSucceeded;
    };
    PrefabPanel->SetCallbacks(std::move(Callbacks));
    PrefabListener = PrefabLibrary::Get().AddChangeListener([this](const AssetId& Changed)
    {
        if (Scene* World = GetEditScene())
        {
            std::string Error;
            PrefabUtility::RefreshInstances(*World, Changed, Error);
            PopulateHierarchy();
            RefreshSelectionUi();
        }
    });
}

void EditorMainWindow::RegisterPrefabTools()
{
    const QJsonObject Text{{"type", "string"}};
    const QJsonObject Boolean{{"type", "boolean"}};
    Toolset->Register("prefab_instantiate", "Instantiate a prefab asset in the edit scene; supports undo",
        {{"assetId", Text}}, {"assetId"}, false, [this](const QJsonObject& Arguments)
    {
        AssetId Prefab;
        if (!GetEditScene() || BoundEngine.GetPlaySession().IsSimulating()
            || !Guid::TryParse(Arguments["assetId"].toString().toStdString(), Prefab))
        {
            return EditorToolset::Failure("An edit scene and valid prefab asset ID are required");
        }
        ObjectHandle Created;
        if (!ExecuteCommand(MakeInstantiatePrefabCommand(GetEditScene(), Prefab, {}, nullptr, &Created)))
        {
            return EditorToolset::Failure("Prefab instantiation failed; check the asset and editor log");
        }
        SelectObject(Created);
        return EditorToolset::Success(QString::fromStdString(GetEditScene()->FindByHandle(Created)->GetPersistentId()));
    });
    Toolset->Register("prefab_create", "Create a prefab in Game Content from an object subtree; scene link supports undo",
        {{"objectId", Text}, {"path", Text}, {"variant", Boolean}}, {"objectId", "path"}, false,
        [this](const QJsonObject& Arguments)
    {
        Scene* World = GetEditScene();
        if (!World || BoundEngine.GetPlaySession().IsSimulating())
        {
            return EditorToolset::Failure("Prefab creation requires an editable scene");
        }
        GameObject* Object = World->FindByPersistentId(Arguments["objectId"].toString().toStdString());
        if (!Object)
        {
            return EditorToolset::Failure("Object not found");
        }
        const auto Path = BoundEngine.GetAssetRegistry().GetGameContentRoot() / Arguments["path"].toString().toStdString();
        AssetId Created;
        ObjectHandle Updated;
        std::string Error;
        const bool bVariant = Arguments["variant"].toBool();
        if (!ExecuteCommand(MakePrefabSubtreeEditCommand(World, Object->GetObjectHandle(), "Create Prefab",
            [Path, bVariant, &Created](Scene& Scene, GameObject& Root, std::string& Failure)
            {
                return PrefabUtility::CreatePrefabAsset(Scene, Root, Path, bVariant, Created, Failure);
            }, &Error, &Updated)))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        ContentBrowser->Refresh();
        SelectObject(Updated);
        return EditorToolset::Success(QString::fromStdString(Created.ToString()));
    });
    Toolset->Register("prefab_inspect", "Inspect the prefab link and overrides of an edit-scene object",
        {{"objectId", Text}}, {"objectId"}, true, [this](const QJsonObject& Arguments)
    {
        Scene* World = GetEditScene();
        GameObject* Root = nullptr;
        if (World)
        {
            Root = PrefabUtility::GetOutermostInstanceRoot(World->FindByPersistentId(Arguments["objectId"].toString().toStdString()));
        }
        if (!Root)
        {
            return EditorToolset::Failure("Object is not part of a prefab instance");
        }
        std::vector<PrefabOverride> Overrides;
        std::string Error;
        if (!PrefabUtility::GetOverrides(*World, *Root, Overrides, Error))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        QJsonArray Changes;
        for (const auto& Override : Overrides)
        {
            Changes.append(QJsonObject{{"description", QString::fromStdString(Override.DisplayText)},
                {"objectId", QString::fromStdString(Override.HostObjectId)},
                {"componentId", QString::fromStdString(Override.HostComponentId)},
                {"property", QString::fromStdString(Override.Property)}});
        }
        return EditorToolset::Success(QJsonObject{{"assetId", QString::fromStdString(PrefabUtility::GetPrefabAsset(Root).ToString())},
            {"rootId", QString::fromStdString(Root->GetPersistentId())}, {"overrides", Changes}});
    });
    const QJsonObject Operation{{"type", "string"}, {"enum", QJsonArray{"revert", "apply", "unpack", "unpackAll"}}};
    Toolset->Register("prefab_modify", "Revert/apply all overrides or unpack an instance; apply writes the shared asset and clears undo",
        {{"objectId", Text}, {"operation", Operation}}, {"objectId", "operation"}, false,
        [this](const QJsonObject& Arguments)
    {
        Scene* World = GetEditScene();
        if (!World || BoundEngine.GetPlaySession().IsSimulating())
        {
            return EditorToolset::Failure("Prefab modification requires an editable scene");
        }
        GameObject* Root = PrefabUtility::GetOutermostInstanceRoot(World->FindByPersistentId(Arguments["objectId"].toString().toStdString()));
        if (!Root)
        {
            return EditorToolset::Failure("Object is not part of a prefab instance");
        }
        const QString Operation = Arguments["operation"].toString();
        std::string Error;
        if (Operation == "apply")
        {
            if (!PrefabUtility::ApplyOverrides(*World, *Root, {}, Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            CommandStack.Clear();
            Document.MarkDirty();
            PopulateHierarchy();
            RefreshSelectionUi();
            return EditorToolset::Success(true);
        }
        ObjectHandle Updated;
        if (!ExecuteCommand(MakePrefabSubtreeEditCommand(World, Root->GetObjectHandle(), "Modify Prefab",
            [Operation](Scene& Scene, GameObject& Object, std::string& Failure)
            {
                if (Operation == "revert")
                {
                    return PrefabUtility::RevertAll(Scene, Object, Failure);
                }
                PrefabUnpackMode Mode = PrefabUnpackMode::OutermostRoot;
                if (Operation == "unpackAll")
                {
                    Mode = PrefabUnpackMode::Completely;
                }
                return PrefabUtility::Unpack(Scene, Object, Mode, Failure);
            }, &Error, &Updated)))
        {
            return EditorToolset::Failure(QString::fromStdString(Error));
        }
        SelectObject(Updated);
        return EditorToolset::Success(true);
    });
}
