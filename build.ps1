param(
    [switch]$Test,
    [string]$OutputExe = ''
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$BuildDir = Join-Path $Root "build"
$ObjDir = Join-Path $BuildDir "obj"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (!(Test-Path $VsWhere)) {
    throw "vswhere.exe was not found. Install Visual Studio with C++ tools."
}

$VsPath = & $VsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$VsPath) {
    throw "MSVC C++ tools were not found."
}

$VcVars = Join-Path $VsPath "VC\Auxiliary\Build\vcvars64.bat"
if (!(Test-Path $VcVars)) {
    throw "vcvars64.bat was not found under $VsPath."
}

$TestExePath = if ([string]::IsNullOrWhiteSpace($OutputExe)) {
    Join-Path $BuildDir "mf95_tests.exe"
} else {
    [IO.Path]::GetFullPath($OutputExe)
}

$IncludePath = Join-Path $Root "include"
$SrcPpu = Join-Path $Root "src\mf_ppu.c"
$SrcAudio = Join-Path $Root "src\mf_audio.c"
$SrcTest = Join-Path $Root "tests\test_main.c"

$CompileScript = Join-Path $BuildDir "compile.bat"
$CompileBatchContent = @"
@echo off
call "$VcVars" > nul
echo Compiling mf95_tests.exe (PPU + Audio scaffolding)...
cl.exe /nologo /W4 /O2 /MD /utf-8 /I "$IncludePath" /Fe"$TestExePath" /Fo"$ObjDir\\" "$SrcPpu" "$SrcAudio" "$SrcTest" winmm.lib
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
"@

Set-Content -Path $CompileScript -Value $CompileBatchContent -Encoding ASCII
Write-Host "Building mf95 test executable..." -ForegroundColor Cyan
& cmd.exe /c $CompileScript

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Write-Host "Build complete: $TestExePath" -ForegroundColor Green

if ($Test -or ($PSBoundParameters.ContainsKey('Test') -eq $false)) {
    Write-Host "Executing scaffolding verification tests..." -ForegroundColor Cyan
    & $TestExePath
    if ($LASTEXITCODE -ne 0) {
        throw "Scaffolding tests failed with exit code $LASTEXITCODE"
    }
}
