param(
    [string]$Preset = 'windows-msvc-debug',
    [int]$Jobs = 4
)

$ErrorActionPreference = 'Stop'
$WorkspaceRoot = Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $WorkspaceRoot
try {
    if (-not (Test-Path -LiteralPath 'Root/Tests/TestTargets.cmake')) {
        throw 'Local C++ tests are missing. Restore Root/Tests/TestTargets.cmake as described in AGENTS.md.'
    }
    & cmake --preset $Preset
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    $ManifestLines = & ctest --preset $Preset --show-only=json-v1
    if ($LASTEXITCODE -ne 0) { throw 'Cannot read CTest manifest.' }
    $Manifest = ($ManifestLines -join "`n") | ConvertFrom-Json
    if ($null -eq $Manifest.tests -or @($Manifest.tests).Count -eq 0) {
        throw 'CTest contains no tests. Refusing an empty successful run.'
    }
    & cmake --build --preset $Preset --target SakuraNativeVerification --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Native test build failed.' }
    & ctest --preset $Preset --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Native tests failed.' }
}
finally {
    Pop-Location
}
