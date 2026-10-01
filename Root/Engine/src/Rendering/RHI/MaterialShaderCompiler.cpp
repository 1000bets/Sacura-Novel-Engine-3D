#include "MaterialShaderCompiler.h"
#include "Graphics/ShaderTools/include/GLSLangUtils.hpp"
#include "spirv_glsl.hpp"
#include <string>
#include "Common/interface/DataBlobImpl.hpp"

void CreateEngineShader(Diligent::IRenderDevice* Device, const Diligent::ShaderCreateInfo& Description,
    Diligent::IShader** OutShader, Diligent::IDataBlob** OutErrors)
{
    if (Device->GetDeviceInfo().Type != Diligent::RENDER_DEVICE_TYPE_GL)
    {
        Device->CreateShader(Description, OutShader, OutErrors);
        return;
    }
    bool bCompilerInitialized = false;
    try
    {
        Diligent::GLSLangUtils::InitializeGlslang();
        bCompilerInitialized = true;
        auto Spirv = Diligent::GLSLangUtils::HLSLtoSPIRV(Description,
            Diligent::GLSLangUtils::SpirvVersion::Vk100, "#define GL_SUPPORTED 1\n", OutErrors);
        Diligent::GLSLangUtils::FinalizeGlslang();
        bCompilerInitialized = false;
        if (Spirv.empty())
        {
            return;
        }
        diligent_spirv_cross::CompilerGLSL Compiler(std::move(Spirv));
        diligent_spirv_cross::CompilerGLSL::Options Options;
        Options.version = 430;
        Options.separate_shader_objects = true;
        Options.enable_420pack_extension = false;
        Options.vertex.fixup_clipspace = false;
        Compiler.set_common_options(Options);
        const auto Resources = Compiler.get_shader_resources();
        for (const auto& Buffer : Resources.uniform_buffers)
        {
            Compiler.unset_decoration(Buffer.id, spv::DecorationBinding);
            Compiler.unset_decoration(Buffer.id, spv::DecorationDescriptorSet);
        }
        Compiler.build_combined_image_samplers();
        for (const auto& Sampler : Compiler.get_combined_image_samplers())
        {
            Compiler.set_name(Sampler.combined_id, Compiler.get_name(Sampler.image_id));
        }
        std::string Source = Compiler.compile();
        const size_t VersionEnd = Source.find('\n');
        Source.erase(0, VersionEnd + 1);
        Diligent::ShaderCreateInfo Converted = Description;
        Converted.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_GLSL;
        Converted.FilePath = nullptr;
        Converted.Source = Source.c_str();
        Converted.SourceLength = Source.size();
        Converted.EntryPoint = "main";
        Device->CreateShader(Converted, OutShader, OutErrors);
    }
    catch (const std::exception& Exception)
    {
        if (bCompilerInitialized)
        {
            Diligent::GLSLangUtils::FinalizeGlslang();
        }
        if (OutErrors != nullptr)
        {
            const std::string Diagnostic = Exception.what();
            auto Error = Diligent::DataBlobImpl::Create(Diagnostic.size() + 1, Diagnostic.c_str());
            if (*OutErrors != nullptr)
            {
                (*OutErrors)->Release();
            }
            *OutErrors = Error.Detach();
        }
    }

}
