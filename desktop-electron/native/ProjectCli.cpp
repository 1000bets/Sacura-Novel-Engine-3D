#include "ProjectCli.h"
#include "Engine.h"
#include "Core/EnginePaths.h"
#include "Core/Threading/ThreadContext.h"
#include "Project/ProjectGenerator.h"
#include "Project/ProjectSession.h"
#include <nlohmann/json.hpp>
#include <iostream>

int RunProjectCli(const QStringList& A)
{
    if (A.size()<2 || (A[1]!="--project-create" && A[1]!="--project-check")) return -1;
    nlohmann::json Result;
    try {
        std::string Error;
        ProjectDescriptor Descriptor;
        if (A[1]=="--project-create") {
            if(A.size()!=4 && A.size()!=5) throw std::runtime_error("Expected parent directory, project name and optional template ID");
            ProjectGeneratorRequest Request;
            Request.ParentDirectory=std::filesystem::path(A[2].toStdWString());
            Request.ProjectName=A[3].toUtf8().toStdString();
            if(A.size()==5) Request.TemplateId=A[4].toStdString();
            if(!ProjectGenerator::CreateProject(Request,Descriptor,Error)) throw std::runtime_error(Error);
        } else {
            if(A.size()!=3) throw std::runtime_error("Expected .project path");
            SetCurrentThreadRole(ThreadRole::Game);
            EnginePaths::InitializeFromExecutable(std::filesystem::path(A[0].toStdWString()));
            Engine Runtime;
            Runtime.InitializeHeadless({});
            ProjectSession Session; Session.BindEngine(&Runtime);
            const bool Ok=Runtime.IsInitialized() && Session.OpenProject(std::filesystem::path(A[2].toStdWString()))
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
