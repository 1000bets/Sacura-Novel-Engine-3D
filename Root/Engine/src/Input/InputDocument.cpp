#include "Input/InputDocument.h"
#include "Input/InputSession.h"
#include "Assets/AssetRegistry.h"
#include "Core/IO/AtomicFileWriter.h"
#include <fstream>

bool InputDocument::Read(const std::filesystem::path& Path, nlohmann::json& Document, std::string& Error)
{
    try
    {
        std::ifstream Stream(Path);
        auto Candidate = nlohmann::json::parse(Stream);
        if (!Candidate.is_object() || Candidate.value("Version", 0) != 1)
        {
            Error = "Unsupported input document version: " + Path.string();
            return false;
        }
        Document = std::move(Candidate);
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = "Cannot read input document: " + Path.string() + ": " + Exception.what();
        return false;
    }
}

bool InputDocument::Write(const std::filesystem::path& Path, const nlohmann::json& Document, std::string& Error)
{
    try
    {
        auto Output = Document;
        Output["Version"] = 1;
        return AtomicFileWriter::WriteText(Path, Output.dump(2), Error);
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

InputConfiguration InputDocument::CreateDefaults()
{
    InputConfiguration Configuration;
    InputAction Advance;
    Advance.Identifier = "engine.input.advance";
    Advance.Name = "Продолжить диалог";
    InputAction Pause;
    Pause.Identifier = "engine.input.pause";
    Pause.Name = "Пауза";
    Pause.bExecuteWhenPaused = true;
    Configuration.Actions = {Advance, Pause};
    InputMappingContext Dialogue;
    Dialogue.Identifier = "engine.input.dialogue";
    Dialogue.Name = "Диалог";
    Dialogue.Bindings = {{"advance-space", Advance.Identifier, "Keyboard", "Space"},
        {"advance-enter", Advance.Identifier, "Keyboard", "Enter"},
        {"advance-gamepad", Advance.Identifier, "Gamepad", "South"},
        {"advance-touch", Advance.Identifier, "Touch", "advance"}};
    InputMappingContext System;
    System.Identifier = "engine.input.system";
    System.Name = "Система";
    System.Bindings = {{"pause-key", Pause.Identifier, "Keyboard", "Escape"},
        {"pause-gamepad", Pause.Identifier, "Gamepad", "Start"},
        {"pause-touch", Pause.Identifier, "Touch", "pause"}};
    Configuration.Contexts = {Dialogue, System};
    Configuration.StartupContexts = {{Dialogue.Identifier, 10}, {System.Identifier, 100}};
    return Configuration;
}

bool InputDocument::LoadConfiguration(AssetRegistry& Registry, InputConfiguration& Configuration, std::string& Error,
    const std::filesystem::path& ProjectFile, bool bValidate)
{
    auto Candidate = CreateDefaults();
    try
    {
        for (const auto& Entry : Registry.FindByType(InputActionAssetType))
        {
            nlohmann::json Document;
            if (!Read(Entry.AbsolutePath, Document, Error))
            {
                return false;
            }
            auto Action = Document.at("Action").get<InputAction>();
            Action.Identifier = Entry.Metadata.Guid.ToString();
            Candidate.Actions.push_back(std::move(Action));
        }
        for (const auto& Entry : Registry.FindByType(InputMappingContextAssetType))
        {
            nlohmann::json Document;
            if (!Read(Entry.AbsolutePath, Document, Error))
            {
                return false;
            }
            auto Context = Document.at("Context").get<InputMappingContext>();
            Context.Identifier = Entry.Metadata.Guid.ToString();
            Candidate.Contexts.push_back(std::move(Context));
        }
        const auto Root = Registry.GetGameContentRoot().parent_path();
        if (!ProjectFile.empty())
        {
            nlohmann::json Document;
            if (!ReadProjectInput(ProjectFile, Document, Error))
            {
                return false;
            }
            if (Document.contains("Contexts"))
            {
                Candidate.StartupContexts = Document.at("Contexts").get<std::vector<InputContextActivation>>();
            }
        }
        const auto Profile = Root / "Saved" / "InputProfile.json";
        if (std::filesystem::exists(Profile))
        {
            nlohmann::json Document;
            if (!Read(Profile, Document, Error))
            {
                return false;
            }
            Candidate.Overrides = Document.at("Overrides").get<std::vector<InputBindingOverride>>();
        }
        if (bValidate && !InputSession::Validate(Candidate, Error))
        {
            return false;
        }
        Configuration = std::move(Candidate);
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool InputDocument::ReadProjectInput(const std::filesystem::path& ProjectFile, nlohmann::json& Settings, std::string& Error)
{
    try
    {
        std::ifstream Stream(ProjectFile);
        const auto Document = nlohmann::json::parse(Stream);
        Settings = Document.value("input", nlohmann::json::object());
        if (!Settings.is_object())
        {
            Error = "Project input settings must be an object";
            return false;
        }
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool InputDocument::SaveProjectInput(const std::filesystem::path& ProjectFile, const nlohmann::json& Settings, std::string& Error)
{
    try
    {
        std::ifstream Stream(ProjectFile);
        auto Document = nlohmann::json::parse(Stream);
        Stream.close();
        if (!Document.is_object() || !Settings.is_object())
        {
            Error = "Project descriptor and input settings must be objects";
            return false;
        }
        Document["input"] = Settings;
        return AtomicFileWriter::WriteText(ProjectFile, Document.dump(2), Error);
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool InputDocument::SaveStartupContexts(const std::filesystem::path& ProjectFile,
    const std::vector<InputContextActivation>& Contexts, std::string& Error)
{
    nlohmann::json Settings;
    if (!ReadProjectInput(ProjectFile, Settings, Error))
    {
        return false;
    }
    Settings["Contexts"] = Contexts;
    return SaveProjectInput(ProjectFile, Settings, Error);
}

bool InputDocument::SaveProfile(const std::filesystem::path& ContentRoot,
    const std::vector<InputBindingOverride>& Overrides, std::string& Error)
{
    return Write(ContentRoot.parent_path() / "Saved" / "InputProfile.json", {{"Overrides", Overrides}}, Error);
}
