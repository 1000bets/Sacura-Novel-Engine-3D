#include "Documents/InputAssetDocument.h"
#include "Input/InputSession.h"
#include "Assets/ContentHash.h"

bool InputAssetDocument::Open(AssetRegistry& InRegistry, const AssetRegistryEntry& InEntry, std::string& Error)
{
    nlohmann::json Candidate;
    if (!InputDocument::Read(InEntry.AbsolutePath, Candidate, Error))
    {
        return false;
    }
    try
    {
        if (InEntry.Metadata.Type == InputActionAssetType)
        {
            Candidate.at("Action").get<InputAction>();
        }
        else
        {
            Candidate.at("Context").get<InputMappingContext>();
        }
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
    Registry = &InRegistry;
    Entry = InEntry;
    Data = std::move(Candidate);
    Saved = Data;
    Past.clear();
    Future.clear();
    return true;
}

bool InputAssetDocument::Apply(nlohmann::json Candidate, std::string& Error)
{
    if (IsReadOnly())
    {
        Error = "Engine input assets are read-only";
        return false;
    }
    try
    {
        InputConfiguration Configuration;
        if (!InputDocument::LoadConfiguration(*Registry, Configuration, Error, {}, false))
        {
            return false;
        }
        const auto Identifier = Entry.Metadata.Guid.ToString();
        if (Entry.Metadata.Type == InputActionAssetType)
        {
            auto Action = Candidate.at("Action").get<InputAction>();
            Action.Identifier = Identifier;
            for (auto& Existing : Configuration.Actions)
            {
                if (Existing.Identifier == Identifier)
                {
                    Existing = Action;
                }
            }
            Candidate["Action"] = Action;
        }
        else
        {
            auto Context = Candidate.at("Context").get<InputMappingContext>();
            Context.Identifier = Identifier;
            for (auto& Existing : Configuration.Contexts)
            {
                if (Existing.Identifier == Identifier)
                {
                    Existing = Context;
                }
            }
            Candidate["Context"] = Context;
        }
        if (!InputSession::Validate(Configuration, Error))
        {
            return false;
        }
        if (Candidate != Data)
        {
            Past.push_back(Data);
            Future.clear();
            Data = std::move(Candidate);
        }
        return true;
    }
    catch (const std::exception& Exception)
    {
        Error = Exception.what();
        return false;
    }
}

bool InputAssetDocument::Save(std::string& Error)
{
    nlohmann::json Current;
    if (IsReadOnly() || !InputDocument::Read(Entry.AbsolutePath, Current, Error))
    {
        return false;
    }
    if (Current != Saved)
    {
        Error = "Input asset changed on disk. Reopen it before saving.";
        return false;
    }
    if (!InputDocument::Write(Entry.AbsolutePath, Data, Error))
    {
        return false;
    }
    Saved = Data;
    Registry->ScanContent();
    return true;
}

void InputAssetDocument::Undo()
{
    if (!Past.empty())
    {
        Future.push_back(Data);
        Data = std::move(Past.back());
        Past.pop_back();
    }
}

void InputAssetDocument::Redo()
{
    if (!Future.empty())
    {
        Past.push_back(Data);
        Data = std::move(Future.back());
        Future.pop_back();
    }
}

bool InputAssetDocument::Create(AssetRegistry& Registry, const std::string& Name, bool bContext,
    AssetRegistryEntry& Entry, std::string& Error)
{
    if (Name.empty() || Name == "." || Name == ".." || Name.find_first_of("/\\:*?\"<>|") != std::string::npos)
    {
        Error = "Enter a valid input asset name";
        return false;
    }
    std::string Extension = ".inputaction";
    nlohmann::json Data;
    if (bContext)
    {
        Extension = ".inputcontext";
        InputMappingContext Context;
        Context.Name = Name;
        Data["Context"] = Context;
    }
    else
    {
        InputAction Action;
        Action.Name = Name;
        Data["Action"] = Action;
    }
    const auto Filename = Name + Extension;
    const auto Path = Registry.GetGameContentRoot() / "Input" / std::filesystem::u8path(Filename);
    if (Registry.GetGameContentRoot().empty() || Registry.LeafNameExists(Filename) || std::filesystem::exists(Path))
    {
        Error = "Input asset name already exists or no project is open";
        return false;
    }
    if (!InputDocument::Write(Path, Data, Error))
    {
        return false;
    }
    Registry.ScanContent();
    if (!Registry.TryGetByPath("Input/" + Filename, Entry))
    {
        Error = "Input asset registration failed";
        return false;
    }
    return true;
}
