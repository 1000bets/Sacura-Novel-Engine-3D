#pragma once

#include "Documents/InputAssetDocument.h"
#include <QDialog>
#include <functional>

class Engine;
class QListWidget;
class QVBoxLayout;
class QLabel;
class QCloseEvent;

class InputWorkspaceView : public QDialog
{
public:
    InputWorkspaceView(Engine& Runtime, QWidget* Parent = nullptr);
    void OpenAsset(const AssetRegistryEntry& Entry);

protected:
    void closeEvent(QCloseEvent* Event) override;

private:
    void RefreshLibrary();
    void RefreshEditor();
    void Edit(const std::function<void(nlohmann::json&)>& Change);
    void CreateAsset(bool bContext);
    bool Save();
    bool ConfirmDiscard();
    void ShowSettings();
    void ShowProfile();
    void ShowStoryBindings();
    void ExportPackage();
    void ImportPackage();
    Engine& Runtime;
    InputAssetDocument Document;
    bool bOpen = false;
    QListWidget* Library = nullptr;
    QWidget* Editor = nullptr;
    QVBoxLayout* EditorLayout = nullptr;
    QLabel* Diagnostic = nullptr;
};
