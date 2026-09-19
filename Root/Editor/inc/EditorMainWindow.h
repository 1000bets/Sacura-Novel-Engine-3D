#pragma once

#include <QMainWindow>

class Engine;
class ProjectSession;
class ReflectionInspector;
class QLabel;

class EditorMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    EditorMainWindow(Engine& InEngine, ProjectSession& InSession, QWidget* Parent = nullptr);

    void RefreshProjectTitle();

private:
    Engine& BoundEngine;
    ProjectSession& BoundSession;
    QLabel* ProjectStatusLabel = nullptr;
    ReflectionInspector* Inspector = nullptr;
};
