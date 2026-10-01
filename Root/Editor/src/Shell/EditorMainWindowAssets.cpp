#include "Shell/EditorMainWindow.h"

#include "Actions/BuiltinEditorActions.h"
#include "Actions/EditorAction.h"
#include "Shell/EditorClipboard.h"
#include "Commands/EditorCommands.h"
#include "Panels/ContentBrowserWidget.h"
#include "Shell/EditorTheme.h"
#include "Viewport/EditorViewportWidget.h"
#include "Engine.h"
#include "World/Simulation/PlaySession.h"
#include "World/Scene.h"
#include "World/Serialization/SceneSerializer.h"
#include "World/Components/CameraComponent.h"
#include "World/Components/Component.h"
#include "World/GameObject.h"
#include "World/Components/LightComponent.h"
#include "World/Components/MeshRendererComponent.h"
#include "Project/ProjectSession.h"
#include "Launcher/ProjectBrowserDialog.h"
#include "Panels/ReflectionInspector.h"
#include "Rendering/Frame/RenderFrameData.h"
#include "Rendering/Threading/RenderThread.h"
#include "Core/Threading/ThreadContext.h"
#include "Project/ProjectPaths.h"
#include "Rendering/SceneExtractor.h"
#include "Story/StoryRuntime.h"
#include "UI/StoryWidget.h"

#include <QAbstractItemView>
#include <QAbstractButton>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEvent>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QLayout>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStringList>
#include <QStatusBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTextEdit>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>
#include <QWidget>

#include "Reflection/Class.h"
#include "Reflection/ReflectionSubsystem.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

#include "Shell/EditorMainWindowSupport.h"
#include "Panels/HierarchyTreeWidget.h"

bool EditorMainWindow::RegisterAssetEditor(const AssetType& Type, AssetEditorHandler Handler)
{
    if (!Type.IsValid() || !Handler || AssetEditors.find(Type) != AssetEditors.end())
    {
        return false;
    }
    AssetEditors.emplace(Type, std::move(Handler));
    return true;
}

void EditorMainWindow::RegisterBuiltInAssetEditors()
{
    RegisterAssetEditor(SceneAssetType, [this](
        const AssetKey&,
        const AssetRegistryEntry& Entry,
        const QString& VirtualPath)
    {
        if (BoundEngine.GetPlaySession().IsSimulating())
        {
            statusBar()->showMessage(tr("Stop Play before opening another scene"), 2500);
            return;
        }
        if (!PromptSaveIfDirty())
        {
            return;
        }

        std::string Error;
        if (!Document.Open(Entry.AbsolutePath, Error))
        {
            QMessageBox::warning(
                this,
                tr("Open Scene"),
                QString::fromStdString(Error.empty() ? "Failed to open scene" : Error));
            return;
        }

        ClearSelection();
        CommandStack.Clear();
        StoryPlayback.BindScene(Document.GetScene());
        PopulateHierarchy();
        ConfigureRenderView();
        UpdateWindowTitleDirty();
        UpdateStatus();
        statusBar()->showMessage(tr("Opened scene %1").arg(VirtualPath), 2500);
    });

    RegisterAssetEditor(StoryAssetType, [this](
        const AssetKey&,
        const AssetRegistryEntry& Entry,
        const QString& VirtualPath)
    {
        if (BoundEngine.GetPlaySession().IsSimulating())
        {
            statusBar()->showMessage(tr("Stop Play before opening another story"), 2500);
            return;
        }
        StoryPlayback.BindScene(GetEditScene());
        if (!StoryPlayback.LoadFromFile(Entry.AbsolutePath))
        {
            QMessageBox::warning(
                this,
                tr("Open Story"),
                QString::fromStdString(StoryPlayback.GetLastError().empty()
                    ? "Failed to open story"
                    : StoryPlayback.GetLastError()));
            return;
        }
        for (int TabIndex = 0; TabIndex < DockTabs->count(); ++TabIndex)
        {
            if (DockTabs->widget(TabIndex) == StoryDockPage)
            {
                DockTabs->setCurrentIndex(TabIndex);
                break;
            }
        }
        RefreshStoryPlaybackUi();
        statusBar()->showMessage(tr("Opened story %1").arg(VirtualPath), 2500);
    });
}

void EditorMainWindow::OnAssetActivated(
    QString AssetId,
    QString SubAssetIdentifier,
    QString AssetTypeIdentifier,
    QString VirtualPath)
{
    AssetKey Key{};
    if (!Guid::TryParse(AssetId.toStdString(), Key.Asset))
    {
        return;
    }
    if (!SubAssetIdentifier.isEmpty())
    {
        SubAssetId ParsedSubAsset{};
        if (!Guid::TryParse(SubAssetIdentifier.toStdString(), ParsedSubAsset))
        {
            return;
        }
        Key.SubAsset = ParsedSubAsset;
    }

    AssetRegistryEntry Entry{};
    const SubAssetRecord* SubAsset = nullptr;
    if (!BoundEngine.GetAssetRegistry().TryResolveKey(Key, Entry, &SubAsset))
    {
        statusBar()->showMessage(tr("Asset is no longer registered"), 2500);
        return;
    }
    AssetType Type = UnknownAssetType;
    if (SubAsset != nullptr)
    {
        Type = SubAsset->Type;
    }
    else if (!TryParseAssetType(AssetTypeIdentifier.toStdString(), Type))
    {
        return;
    }

    auto FoundEditor = AssetEditors.find(Type);
    if (FoundEditor == AssetEditors.end())
    {
        statusBar()->showMessage(
            tr("No editor is registered for %1 assets").arg(QString::fromUtf8(AssetTypeToString(Type))),
            2500);
        return;
    }
    FoundEditor->second(Key, Entry, VirtualPath);
}

void EditorMainWindow::OnAssetDropped(
    QString AssetId,
    QString SubAssetIdentifier,
    QString AssetTypeIdentifier,
    QString VirtualPath,
    QPoint Position)
{
    if (BoundEngine.GetPlaySession().IsSimulating())
    {
        statusBar()->showMessage(tr("Scene asset drops are disabled during play"), 2000);
        return;
    }
    AssetKey Key{};
    if (!Guid::TryParse(AssetId.toStdString(), Key.Asset))
    {
        return;
    }
    if (!SubAssetIdentifier.isEmpty())
    {
        SubAssetId ParsedSubAsset{};
        if (Guid::TryParse(SubAssetIdentifier.toStdString(), ParsedSubAsset))
        {
            Key.SubAsset = ParsedSubAsset;
        }
    }
    AssetType Type = UnknownAssetType;
    if (!TryParseAssetType(AssetTypeIdentifier.toStdString(), Type))
    {
        return;
    }
    AssetRegistryEntry Entry{};
    if (!BoundEngine.GetAssetRegistry().TryGetById(Key.Asset, Entry))
    {
        return;
    }

    if (Type == MaterialAssetType || Type == MaterialInstanceAssetType)
    {
        GameObject* Selected = GetSelectedGameObject();
        MeshRendererComponent* MeshRenderer = Selected != nullptr
            ? Selected->GetComponent<MeshRendererComponent>()
            : nullptr;
        if (MeshRenderer == nullptr)
        {
            statusBar()->showMessage(tr("Select an object with MeshRenderer to assign a material"), 2500);
            return;
        }
        ExecuteCommand(MakeSetPropertyCommand(
            GetEditScene(),
            MeshRenderer->GetObjectHandle(),
            PropertyId{"material_asset_id"},
            ReflectedValue::MakeString(Key.Asset.ToString())));
        return;
    }

    if (Type == ModelAssetType && !Key.HasSubAsset())
    {
        for (const SubAssetRecord& SubAsset : Entry.Metadata.SubAssets)
        {
            if (SubAsset.Type == StaticMeshAssetType)
            {
                Key.SubAsset = SubAsset.Id;
                Type = StaticMeshAssetType;
                break;
            }
        }
    }
    if (Type != StaticMeshAssetType || !Key.HasSubAsset())
    {
        statusBar()->showMessage(tr("Only Model/StaticMesh assets can create scene objects"), 2500);
        return;
    }

    Scene* EditScene = GetEditScene();
    if (EditScene == nullptr || PrimaryViewport == nullptr)
    {
        return;
    }
    std::string ObjectName = std::filesystem::path(VirtualPath.toStdString()).stem().string();
    const SubAssetRecord* SubAsset = nullptr;
    AssetRegistryEntry ResolvedEntry{};
    if (BoundEngine.GetAssetRegistry().TryResolveKey(Key, ResolvedEntry, &SubAsset)
        && SubAsset != nullptr && !SubAsset->Name.empty())
    {
        ObjectName = SubAsset->Name;
    }

    ObjectHandle CreatedObject;
    ObjectHandle CreatedComponent;
    CommandStack.BeginGroup("Create Mesh Object");
    bool bCreated = ExecuteCommand(MakeCreateObjectCommand(EditScene, ObjectName, ObjectHandle{}, &CreatedObject));
    if (bCreated)
    {
        bCreated = ExecuteCommand(MakeAddComponentCommand(
            EditScene,
            CreatedObject,
            TypeId{MeshRendererComponent::StaticReflectionTypeId()},
            &CreatedComponent));
    }
    if (bCreated)
    {
        bCreated = ExecuteCommand(MakeSetPropertyCommand(
            EditScene,
            CreatedComponent,
            PropertyId{"mesh_asset_id"},
            ReflectedValue::MakeString(Key.Asset.ToString())));
    }
    if (bCreated)
    {
        bCreated = ExecuteCommand(MakeSetPropertyCommand(
            EditScene,
            CreatedComponent,
            PropertyId{"mesh_sub_asset_id"},
            ReflectedValue::MakeString(Key.SubAsset->ToString())));
    }
    if (bCreated)
    {
        const RenderViewCamera& Camera = PrimaryViewport->GetViewCamera();
        Vector3 Forward = Camera.Target - Camera.Position;
        const float Distance = std::max(1.f, Forward.Length());
        Forward.Normalize();
        Vector3 Right = Forward.Cross(Camera.Up);
        Right.Normalize();
        Vector3 Up = Right.Cross(Forward);
        Up.Normalize();
        const float NormalizedX = static_cast<float>(Position.x()) / std::max(1, PrimaryViewport->width()) * 2.f - 1.f;
        const float NormalizedY = 1.f - static_cast<float>(Position.y()) / std::max(1, PrimaryViewport->height()) * 2.f;
        const float HalfHeight = std::tan(Camera.FieldOfViewDegrees * 0.0087266463f) * Distance;
        Transform DroppedTransform;
        DroppedTransform.Position = Camera.Target
            + Right * (NormalizedX * HalfHeight * static_cast<float>(PrimaryViewport->width()) / std::max(1, PrimaryViewport->height()))
            + Up * (NormalizedY * HalfHeight);
        bCreated = ExecuteCommand(MakeSetTransformCommand(EditScene, CreatedObject, DroppedTransform));
    }
    CommandStack.EndGroup();
    if (bCreated)
    {
        SelectObject(CreatedObject);
        statusBar()->showMessage(tr("Created %1 from Content Browser").arg(QString::fromStdString(ObjectName)), 2000);
    }
}

