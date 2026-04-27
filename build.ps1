param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$solution = Join-Path $projectRoot "Cwitcher.sln"
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $solution)) {
    throw "Solution was not found at $solution"
}

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found."
}

$vsPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsPath) {
    throw "Visual Studio with MSBuild was not found."
}

$msbuild = Join-Path $vsPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
if (-not (Test-Path -LiteralPath $msbuild)) {
    $msbuild = Join-Path $vsPath "MSBuild\Current\Bin\MSBuild.exe"
}

if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild.exe was not found under $vsPath"
}

$outExe = Join-Path $projectRoot "bin\$Configuration\Cwitcher.exe"

Write-Host "Using MSBuild: $msbuild"
& $msbuild $solution /p:Configuration=$Configuration /p:Platform=$Platform /m

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

Write-Host "Built: $outExe"
