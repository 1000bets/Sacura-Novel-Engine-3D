#include "EditorMainWindow.h"

#include "Engine.h"
#include "Project/ProjectSession.h"
#include "ReflectionInspector.h"

#include <QLabel>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

EditorMainWindow::EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent)
    : QMainWindow(Parent)
    , BoundEngine(InEngine)
    , BoundSession(InSession)
{
    setWindowTitle("Sakura Novel Editor");
    resize(1280, 720);

    auto* Central = new QWidget(this);
    auto* Layout = new QVBoxLayout(Central);
    ProjectStatusLabel = new QLabel(Central);
    Inspector = new ReflectionInspector(Central);
    Layout->addWidget(ProjectStatusLabel);
    Layout->addWidget(Inspector, 1);
    setCentralWidget(Central);

    statusBar()->showMessage("Editor ready");
    RefreshProjectTitle();
}

void EditorMainWindow::RefreshProjectTitle()
{
    const ProjectDescriptor* Descriptor = BoundSession.GetProject();
    if (Descriptor == nullptr)
    {
        ProjectStatusLabel->setText("No project open");
        setWindowTitle("Sakura Novel Editor");
        return;
    }

    ProjectStatusLabel->setText(
        QString("Project: %1\nRoot: %2\nContent: %3")
            .arg(QString::fromStdString(Descriptor->Name))
            .arg(QString::fromStdString(Descriptor->ProjectRoot.generic_string()))
            .arg(QString::fromStdString(BoundEngine.GetContentRoot().generic_string())));
    setWindowTitle(QString("Sakura Novel Editor — %1").arg(QString::fromStdString(Descriptor->Name)));
}
