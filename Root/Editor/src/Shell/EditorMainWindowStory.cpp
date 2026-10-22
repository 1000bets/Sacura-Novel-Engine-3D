#include "Shell/EditorMainWindow.h"
#include "UI/Adapters/StoryGraphController.h"
#include "UI/Adapters/WorkspaceDockController.h"
#include "UI/Views/StoryWorkspaceView.h"
#include "UI/Views/StoryInspectorView.h"
#include "Engine.h"
#include "Project/ProjectPaths.h"
#include "Project/ProjectSession.h"
#include "World/Scene.h"
#include "World/GameObject.h"
#include "World/Simulation/PlaySession.h"
#include "Assets/StoryAssetCreation.h"
#include "Panels/ContentBrowserWidget.h"
#include <QApplication>
#include <QFileDialog>
#include <QMessageBox>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTabWidget>
#include <QTreeWidget>

void EditorMainWindow::InitializeStoryAuthoring()
{
    StoryAuthoring = std::make_unique<EditorUi::StoryGraphController>(*StoryGraph, *StoryInspector);
    StoryAuthoring->SelectionActivated = [this]()
    {
        HierarchyTree->clearSelection();
        HierarchyTree->setCurrentItem(nullptr);
        ClearSelection();
        bStoryEditingContext = true;
        InspectorPages->setCurrentWidget(StoryInspector);
    };
    StoryAuthoring->Changed = [this]()
    {
        UpdateWindowTitleDirty();
        UpdateStatus();
    };
    StoryAuthoring->ErrorReported = [this](const QString& Error)
    { statusBar()->showMessage(tr("История: ") + Error, 12000); };
    StoryAuthoring->OpenRequested = [this]() { OpenStoryFromDialog(); };
    StoryAuthoring->NewDocumentRequested = [this](int Kind, QPointF Position)
    {
        if (CreateStoryFromDialog())
        {
            StoryGraph->AddNodeRequested(Kind, Position);
        }
    };
    connect(qApp, &QApplication::focusChanged, this,
            [this](QWidget*, QWidget* Focus)
            {
                if (Focus == nullptr)
                {
                    return;
                }
                if (Focus == StoryGraph || StoryGraph->isAncestorOf(Focus) || Focus == StoryInspector ||
                    StoryInspector->isAncestorOf(Focus))
                {
                    bStoryEditingContext = true;
                }
                else if (Focus == HierarchyTree || HierarchyTree->isAncestorOf(Focus) || Focus == PrimaryViewport ||
                         PrimaryViewport->isAncestorOf(Focus) || Focus == ObjectInspectorContent ||
                         ObjectInspectorContent->isAncestorOf(Focus))
                {
                    bStoryEditingContext = false;
                }
            });
    RefreshStoryTargets();
    const auto* Project = BoundSession.GetProject();
    if (Project != nullptr && !Project->StartupStory.empty())
    {
        const auto Relative =
            (Project->ProjectRoot / Project->StartupStory).lexically_relative(ProjectPaths::Content());
        AssetRegistryEntry Entry;
        if (BoundEngine.GetAssetRegistry().TryGetByPath(Relative.generic_string(), Entry))
        {
            AssetKey Key;
            Key.Asset = Entry.Metadata.Guid;
            std::string Error;
            if (!StoryAuthoring->Open(BoundEngine.GetAssetRegistry(), Key, Error))
            {
                statusBar()->showMessage(QString::fromStdString(Error), 12000);
            }
        }
    }
    StoryAuthoring->Refresh();
}

bool EditorMainWindow::IsStoryEditingContext() const
{
    return StoryAuthoring != nullptr && StoryAuthoring->GetDocument().IsOpen() && bStoryEditingContext;
}

void EditorMainWindow::RefreshStoryTargets()
{
    if (StoryAuthoring == nullptr)
    {
        return;
    }
    QVector<QPair<QString, QString>> Objects;
    if (auto* Scene = GetEditScene())
    {
        for (auto* Object : Scene->GetAllObjects())
        {
            if (!Object->IsPendingDestroy())
            {
                Objects.push_back(
                    {QString::fromStdString(Object->GetPersistentId()), QString::fromStdString(Object->GetName())});
            }
        }
    }
    QMap<QString, int> Counts;
    QMap<QString, int> Ordinals;
    for (const auto& Object : Objects)
    {
        ++Counts[Object.second];
    }
    for (auto& Object : Objects)
    {
        if (Counts.value(Object.second) > 1)
        {
            const QString Name = Object.second;
            Object.second = Name + QStringLiteral(" (%1)").arg(++Ordinals[Name]);
        }
    }
    StoryAuthoring->SetObjects(std::move(Objects));
}

void EditorMainWindow::SaveActiveDocument()
{
    if (IsStoryEditingContext())
    {
        SaveStory();
    }
    else
    {
        OnSaveScene();
    }
}

bool EditorMainWindow::SaveStory()
{
    if (StoryAuthoring == nullptr)
    {
        return false;
    }
    std::string Error;
    if (!StoryAuthoring->Save(Error))
    {
        QMessageBox::warning(this, tr("Сохранение истории"), QString::fromStdString(Error));
        return false;
    }
    UpdateWindowTitleDirty();
    UpdateStatus();
    statusBar()->showMessage(tr("История сохранена"), 2500);
    return true;
}

bool EditorMainWindow::PromptSaveStoryIfDirty()
{
    if (StoryAuthoring == nullptr)
    {
        return true;
    }
    StoryAuthoring->FlushEdits();
    if (!StoryAuthoring->GetDocument().IsDirty())
    {
        return true;
    }
    const auto Answer = QMessageBox::question(
        this, tr("Несохранённая история"), tr("Сохранить изменения в истории перед продолжением?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (Answer == QMessageBox::Cancel)
    {
        return false;
    }
    return Answer != QMessageBox::Save || SaveStory();
}

bool EditorMainWindow::OpenStoryAsset(const AssetKey& Asset)
{
    if (BoundEngine.GetPlaySession().IsSimulating() || bPlayRequested)
    {
        statusBar()->showMessage(tr("Остановите предпросмотр перед открытием истории"), 4000);
        return false;
    }
    if (StoryAuthoring->GetDocument().GetAsset() != Asset || !StoryAuthoring->GetDocument().IsOpen() ||
        StoryAuthoring->GetDocument().HasExternalChanges())
    {
        if (!PromptSaveStoryIfDirty())
        {
            return false;
        }
        std::string Error;
        if (!StoryAuthoring->Open(BoundEngine.GetAssetRegistry(), Asset, Error))
        {
            QMessageBox::warning(this, tr("Открытие истории"), QString::fromStdString(Error));
            return false;
        }
    }
    Docking->Open(*StoryGraph);
    bStoryEditingContext = true;
    InspectorPages->setCurrentWidget(StoryInspector);
    return true;
}

void EditorMainWindow::OpenStoryFromDialog()
{
    const QString File = QFileDialog::getOpenFileName(this, tr("Открыть историю"),
                                                      QString::fromStdString(ProjectPaths::Content().generic_string()),
                                                      tr("Истории Sacura (*.story)"));
    if (File.isEmpty())
    {
        return;
    }
    AssetRegistryEntry Entry;
    const auto Path = std::filesystem::path(File.toStdWString());
    const auto Relative = Path.lexically_relative(ProjectPaths::Content());
    if (!BoundEngine.GetAssetRegistry().TryGetByPath(Relative.generic_string(), Entry))
    {
        QMessageBox::warning(
            this, tr("Открытие истории"),
            tr("Выберите ресурс истории в Content проекта. Ресурсы Engine открываются из Content Browser."));
        return;
    }
    AssetKey Key;
    Key.Asset = Entry.Metadata.Guid;
    OpenStoryAsset(Key);
}

bool EditorMainWindow::CreateStoryFromDialog()
{
    if (BoundEngine.GetPlaySession().IsSimulating() || bPlayRequested || !PromptSaveStoryIfDirty())
    {
        return false;
    }
    const QString File = QFileDialog::getSaveFileName(
        this, tr("Создать историю"),
        QString::fromStdString((ProjectPaths::Content() / "NewStory.story").generic_string()),
        tr("Истории Sacura (*.story)"));
    if (File.isEmpty())
    {
        return false;
    }
    std::filesystem::path Path(File.toStdWString());
    if (Path.extension().empty())
    {
        Path += ".story";
    }
    std::string Relative;
    AssetDiagnostic Diagnostic;
    if (!AssetPath::TryMakeRelative(Path, ProjectPaths::Content(), Relative, Diagnostic))
    {
        QMessageBox::warning(this, tr("Создание истории"), tr("Выберите папку внутри Content проекта."));
        return false;
    }
    AssetKey Created;
    Diagnostic = StoryAssetCreation::Create(BoundEngine.GetAssetRegistry(),
                                            AssetPath::MakeVirtualPath(AssetMount::Game, Relative), Created);
    if (Diagnostic.HasError())
    {
        QMessageBox::warning(this, tr("Создание истории"), QString::fromStdString(Diagnostic.Message));
        return false;
    }
    // The prior draft was already reviewed before creation; don't prompt a second time.
    std::string Error;
    if (!StoryAuthoring->Open(BoundEngine.GetAssetRegistry(), Created, Error))
    {
        QMessageBox::warning(this, tr("Открытие истории"), QString::fromStdString(Error));
        return false;
    }
    Docking->Open(*StoryGraph);
    InspectorPages->setCurrentWidget(StoryInspector);
    bStoryEditingContext = true;
    ContentBrowser->Refresh();
    return true;
}
