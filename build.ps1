# Build script for UpdateLock C++ project
$ErrorActionPreference = 'SilentlyContinue'
$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "   UpdateLock C++ 项目自动构建脚本 (PowerShell)   " -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan

$buildDir = Join-Path $PSScriptRoot "build"
if (-not (Test-Path $buildDir)) {
    New-Item -Path $buildDir -ItemType Directory | Out-Null
}

# 1. 寻找 Visual Studio vcvars64.bat
$vsPaths = @(
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat",
    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
)

$foundVs = $null
foreach ($p in $vsPaths) {
    if (Test-Path $p) {
        $foundVs = $p
        break
    }
}

$srcMain = Join-Path $PSScriptRoot "src\main.cc"
$srcSec = Join-Path $PSScriptRoot "src\security_manager.cc"
$srcProc = Join-Path $PSScriptRoot "src\process_manager.cc"
$srcImm = Join-Path $PSScriptRoot "src\immunizer.cc"

if ($foundVs) {
    Write-Host "[✓] 找到 Visual Studio 环境: $foundVs" -ForegroundColor Green
    $cmd = "call `"$foundVs`" && cl /std:c++17 /EHsc /utf-8 /Fe:`"$buildDir\UpdateLock.exe`" `"$srcMain`" `"$srcSec`" `"$srcProc`" `"$srcImm`" advapi32.lib shell32.lib user32.lib"
    cmd /c $cmd
    if (Test-Path "$buildDir\UpdateLock.exe") {
        Write-Host "`n[★] 编译成功！可执行文件已生成: $buildDir\UpdateLock.exe" -ForegroundColor Yellow
        exit 0
    }
}

# 2. 尝试寻找 g++
$gpp = Get-Command g++ -ErrorAction SilentlyContinue
if ($gpp) {
    Write-Host "[✓] 找到 g++ 编译器: $($gpp.Source)" -ForegroundColor Green
    g++ -std=c++17 -O2 "$srcMain" "$srcSec" "$srcProc" "$srcImm" -ladvapi32 -lshell32 -luser32 -o "$buildDir\UpdateLock.exe"
    if (Test-Path "$buildDir\UpdateLock.exe") {
        Write-Host "`n[★] 编译成功！可执行文件已生成: $buildDir\UpdateLock.exe" -ForegroundColor Yellow
        exit 0
    }
}

Write-Host "[!] 未找到现成的 MSVC 或 g++ 编译器工具链。" -ForegroundColor Red
Write-Host "    已为您提供零依赖直接可运行的 PowerShell 免编译通用脚本: UpdateLock.ps1" -ForegroundColor Yellow
Write-Host "    若需要编译 C++ 版，请安装 Visual Studio 或 MinGW 后运行 build.ps1。" -ForegroundColor Yellow
