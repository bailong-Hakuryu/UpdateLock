@echo off
chcp 65001 > nul
echo ===================================================
echo   UpdateLock C++ 自动编译与构建脚本
echo ===================================================

:: 1. 尝试寻找 Visual Studio 环境
set "VS_PATH="
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
) else if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VS_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
)

if defined VS_PATH (
    echo [✓] 找到 Visual Studio 环境: %VS_PATH%
    call "%VS_PATH%"
    if not exist build mkdir build
    cl /std:c++17 /EHsc /utf-8 /Fe:build\UpdateLock.exe src\main.cc src\security_manager.cc src\process_manager.cc src\immunizer.cc advapi32.lib shell32.lib user32.lib
    if %errorlevel% equ 0 (
        echo.
        echo [✓] 编译成功！可执行文件路径: build\UpdateLock.exe
    ) else (
        echo [!] 编译过程中发生错误。
    )
    goto END
)

:: 2. 尝试寻找 g++
where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [✓] 找到 g++ 编译器
    if not exist build mkdir build
    g++ -std=c++17 -O2 src/main.cc src/security_manager.cc src/process_manager.cc src/immunizer.cc -ladvapi32 -lshell32 -luser32 -o build/UpdateLock.exe
    if %errorlevel% equ 0 (
        echo.
        echo [✓] 编译成功！可执行文件路径: build/UpdateLock.exe
    ) else (
        echo [!] 编译过程中发生错误。
    )
    goto END
)

echo [!] 未检测到 MSVC (Visual Studio) 或 g++ 编译器。
echo     请安装 Visual Studio (C++ 桌面开发) 或 MinGW/g++ 环境后运行此脚本。

:END
pause
