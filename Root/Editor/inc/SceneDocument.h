#pragma once

#include "Gameplay/ObjectHandle.h"
#include <filesystem>
#include <string>

class Engine;
class Scene;

class SceneDocument
{
public:
    void BindEngine(Engine* InEngine);
    bool AdoptScene(Scene* ExistingScene, const std::filesystem::path& AbsolutePath, std::string& OutError);

    bool Open(const std::filesystem::path& AbsolutePath, std::string& OutError);
    bool Save(std::string& OutError);
    bool SaveAs(const std::filesystem::path& AbsolutePath, std::string& OutError);
    void Close();

    bool IsOpen() const { return GetScene() != nullptr; }
    bool IsDirty() const { return bDirty && IsOpen(); }
    void MarkDirty();

    Scene* GetScene() const;
    const std::filesystem::path& GetDocumentPath() const { return DocumentPath; }
    const std::string& GetLastError() const { return LastError; }

private:
    bool ReplaceBoundScene(Scene* NewScene, const std::filesystem::path& Path, std::string& OutError);

    Engine* BoundEngine = nullptr;
    ObjectHandle BoundScene;
    std::filesystem::path DocumentPath;
    bool bDirty = false;
    std::string LastError;
};
