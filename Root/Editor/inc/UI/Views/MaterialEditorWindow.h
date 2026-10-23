#pragma once

#include "Documents/MaterialEditingSession.h"
#include <QDialog>

class Engine;
class QFormLayout;
class QPlainTextEdit;
class QLabel;
class QTimer;
class MaterialWidget;
class QFileSystemWatcher;

class MaterialEditorWindow : public QDialog
{
public:
    MaterialEditorWindow(Engine& Runtime, std::shared_ptr<MaterialEditingSession> Session, QWidget* Parent);
    MaterialEditingSession& GetSession() { return *Session; }
    bool FlushSource();
    void Refresh();
    void reject() override;

protected:
    void closeEvent(QCloseEvent* Event) override;

private:
    bool Run(MaterialEditOperation Operation, const nlohmann::json& Value = {}, uint64_t ExpectedRevision = 0);
    void RebuildFields();
    void SetParameter(const MaterialParameterDefinition& Definition, const nlohmann::json& Value);
    QWidget* MakeParameterField(const MaterialParameterDefinition& Definition, const MaterialParameterValue& Value);
    bool Save();
    void EditParameterDefinition(const std::string& Identifier = {});
    Engine& Runtime;
    std::shared_ptr<MaterialEditingSession> Session;
    QFormLayout* Fields = nullptr;
    QPlainTextEdit* SourceEditor = nullptr;
    QPlainTextEdit* Diagnostics = nullptr;
    QLabel* StateLabel = nullptr;
    MaterialWidget* Preview = nullptr;
    QTimer* SourceTimer = nullptr;
    QFileSystemWatcher* Watcher = nullptr;
    std::shared_ptr<DynamicMaterialInstance> DisplayedMaterial;
    uint64_t DisplayedRevision = 0;
    uint64_t DisplayedPreview = 0;
    bool bSourcePending = false;
};
