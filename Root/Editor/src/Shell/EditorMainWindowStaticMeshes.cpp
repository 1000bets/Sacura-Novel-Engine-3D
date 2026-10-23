#include "Shell/EditorMainWindow.h"
#include "UI/Views/StaticMeshEditorWindow.h"
#include "Automation/EditorToolset.h"
#include "Engine.h"
#include <QJsonDocument>
#include <QStatusBar>

StaticMeshEditorWindow* EditorMainWindow::OpenStaticMeshEditor(const AssetKey& RequestedAsset)
{
    AssetKey Asset = RequestedAsset;
    AssetRegistryEntry Entry;
    if (!BoundEngine.GetAssetRegistry().TryResolveKey(Asset, Entry))
    {
        return nullptr;
    }
    if (!Asset.HasSubAsset())
    {
        for (const auto& Subasset : Entry.Metadata.SubAssets)
        {
            if (Subasset.Type == StaticMeshAssetType)
            {
                Asset.SubAsset = Subasset.Id;
                break;
            }
        }
    }
    StaticMeshDocument Probe(BoundEngine.GetAssetRegistry());
    std::string Error;
    if (!Probe.Open(Asset, Error))
    {
        statusBar()->showMessage(QString::fromStdString(Error), 5000);
        return nullptr;
    }
    for (QMainWindow* AssetWindow : findChildren<QMainWindow*>())
    {
        auto* Window = dynamic_cast<StaticMeshEditorWindow*>(AssetWindow);
        if (Window && Window->GetDocument().GetAsset() == Asset)
        {
            if (Window->isMinimized())
            {
                Window->showNormal();
            }
            Window->show();
            Window->raise();
            Window->activateWindow();
            return Window;
        }
    }
    auto* Window = new StaticMeshEditorWindow(BoundEngine, Asset, this);
    Window->show();
    return Window;
}

void EditorMainWindow::RegisterStaticMeshTools()
{
    const QJsonObject Text{{"type", "string"}};
    Toolset->Register("static_mesh_document", "Open or inspect mesh defaults; edit, undo, redo, reload and explicitly save the shared UI draft",
        {{"path", Text}, {"subAssetId", Text},
            {"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"inspect", "edit", "undo", "redo", "reload", "save"}}}},
            {"defaults", QJsonObject{{"type", "object"}}}}, {"path", "operation"}, false,
        [this](const QJsonObject& Arguments)
        {
            const auto Operation = Arguments.value("operation").toString();
            if (Operation != "inspect" && (BoundEngine.GetPlaySession().IsSimulating() || BoundEngine.IsGameRunning() || bPlayRequested))
            {
                return EditorToolset::Failure("Stop Play before editing mesh defaults");
            }
            AssetRegistryEntry Entry;
            if (!BoundEngine.GetAssetRegistry().TryGetByPath(Arguments.value("path").toString().toStdString(), Entry))
            {
                return EditorToolset::Failure("Mesh model not found");
            }
            AssetKey Asset{Entry.Metadata.Guid, {}};
            const auto Identifier = Arguments.value("subAssetId").toString();
            if (!Identifier.isEmpty())
            {
                SubAssetId Subasset;
                if (!Guid::TryParse(Identifier.toStdString(), Subasset))
                {
                    return EditorToolset::Failure("Invalid mesh subasset ID");
                }
                Asset.SubAsset = Subasset;
            }
            auto* Window = OpenStaticMeshEditor(Asset);
            if (!Window)
            {
                return EditorToolset::Failure("StaticMesh editor could not open");
            }
            std::string Error;
            if (Operation != "inspect")
            {
                nlohmann::json Value = nlohmann::json::object();
                if (Operation == "edit")
                {
                    if (!Arguments.value("defaults").isObject())
                    {
                        return EditorToolset::Failure("Edit requires a defaults object");
                    }
                    Value = nlohmann::json::parse(QJsonDocument(Arguments.value("defaults").toObject()).toJson().toStdString());
                }
                if (!Window->Run(Operation, Value, Error))
                {
                    return EditorToolset::Failure(QString::fromStdString(Error));
                }
            }
            auto& Document = Window->GetDocument();
            const auto Defaults = QJsonDocument::fromJson(QByteArray::fromStdString(Document.GetDefaults().ToJson().dump())).object();
            return EditorToolset::Success(QJsonObject{{"defaults", Defaults}, {"dirty", Document.IsDirty()},
                {"subAssetId", QString::fromStdString(Document.GetAsset().SubAsset->ToString())}});
        });
}
