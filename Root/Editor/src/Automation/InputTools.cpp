#include "Automation/InputTools.h"
#include "Automation/EditorToolset.h"
#include "Documents/InputAssetDocument.h"
#include "Documents/InputPackage.h"
#include "Engine.h"
#include <QJsonDocument>
#include <map>

static QJsonValue InputToolValue(const nlohmann::json& Value)
{
    const auto Wrapped = QJsonDocument::fromJson(QByteArray::fromStdString(nlohmann::json{{"value", Value}}.dump())).object();
    return Wrapped.value("value");
}

void RegisterInputTools(EditorToolset& Toolset, Engine& Runtime)
{
    const QJsonObject Text{{"type", "string"}};
    const QJsonObject Object{{"type", "object"}};
    auto Documents = std::make_shared<std::map<std::string, InputAssetDocument>>();
    const auto CanEdit = [&Runtime]()
    {
        return !Runtime.GetPlaySession().IsSimulating() && !Runtime.IsGameRunning()
            && !Runtime.GetAssetRegistry().GetGameContentRoot().empty();
    };
    Toolset.Register("input_list", "List input actions, contexts, active values and phases", {}, {}, true,
        [&Runtime](const QJsonObject&)
        {
            InputConfiguration Configuration;
            std::string Error;
            if (!InputDocument::LoadConfiguration(Runtime.GetAssetRegistry(), Configuration, Error, Runtime.GetProjectFile()))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            nlohmann::json Data{{"actions", Configuration.Actions}, {"contexts", Configuration.Contexts},
                {"startupContexts", Configuration.StartupContexts}};
            if (const auto* Session = Runtime.GetInputSession())
            {
                Data["activeContexts"] = Session->GetContexts();
                Data["values"] = nlohmann::json::object();
                for (const auto& Action : Configuration.Actions)
                {
                    Data["values"][Action.Identifier] = Session->GetValue(Action.Identifier);
                }
                Data["events"] = nlohmann::json::array();
                for (const auto& Event : Session->GetEvents())
                {
                    Data["events"].push_back({{"action", Event.Action}, {"phase", InputPhaseName(Event.Phase)}, {"value", Event.Value}});
                }
            }
            return EditorToolset::Success(InputToolValue(Data));
        });
    Toolset.Register("input_create", "Create a local input action or context asset",
        {{"name", Text}, {"kind", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"action", "context"}}}}},
        {"name", "kind"}, false, [&Runtime, CanEdit](const QJsonObject& Arguments)
        {
            if (!CanEdit())
            {
                return EditorToolset::Failure("Open a project and stop Play before editing input assets");
            }
            std::string Error;
            AssetRegistryEntry Entry;
            if (!InputAssetDocument::Create(Runtime.GetAssetRegistry(), Arguments.value("name").toString().toStdString(),
                Arguments.value("kind") == "context", Entry, Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            return EditorToolset::Success(QJsonObject{{"path", QString::fromStdString(Entry.VirtualPath)},
                {"identifier", QString::fromStdString(Entry.Metadata.Guid.ToString())}});
        });
    Toolset.Register("input_document", "Inspect, apply a complete draft, undo, redo or explicitly save an input asset",
        {{"path", Text}, {"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"inspect", "apply", "undo", "redo", "save", "reload"}}}},
            {"document", Object}}, {"path", "operation"}, false,
        [&Runtime, Documents, CanEdit](const QJsonObject& Arguments)
        {
            const auto Operation = Arguments.value("operation").toString();
            if (Operation != "inspect" && !CanEdit())
            {
                return EditorToolset::Failure("Stop Play before editing input assets");
            }
            AssetRegistryEntry Entry;
            const auto Path = Arguments.value("path").toString().toStdString();
            if (!Runtime.GetAssetRegistry().TryGetByPath(Path, Entry)
                || (Entry.Metadata.Type != InputActionAssetType && Entry.Metadata.Type != InputMappingContextAssetType))
            {
                return EditorToolset::Failure("Input asset not found");
            }
            const auto Identifier = Entry.Metadata.Guid.ToString();
            auto& Document = (*Documents)[Identifier];
            std::string Error;
            if (Document.GetEntry().AbsolutePath.empty() || Operation == "reload")
            {
                if (!Document.Open(Runtime.GetAssetRegistry(), Entry, Error))
                {
                    return EditorToolset::Failure(QString::fromStdString(Error));
                }
            }
            if (Operation == "apply")
            {
                if (!Arguments.contains("document"))
                {
                    return EditorToolset::Failure("Apply requires a document object");
                }
                const auto Candidate = nlohmann::json::parse(QJsonDocument(Arguments.value("document").toObject()).toJson().toStdString());
                if (!Document.Apply(Candidate, Error))
                {
                    return EditorToolset::Failure(QString::fromStdString(Error));
                }
            }
            else if (Operation == "undo")
            {
                Document.Undo();
            }
            else if (Operation == "redo")
            {
                Document.Redo();
            }
            else if (Operation == "save" && !Document.Save(Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            return EditorToolset::Success(InputToolValue({{"document", Document.GetData()}, {"dirty", Document.IsDirty()}}));
        });
    Toolset.Register("input_package", "Export a dependency-complete input package, inspect it, or import an independent copy",
        {{"operation", QJsonObject{{"type", "string"}, {"enum", QJsonArray{"export", "inspect", "import"}}}},
            {"file", Text}, {"asset", Text}}, {"operation", "file"}, false,
        [&Runtime, CanEdit](const QJsonObject& Arguments)
        {
            const auto Operation = Arguments.value("operation").toString();
            const auto File = std::filesystem::path(Arguments.value("file").toString().toStdWString());
            if (Operation != "inspect" && !CanEdit())
            {
                return EditorToolset::Failure("Open a project and stop Play before package operations");
            }
            std::string Error;
            if (Operation == "export")
            {
                AssetRegistryEntry Entry;
                if (!Runtime.GetAssetRegistry().TryGetByPath(Arguments.value("asset").toString().toStdString(), Entry)
                    || !InputPackage::Export(Runtime.GetAssetRegistry(), Entry, File, Error))
                {
                    return EditorToolset::Failure(QString::fromStdString("Input export failed: " + Error));
                }
                return EditorToolset::Success(true);
            }
            nlohmann::json Package;
            if (!InputDocument::Read(File, Package, Error) || !InputPackage::Validate(Package, Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            if (Operation == "inspect")
            {
                return EditorToolset::Success(InputToolValue(Package));
            }
            std::vector<AssetRegistryEntry> Imported;
            if (!InputPackage::Import(Runtime.GetAssetRegistry(), Package, Imported, Error))
            {
                return EditorToolset::Failure(QString::fromStdString(Error));
            }
            QJsonArray Paths;
            for (const auto& Entry : Imported)
            {
                Paths.append(QString::fromStdString(Entry.VirtualPath));
            }
            return EditorToolset::Success(Paths);
        });
}
