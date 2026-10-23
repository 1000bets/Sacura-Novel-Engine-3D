#pragma once

#include "Documents/CharacterDocument.h"
#include "Rendering/RenderView.h"
#include <QDialog>
#include <QElapsedTimer>
#include <functional>

class Engine;
class QListWidget;
class QLabel;
class QVBoxLayout;
class QCloseEvent;
class EditorViewportWidget;
class CharacterComponent;

class CharacterWorkspaceView : public QDialog
{
public:
    CharacterWorkspaceView(Engine& Runtime, CharacterDocuments& Documents, QWidget* Parent = nullptr);
    ~CharacterWorkspaceView() override;
    void OpenAsset(const AssetRegistryEntry& Entry);
    void Refresh();
    std::function<void(const AssetRegistryEntry&)> Place;
protected:
    void closeEvent(QCloseEvent* Event) override;
private:
    void RefreshLibrary();
    void RefreshFields();
    void Edit(const std::function<void(CharacterDefinition&)>& Change);
    void Create(bool bDuplicate);
    void Import(bool bAnimationsOnly);
    bool Save();
    bool ConfirmClose();
    void TickPreview();
    Engine& Runtime;
    CharacterDocuments& Documents;
    std::shared_ptr<CharacterDocument> Document;
    QListWidget* Library = nullptr;
    QVBoxLayout* Fields = nullptr;
    QWidget* FieldContainer = nullptr;
    QLabel* Diagnostic = nullptr;
    EditorViewportWidget* Viewport = nullptr;
    Scene* PreviewScene = nullptr;
    CharacterComponent* PreviewCharacter = nullptr;
    RenderViewId ViewId;
    QElapsedTimer PreviewClock;
    bool bPlaying = true;
    std::string PreviewAnimation;
    std::string LastDocument;
};
