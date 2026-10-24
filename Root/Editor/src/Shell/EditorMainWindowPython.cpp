#include "Shell/EditorMainWindow.h"
#include "UI/Views/PythonScriptEditorWindow.h"
#include "Automation/EditorToolset.h"
#include "Engine.h"
#include <QJsonArray>
#include <QStatusBar>

PythonScriptEditorWindow* EditorMainWindow::OpenPythonScriptEditor(const AssetKey& Asset)
{
    for (QMainWindow* AssetWindow : findChildren<QMainWindow*>())
    {
        auto* Window = dynamic_cast<PythonScriptEditorWindow*>(AssetWindow);
        if (Window && Window->GetDocument().GetEntry().Metadata.Guid == Asset.Asset && Window->isVisible())
        {
            Window->raise();
            Window->activateWindow();
            return Window;
        }
    }
    auto Document = std::make_shared<PythonScriptDocument>(BoundEngine.GetAssetRegistry());
    std::string Error;
    if (!Document->Open(Asset, Error))
    {
        statusBar()->showMessage(QString::fromStdString(Error), 6000);
        return nullptr;
    }
    auto* Window = new PythonScriptEditorWindow(BoundEngine, Document, this);
    Window->show();
    return Window;
}

void EditorMainWindow::RegisterPythonScriptTools()
{
    const QJsonObject Text{{"type", "string"}};
    Toolset->Register("python_document", "Inspect or edit a Python source draft without executing it; save explicitly",
        {{"path", Text}, {"source", Text},
            {"revision", QJsonObject{{"type", "integer"}, {"minimum", 1}}},
            {"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"inspect", "source", "undo", "redo", "reload", "save"}}}}},
        {"path", "operation"}, false, [this](const QJsonObject& Arguments)
        {
            AssetRegistryEntry Entry;
            if (!BoundEngine.GetAssetRegistry().TryGetByPath(Arguments.value("path").toString().toStdString(), Entry)
                || Entry.Metadata.Type != PythonScriptAssetType)
            {
                return EditorToolset::Failure("Python script asset not found");
            }
            auto* Window = OpenPythonScriptEditor(AssetKey{Entry.Metadata.Guid, {}});
            if (!Window)
            {
                return EditorToolset::Failure("Unable to open Python document");
            }
            const QString Operation = Arguments.value("operation").toString();
            if (Operation != "inspect")
            {
                if (!Arguments.contains("revision") || static_cast<uint64_t>(Arguments.value("revision").toInteger()) != Window->GetRevision())
                {
                    return EditorToolset::Failure("Refresh the Python document before editing");
                }
                if (Operation == "source" && !Arguments.contains("source"))
                {
                    return EditorToolset::Failure("Python source is required");
                }
                std::string Error;
                if (!Window->Execute(Operation, Arguments.value("source").toString().toStdString(), Error))
                {
                    return EditorToolset::Failure(QString::fromStdString(Error));
                }
            }
            return EditorToolset::Success(QJsonObject{
                {"source", QString::fromStdString(Window->GetDocument().GetSource())},
                {"dirty", Window->GetDocument().IsDirty()},
                {"revision", static_cast<qint64>(Window->GetRevision())},
                {"readOnly", Entry.Mount != AssetMount::Game}});
        });
}
