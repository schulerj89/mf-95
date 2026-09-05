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
$GameExePath = Join-Path $BuildDir "mf95.exe"

$IncludePath = Join-Path $Root "include"
$TestIncludePath = Join-Path $Root "tests"
$SrcMain = Join-Path $Root "src\main.c"
$SrcPpu = Join-Path $Root "src\mf_ppu.c"
$SrcAudio = Join-Path $Root "src\mf_audio.c"
$SrcAssets = Join-Path $Root "src\mf_assets.c"
$SrcSystem = Join-Path $Root "src\mf_system.c"
$SrcGame = Join-Path $Root "src\mf_game.c"

$TestSources = Get-ChildItem (Join-Path $Root "tests\*.c") | ForEach-Object { "`"$($_.FullName)`"" }
$TestSourcesStr = $TestSources -join " "

$CompileScript = Join-Path $BuildDir "compile.bat"
$CompileBatchContent = @"
@echo off
call "$VcVars" > nul
echo Compiling mf95_tests.exe (Modular Subsystem Tests: PPU + Audio + Assets + Boot + Scenes)...
cl.exe /nologo /W4 /O2 /MD /utf-8 /I "$IncludePath" /I "$TestIncludePath" /Fe"$TestExePath" /Fo"$ObjDir\\" "$SrcPpu" "$SrcAudio" "$SrcAssets" "$SrcSystem" "$SrcGame" $TestSourcesStr winmm.lib
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo Compiling mf95.exe (Interactive Application with CLI Console)...
cl.exe /nologo /W4 /O2 /MD /utf-8 /I "$IncludePath" /Fe"$GameExePath" /Fo"$ObjDir\\" "$SrcMain" "$SrcPpu" "$SrcAudio" "$SrcAssets" "$SrcSystem" "$SrcGame" user32.lib gdi32.lib winmm.lib /link /SUBSYSTEM:CONSOLE
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
