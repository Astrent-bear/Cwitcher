param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $projectRoot "src\\main.c"
$outDir = Join-Path $projectRoot "bin\\$Configuration"
$objDir = Join-Path $projectRoot "obj\\$Configuration"
$outExe = Join-Path $outDir "KeyboardSwitcherC.exe"
$vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "vswhere.exe was not found."
}

$vsPath = & $vswhere -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "Visual Studio with C++ build tools was not found."
}

$vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "vcvars64.bat was not found at $vcvars"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$optFlags = if ($Configuration -eq "Release") { "/O2 /DNDEBUG" } else { "/Zi /Od /DDEBUG" }
$pdb = Join-Path $outDir "KeyboardSwitcherC.pdb"
$obj = Join-Path $objDir "main.obj"

$compile = @(
    "cl",
    "/nologo",
    "/TC",
    "/std:c11",
    "/utf-8",
    "/DUNICODE",
    "/D_UNICODE",
    "/DWIN32_LEAN_AND_MEAN",
    "/D_CRT_SECURE_NO_WARNINGS",
    "/W4",
    "/wd4100",
    "/wd4996",
    $optFlags,
    "/Fo`"$obj`"",
    "/Fe`"$outExe`"",
    "/Fd`"$pdb`"",
    "`"$src`"",
    "/link",
    "/SUBSYSTEM:WINDOWS",
    "user32.lib",
    "shell32.lib",
    "gdi32.lib",
    "kernel32.lib"
)

$command = "`"$vcvars`" >nul && " + ($compile -join " ")

Write-Host "Using vcvars: $vcvars"
cmd /c $command

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

Write-Host "Built: $outExe"
