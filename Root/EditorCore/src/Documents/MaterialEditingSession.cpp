#include "Documents/MaterialEditingSession.h"
#include "Core/Threading/JobSystem.h"
#include "Core/Threading/ThreadContext.h"
#include "Rendering/Assets/AssetGpuUploader.h"
#include "Rendering/Threading/RenderThread.h"

MaterialEditingSession::MaterialEditingSession(AssetRegistry& InRegistry) : Registry(InRegistry) {}

AssetDiagnostic MaterialEditingSession::Open(const AssetKey& Asset)
{
    AssertGameThread();
    AssetRegistryEntry Entry;
    if (!Registry.TryResolveKey(Asset, Entry))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "MaterialEditor", "Material asset is unavailable");
    }
    const bool bNextInstance = Entry.Metadata.Type == MaterialInstanceAssetType;
    std::unique_ptr<MaterialDocument> Next;
    if (bNextInstance)
    {
        Next = std::make_unique<MaterialInstanceDocument>();
    }
    else
    {
        Next = std::make_unique<MaterialDocument>();
    }
    try
    {
        auto Result = Next->Open(Registry, Asset);
        if (Result.HasError())
        {
            return Result;
        }
    }
    catch (const std::exception& Exception)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialEditor", Exception.what());
    }
    Document = std::move(Next);
    bInstance = bNextInstance;
    Preparation.reset();
    Compilation.reset();
    Diagnostic.clear();
    ++Revision;
    return AssetDiagnostic::Ok();
}

AssetDiagnostic MaterialEditingSession::ResolveCandidate(const nlohmann::json& Candidate, ResolvedMaterial& Material) const
{
    return MaterialDocumentIO::Resolve(Registry, Document->GetAsset(), Material,
        {{Document->GetAsset().Asset.ToString(), Candidate}});
}

AssetDiagnostic MaterialEditingSession::Execute(MaterialEditOperation Operation, const nlohmann::json& Value,
    uint64_t ExpectedRevision)
{
    AssertGameThread();
    if (!Document || (ExpectedRevision != 0 && ExpectedRevision != Revision))
    {
        return AssetDiagnostic::Fail(AssetErrorCode::AssetChanged, "MaterialEditor", "Stale material revision; refresh before editing");
    }
    try
    {
        if (Operation == MaterialEditOperation::Reload)
        {
            return Open(Document->GetAsset());
        }
        if (Operation == MaterialEditOperation::Compile)
        {
            JobSystem* Jobs = JobSystem::Get();
            if (Jobs == nullptr || !Jobs->IsInitialized())
            {
                return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "MaterialEditor", "Material preparation requires the job system");
            }
            auto Pending = std::make_shared<PreparedDraft>();
            auto RegistrySnapshot = Registry;
            const auto Key = Document->GetAsset();
            const auto Draft = Document->GetDocument();
            Jobs->Schedule([Pending, RegistrySnapshot, Key, Draft]()
            {
                Pending->Diagnostic = MaterialDocumentIO::Resolve(RegistrySnapshot, Key, Pending->Material,
                    {{Key.Asset.ToString(), Draft}});
                Pending->bReady.store(true, std::memory_order_release);
            });
            Preparation = Pending;
            Compilation.reset();
            RequestedRevision = Revision;
            Diagnostic = "Preparing material";
            return AssetDiagnostic::Ok();
        }
        if (Document->IsReadOnly())
        {
            return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "MaterialEditor", "Engine Content is read-only");
        }
        if (Operation == MaterialEditOperation::Save)
        {
            return Document->Save();
        }
        if (Operation == MaterialEditOperation::Document)
        {
            const auto& Previous = Document->GetDocument();
            auto PreviousSettings = Previous;
            auto NextSettings = Value;
            if (bInstance)
            {
                PreviousSettings.erase("parent");
                PreviousSettings.erase("overrides");
                NextSettings.erase("parent");
                NextSettings.erase("overrides");
                if (NextSettings != PreviousSettings)
                {
                    return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialEditor", "An instance edits only parent and parameter overrides");
                }
            }
            else if (Value.value("domain", "Surface") != Previous.value("domain", "Surface")
                || Value.contains("source")
                || Value.value("schemaVersion", 0) != Previous.value("schemaVersion", 0))
            {
                return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "MaterialEditor", "Create a material in the required domain; source reassignment and schema changes are not supported by this editor");
            }
            if (Value == Previous)
            {
                return AssetDiagnostic::Ok();
            }
            ResolvedMaterial Candidate;
            auto Result = ResolveCandidate(Value, Candidate);
            if (Result.HasError())
            {
                return Result;
            }
            Document->SetDocument(Value);
        }
        else if (Operation == MaterialEditOperation::Source)
        {
            if (Document->IsSourceReadOnly())
            {
                return AssetDiagnostic::Fail(AssetErrorCode::UnsupportedFeature, "MaterialEditor", "Shader source is read-only");
            }
            const auto Text = Value.get<std::string>();
            if (Text == Document->GetSource())
            {
                return AssetDiagnostic::Ok();
            }
            Document->SetSource(Text);
        }
        else if (Operation == MaterialEditOperation::Undo)
        {
            Document->Undo();
        }
        else if (Operation == MaterialEditOperation::Redo)
        {
            Document->Redo();
        }
        ++Revision;
        return AssetDiagnostic::Ok();
    }
    catch (const std::exception& Exception)
    {
        return AssetDiagnostic::Fail(AssetErrorCode::InvalidData, "MaterialEditor", Exception.what());
    }
}

void MaterialEditingSession::Poll(AssetGpuUploader& Uploader)
{
    AssertGameThread();
    if (RequestedRevision != Revision)
    {
        Preparation.reset();
        Compilation.reset();
        return;
    }
    if (Preparation && Preparation->bReady.load(std::memory_order_acquire))
    {
        RenderThread* Thread = RenderThread::Get();
        if (Thread == nullptr || !Thread->IsReady())
        {
            return;
        }
        if (Preparation->Diagnostic.HasError())
        {
            Diagnostic = Preparation->Diagnostic.Message;
            Preparation.reset();
            return;
        }
        CompiledMaterial = std::move(Preparation->Material);
        Preparation.reset();
        Compilation = Uploader.RequestMaterial(CompiledMaterial);
        Diagnostic = "Compiling material";
    }
    if (!Compilation)
    {
        return;
    }
    const auto State = Compilation->State.load();
    if (State == MaterialCompilationState::Ready)
    {
        Preview = std::make_shared<DynamicMaterialInstance>(CompiledMaterial, false);
        PreviewRevision = RequestedRevision;
        Diagnostic = "Compilation succeeded";
        Compilation.reset();
    }
    else if (State == MaterialCompilationState::Failed || State == MaterialCompilationState::Cancelled)
    {
        Diagnostic = Compilation->GetDiagnostic();
        if (Diagnostic.empty())
        {
            Diagnostic = "Material compilation was cancelled";
        }
        Compilation.reset();
    }
}
