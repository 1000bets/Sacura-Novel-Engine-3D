param(
    [Parameter(Mandatory = $true)][string]$SourceDirectory,
    [Parameter(Mandatory = $true)][string]$BuildDirectory,
    [Parameter(Mandatory = $true)][string]$InstallDirectory,
    [Parameter(Mandatory = $true)][string]$Configuration,
    [Parameter(Mandatory = $true)][string]$CMakeExecutable,
    [string]$VisualStudioDirectory = '',
    [int]$ParallelJobs = 4,
    [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'
try
{
    if (-not $VisualStudioDirectory)
    {
        $VisualStudioLocator = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $VisualStudioDirectory = & $VisualStudioLocator -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $VisualStudioDirectory)
        {
            throw 'Visual Studio with the C++ workload is required.'
        }
    }
    Import-Module "$VisualStudioDirectory/Common7/Tools/Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath $VisualStudioDirectory -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    $NinjaExecutable = "$VisualStudioDirectory/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
    if (-not (Test-Path -LiteralPath $NinjaExecutable))
    {
        $NinjaExecutable = (Get-Command ninja -ErrorAction Stop).Source
    }
    & $CMakeExecutable -S $SourceDirectory -B $BuildDirectory -G Ninja "-DCMAKE_MAKE_PROGRAM=$NinjaExecutable" "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_INSTALL_PREFIX=$InstallDirectory" -DBUILD_SHARED_LIBS=ON -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF -DFEATURE_opengl=ON -DFEATURE_sql=OFF -DFEATURE_network=OFF -DFEATURE_dbus=OFF -DFEATURE_printsupport=OFF -DFEATURE_system_zlib=OFF -DFEATURE_system_pcre2=OFF -DFEATURE_system_jpeg=OFF -DFEATURE_system_png=OFF -DFEATURE_system_freetype=OFF -DFEATURE_system_harfbuzz=OFF
    if ($LASTEXITCODE -ne 0)
    {
        throw 'Qt source configuration failed.'
    }
    if (-not $ConfigureOnly)
    {
        & $CMakeExecutable --build $BuildDirectory --parallel $ParallelJobs
        if ($LASTEXITCODE -ne 0)
        {
            throw 'Qt source build failed.'
        }
        & $CMakeExecutable --install $BuildDirectory
        if ($LASTEXITCODE -ne 0)
        {
            throw 'Qt installation failed.'
        }
    }
}
catch
{
    Write-Error $_ -ErrorAction Continue
    exit 1
}
