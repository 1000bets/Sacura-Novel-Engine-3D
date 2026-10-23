#include "Shell/EditorMainWindow.h"
#include "UI/Views/MaterialEditorWindow.h"
#include "Automation/EditorToolset.h"
#include "Engine.h"
#include <QJsonDocument>
#include <QMessageBox>
#include <QStatusBar>

MaterialEditorWindow* EditorMainWindow::OpenMaterialEditor(const AssetKey& Asset)
{
    for (QMainWindow* AssetWindow : findChildren<QMainWindow*>())
    {
        auto* Window = dynamic_cast<MaterialEditorWindow*>(AssetWindow);
        if (Window && Window->GetSession().GetDocument().GetAsset() == Asset && Window->isVisible())
        {
            Window->raise();
            Window->activateWindow();
            return Window;
        }
    }
    auto Session = std::make_shared<MaterialEditingSession>(BoundEngine.GetAssetRegistry());
    const auto Result = Session->Open(Asset);
    if (Result.HasError())
    {
        PrintString("Material editor: " + Result.Message);
        statusBar()->showMessage(QString::fromStdString(Result.Message), 6000);
        return nullptr;
    }
    auto* Window = new MaterialEditorWindow(BoundEngine, Session, this);
    Window->show();
    return Window;
}

void EditorMainWindow::RegisterMaterialTools()
{
    const QJsonObject Text{{"type", "string"}};
    Toolset->Register("material_document", "Open or inspect the shared material editing session; edit, undo, redo, reload, save or compile its draft",
        {{"path", Text}, {"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"inspect", "document", "source", "undo", "redo", "reload", "save", "compile"}}}},
            {"document", QJsonObject{{"type", "object"}}}, {"source", Text},
            {"revision", QJsonObject{{"type", "integer"}, {"minimum", 1}}}}, {"path", "operation"}, false,
        [this](const QJsonObject& Arguments)
        {
            const auto Operation = Arguments.value("operation").toString();
            if (Operation != "inspect" && (BoundEngine.GetPlaySession().IsSimulating() || BoundEngine.IsGameRunning()))
            {
                return EditorToolset::Failure("Stop Play before editing materials");
            }
            AssetRegistryEntry Entry;
            if (!BoundEngine.GetAssetRegistry().TryGetByPath(Arguments.value("path").toString().toStdString(), Entry)
                || (Entry.Metadata.Type != MaterialAssetType && Entry.Metadata.Type != MaterialInstanceAssetType))
            {
                return EditorToolset::Failure("Material asset not found");
            }
            auto* Window = OpenMaterialEditor(AssetKey{Entry.Metadata.Guid, {}});
            if (!Window || !Window->FlushSource())
            {
                return EditorToolset::Failure("Material editor could not open or flush the draft");
            }
            auto& Session = Window->GetSession();
            Session.Poll(BoundEngine.GetAssetGpuUploader());
            if (Operation != "inspect")
            {
                const std::map<QString, MaterialEditOperation> Operations{
                    {"document", MaterialEditOperation::Document}, {"source", MaterialEditOperation::Source},
                    {"undo", MaterialEditOperation::Undo}, {"redo", MaterialEditOperation::Redo},
                    {"reload", MaterialEditOperation::Reload}, {"save", MaterialEditOperation::Save}, {"compile", MaterialEditOperation::Compile}};
                const auto Found = Operations.find(Operation);
                if (Found == Operations.end())
                {
                    return EditorToolset::Failure("Unknown material operation");
                }
                if (!Arguments.value("revision").isDouble() || Arguments.value("revision").toInteger() < 1
                    || Arguments.value("revision").toDouble() != static_cast<double>(Arguments.value("revision").toInteger()))
                {
                    return EditorToolset::Failure("Inspect first and provide the current draft revision");
                }
                nlohmann::json Value;
                if (Operation == "document")
                {
                    if (!Arguments.value("document").isObject())
                    {
                        return EditorToolset::Failure("Document operation requires an object");
                    }
                    Value = nlohmann::json::parse(QJsonDocument(Arguments.value("document").toObject()).toJson().toStdString());
                }
                else if (Operation == "source")
                {
                    if (!Arguments.value("source").isString())
                    {
                        return EditorToolset::Failure("Source operation requires HLSL text");
                    }
                    Value = Arguments.value("source").toString().toStdString();
                }
                const auto Result = Session.Execute(Found->second, Value, static_cast<uint64_t>(Arguments.value("revision").toInteger()));
                if (Result.HasError())
                {
                    return EditorToolset::Failure(QString::fromStdString(Result.Message));
                }
                Window->Refresh();
            }
            const auto& Document = Session.GetDocument();
            nlohmann::json Data{{"document", Document.GetDocument()}, {"source", Document.GetSource()},
                {"dirty", Document.IsDirty()}, {"readOnly", Document.IsReadOnly()}, {"sourceReadOnly", Document.IsSourceReadOnly()},
                {"externalConflict", Document.HasExternalChanges()}, {"revision", Session.GetRevision()},
                {"previewRevision", Session.GetPreviewRevision()}, {"preparing", Session.IsPreparing()},
                {"diagnostic", Session.GetDiagnostic()}};
            return EditorToolset::Success(QJsonDocument::fromJson(QByteArray::fromStdString(Data.dump())).object());
        });
}
