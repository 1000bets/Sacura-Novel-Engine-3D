#include "ProjectCli.h"
#include "Localization/EditorLocalization.h"
#include "Engine.h"
#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Project/ProjectGenerator.h"
#include "Project/ProjectSession.h"
#include <nlohmann/json.hpp>
#include <iostream>

int RunProjectCli(const std::vector<std::string>& A)
{
    if (A.size() == 2 && A[1] == "--editor-localization")
    {
        std::string Bytes(4 * 1024 * 1024 + 1, '\0');
        std::cin.read(Bytes.data(), static_cast<std::streamsize>(Bytes.size()));
        Bytes.resize(static_cast<size_t>(std::cin.gcount()));
        std::string Error;
        bool Success = Bytes.size() <= 4 * 1024 * 1024;
        try
        {
            if (Bytes.find_first_not_of(" \t\r\n") != std::string::npos && Success)
            {
                const auto Document = nlohmann::json::parse(Bytes, nullptr, false);
                Success = Document.is_object();
                if (Success)
                {
                    Success = EditorLocalization::Get().ApplyPreferences(Document, Error);
                }
            }
            auto Result = EditorLocalization::Get().Preferences();
            Result["ok"] = Success;
            Result["error"] = Error;
            std::cout << "\n@project " << Result.dump() << std::endl;
        }
        catch (const std::exception& Failure)
        {
            Success = false;
            std::cout << "\n@project " << nlohmann::json{{"ok", false}, {"error", Failure.what()}}.dump() << std::endl;
        }
        if (Success)
        {
            return 0;
        }
        return 1;
    }
    if (A.size()<2 || (A[1]!="--project-create" && A[1]!="--project-check")) return -1;
    nlohmann::json Result;
    try {
        std::string Error;
        ProjectDescriptor Descriptor;
        if (A[1]=="--project-create") {
            if(A.size()!=4 && A.size()!=5) throw std::runtime_error("Expected parent directory, project name and optional template ID");
            ProjectGeneratorRequest Request;
            Request.ParentDirectory=std::filesystem::u8path(A[2]);
            Request.ProjectName=A[3];
            if(A.size()==5) Request.TemplateId=A[4];
            if(!ProjectGenerator::CreateProject(Request,Descriptor,Error)) throw std::runtime_error(Error);
        } else {
            if(A.size()!=3) throw std::runtime_error("Expected .project path");
            SetCurrentThreadRole(ThreadRole::Game);
            EnginePaths::InitializeFromExecutable(std::filesystem::u8path(A[0]));
            Engine Runtime;
            Runtime.InitializeHeadless({});
            ProjectSession Session; Session.BindEngine(&Runtime);
            const bool Ok=Runtime.IsInitialized() && Session.OpenProject(std::filesystem::u8path(A[2]))
                && Session.GetHealth()==ProjectSessionHealth::Ready;
            Error=Runtime.GetInitializationError()+Session.GetLastError();
            for(const auto& D:Session.GetDiagnostics()) Error+="\n"+D.Message;
            if(Ok) Descriptor=*Session.GetProject();
            Session.CloseProject(); Runtime.Shutdown();
            if(!Ok) throw std::runtime_error(Error.empty()?"Project is degraded or incompatible":Error);
        }
        Result={{"ok",true},{"path",Descriptor.ProjectFile.u8string()},{"name",Descriptor.Name}};
    } catch(const std::exception& E) { Result={{"ok",false},{"error",E.what()}}; }
    std::cout << "\n@project " << Result.dump() << std::endl;
    return Result["ok"].get<bool>()?0:1;
}
