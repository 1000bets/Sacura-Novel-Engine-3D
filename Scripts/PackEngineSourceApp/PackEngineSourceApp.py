#!/usr/bin/env python3
"""Pack engine / editor sources and SampleProject into a RAR archive.

Excludes build trees, IDE caches, and heavy FetchContent / Qt trees.
Requires WinRAR (Rar.exe) on Windows.
"""

from __future__ import annotations

import argparse
import datetime as DateTime
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ExcludedDirectoryNames = {
    ".git",
    ".idea",
    ".vs",
    ".cache",
    "build",
    "Build",
    "cmake-build-debug",
    "cmake-build-release",
    "CMakeFiles",
    "Testing",
    "vcpkg_installed",
    "__pycache__",
    "DiligentCore",
    "DiligentTools",
    "DiligentFX",
    "DiligentSamples",
    "PhysX",
    "Qt",
    "_src",
    "dist",
}

ExcludedFileSuffixes = (
    ".dll",
    ".exe",
    ".lib",
    ".pdb",
    ".obj",
    ".o",
    ".a",
    ".so",
    ".dylib",
    ".ilk",
    ".exp",
    ".idb",
    ".ipch",
    ".pch",
    ".rar",
    ".zip",
    ".7z",
)

ExcludedFileNames = {
    "CMakeCache.txt",
    "cmake_install.cmake",
    "compile_commands.json",
    "install_manifest.txt",
    "aqtinstall.log",
}


def ResolveRepositoryRoot() -> Path:
    if getattr(sys, "frozen", False):
        AnchorPath = Path(sys.executable).resolve()
    else:
        AnchorPath = Path(__file__).resolve()

    Candidate = AnchorPath.parents[2]
    if (Candidate / "CMakeLists.txt").is_file() and (Candidate / "Root").is_dir():
        return Candidate

    for Parent in AnchorPath.parents:
        if (Parent / "CMakeLists.txt").is_file() and (Parent / "Root").is_dir():
            return Parent

    raise FileNotFoundError(
        "Repository root not found. Place PackEngineSourceApp next to Scripts/ "
        "or run from inside the Sacura-Novel-Engine-3D tree."
    )


def ShouldSkipDirectory(DirectoryName: str) -> bool:
    if DirectoryName in ExcludedDirectoryNames:
        return True
    if DirectoryName.startswith("build-"):
        return True
    if DirectoryName.startswith("cmake-build"):
        return True
    return False


def ShouldSkipFile(FilePath: Path) -> bool:
    if FilePath.name in ExcludedFileNames:
        return True
    LowerName = FilePath.name.lower()
    for Suffix in ExcludedFileSuffixes:
        if LowerName.endswith(Suffix):
            return True
    return False


def CopyFilteredTree(SourceRoot: Path, DestinationRoot: Path) -> int:
    CopiedFileCount = 0
    DestinationRoot.mkdir(parents=True, exist_ok=True)

    for CurrentRoot, DirectoryNames, FileNames in os.walk(SourceRoot):
        CurrentPath = Path(CurrentRoot)
        RelativePath = CurrentPath.relative_to(SourceRoot)

        DirectoryNames[:] = [
            DirectoryName
            for DirectoryName in DirectoryNames
            if not ShouldSkipDirectory(DirectoryName)
            and not (
                DirectoryName == "Output"
                and CurrentPath.name == "PackEngineSourceApp"
            )
        ]

        TargetDirectory = DestinationRoot / RelativePath
        TargetDirectory.mkdir(parents=True, exist_ok=True)

        for FileName in FileNames:
            SourceFile = CurrentPath / FileName
            if ShouldSkipFile(SourceFile):
                continue
            shutil.copy2(SourceFile, TargetDirectory / FileName)
            CopiedFileCount += 1

    return CopiedFileCount


def FindRarExecutable() -> Path:
    CandidatePaths = [
        Path(os.environ.get("RAR_EXE", "")),
        Path(r"C:\Program Files\WinRAR\Rar.exe"),
        Path(r"C:\Program Files (x86)\WinRAR\Rar.exe"),
    ]
    PathEnvironment = os.environ.get("PATH", "")
    for Entry in PathEnvironment.split(os.pathsep):
        if Entry:
            CandidatePaths.append(Path(Entry) / "Rar.exe")
            CandidatePaths.append(Path(Entry) / "rar.exe")

    for Candidate in CandidatePaths:
        if Candidate and Candidate.is_file():
            return Candidate

    raise FileNotFoundError(
        "WinRAR Rar.exe not found. Install WinRAR or set RAR_EXE to the full path."
    )


def BuildDefaultArchivePath(RepositoryRoot: Path) -> Path:
    TimeStamp = DateTime.datetime.now().strftime("%Y%m%d_%H%M%S")
    OutputDirectory = RepositoryRoot / "Scripts" / "PackEngineSourceApp" / "Output"
    OutputDirectory.mkdir(parents=True, exist_ok=True)
    return OutputDirectory / f"SakuraNovelEngine3D_Sources_{TimeStamp}.rar"


def PackSources(OutputArchive: Path) -> Path:
    RepositoryRoot = ResolveRepositoryRoot()
    RarExecutable = FindRarExecutable()
    OutputArchive = OutputArchive.resolve()
    OutputArchive.parent.mkdir(parents=True, exist_ok=True)

    if OutputArchive.exists():
        OutputArchive.unlink()

    StagingRootName = "Sakura-Novel-Engine-3D"
    IncludeRoots = [
        RepositoryRoot / "CMakeLists.txt",
        RepositoryRoot / "AGENTS.md",
        RepositoryRoot / "README.md",
        RepositoryRoot / ".gitignore",
        RepositoryRoot / "Root",
        RepositoryRoot / "Samples",
        RepositoryRoot / "Scripts",
    ]

    with tempfile.TemporaryDirectory(prefix="SakuraPackSource_") as TemporaryDirectory:
        StagingRoot = Path(TemporaryDirectory) / StagingRootName
        StagingRoot.mkdir(parents=True, exist_ok=True)
        CopiedFileCount = 0

        for SourcePath in IncludeRoots:
            if not SourcePath.exists():
                continue
            RelativePath = SourcePath.relative_to(RepositoryRoot)
            DestinationPath = StagingRoot / RelativePath
            if SourcePath.is_file():
                DestinationPath.parent.mkdir(parents=True, exist_ok=True)
                if not ShouldSkipFile(SourcePath):
                    shutil.copy2(SourcePath, DestinationPath)
                    CopiedFileCount += 1
            else:
                CopiedFileCount += CopyFilteredTree(SourcePath, DestinationPath)

        Command = [
            str(RarExecutable),
            "a",
            "-r",
            "-m5",
            "-ep1",
            str(OutputArchive),
            StagingRootName,
        ]
        Completed = subprocess.run(
            Command,
            cwd=str(Path(TemporaryDirectory)),
            check=False,
            capture_output=True,
            text=True,
        )
        if Completed.returncode != 0:
            Message = Completed.stderr.strip() or Completed.stdout.strip() or "rar failed"
            raise RuntimeError(f"RAR packing failed ({Completed.returncode}): {Message}")

    print(f"Packed {CopiedFileCount} files into {OutputArchive}")
    print(f"Archive size: {OutputArchive.stat().st_size / (1024 * 1024):.1f} MB")
    return OutputArchive


def ParseArguments(Arguments: list[str]) -> argparse.Namespace:
    Parser = argparse.ArgumentParser(
        description="Pack Sakura engine sources and SampleProject into a RAR archive."
    )
    Parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Destination .rar path (default: Scripts/PackEngineSourceApp/Output/...)",
    )
    return Parser.parse_args(Arguments)


def Main() -> int:
    Parsed = ParseArguments(sys.argv[1:])
    RepositoryRoot = ResolveRepositoryRoot()
    OutputArchive = Parsed.output if Parsed.output is not None else BuildDefaultArchivePath(RepositoryRoot)
    PackSources(OutputArchive)
    return 0


if __name__ == "__main__":
    raise SystemExit(Main())
