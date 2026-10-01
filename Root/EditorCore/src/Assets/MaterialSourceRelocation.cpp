#include "Assets/MaterialSourceRelocation.h"
#include "Core/IO/AtomicFileWriter.h"
#include "Assets/AssetPath.h"
#include <fstream>
#include <regex>
#include <sstream>

AssetDiagnostic MaterialSourceRelocation::Prepare(const AssetRegistry& Registry, const std::filesystem::path& Source,
    const std::filesystem::path& Destination, std::vector<MaterialSourceEdit>& OutEdits)
{
    OutEdits.clear();
    const auto SourceRoot = Source.lexically_normal();
    auto Relocated = [&](const std::filesystem::path& Path)
    {
        const auto Absolute = Path.lexically_normal();
        if (Absolute == SourceRoot)
        {
            return Destination;
        }
        if (AssetPath::IsInsideContent(Absolute, SourceRoot))
        {
            return Destination / Absolute.lexically_relative(SourceRoot);
        }
        return Absolute;
    };
    const std::regex IncludeExpression(R"(^\s*#\s*include\s*["<]([^">]+)[">])");
    for (const auto& Entry : Registry.FindByType(ShaderSourceAssetType))
    {
        if (Entry.Mount != AssetMount::Game)
        {
            continue;
        }
        std::ifstream Input(Entry.AbsolutePath);
        if (!Input)
        {
            return AssetDiagnostic::Fail(AssetErrorCode::NotFound, "ShaderMove", "Cannot read shader source", {}, Entry.AbsolutePath);
        }
        MaterialSourceEdit Edit;
        Edit.Original.assign(std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>());
        const auto OldPath = std::filesystem::path(Entry.AbsolutePath);
        const auto NewPath = Relocated(OldPath);
        Edit.Path = NewPath.string();
        std::istringstream Lines(Edit.Original);
        std::string Line;
        bool bChanged = false;
        while (std::getline(Lines, Line))
        {
            std::smatch Match;
            if (std::regex_search(Line, Match, IncludeExpression))
            {
                const std::string Included = Match[1].str();
                if (Included.rfind("Sakura/", 0) != 0 && Included.rfind("/Engine/", 0) != 0)
                {
                    auto Target = OldPath.parent_path() / Included;
                    if (Included.rfind("/", 0) == 0)
                    {
                        AssetRegistryEntry IncludedAsset;
                        if (Registry.TryGetByPath(Included, IncludedAsset))
                        {
                            Target = IncludedAsset.AbsolutePath;
                        }
                    }
                    const auto NewTarget = Relocated(Target);
                    if (NewTarget != Target.lexically_normal() || NewPath != OldPath)
                    {
                        const auto Relative = NewTarget.lexically_relative(NewPath.parent_path()).generic_string();
                        if (Relative != Included)
                        {
                            Line.replace(static_cast<size_t>(Match.position(1)), static_cast<size_t>(Match.length(1)), Relative);
                            bChanged = true;
                        }
                    }
                }
            }
            Edit.Updated += Line + "\n";
        }
        if (bChanged)
        {
            OutEdits.push_back(std::move(Edit));
        }
    }
    return AssetDiagnostic::Ok();
}

AssetDiagnostic MaterialSourceRelocation::Apply(const std::vector<MaterialSourceEdit>& Edits)
{
    size_t AppliedCount = 0;
    for (const auto& Edit : Edits)
    {
        std::string Error;
        if (!AtomicFileWriter::WriteText(Edit.Path, Edit.Updated, Error))
        {
            for (size_t Index = 0; Index < AppliedCount; ++Index)
            {
                AtomicFileWriter::WriteText(Edits[Index].Path, Edits[Index].Original, Error);
            }
            return AssetDiagnostic::Fail(AssetErrorCode::InternalError, "ShaderMove", Error);
        }
        ++AppliedCount;
    }
    return AssetDiagnostic::Ok();
}

void MaterialSourceRelocation::Restore(const std::vector<MaterialSourceEdit>& Edits)
{
    for (const auto& Edit : Edits)
    {
        std::string Error;
        AtomicFileWriter::WriteText(Edit.Path, Edit.Original, Error);
    }
}
