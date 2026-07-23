@echo off
chcp 65001 > nul
title UpdateLock - 开源软件防自动更新免疫工具

:: 自动检查管理员权限，若未获得则自动弹出 Windows UAC 窗口请求管理员权限
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo =============================================================
    echo   正在请求管理员权限，请在弹出的 UAC 提示窗口中点击【是】...
    echo =============================================================
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

:: 已获得管理员权限，自动绕过 PowerShell 执行策略直接启动
cd /d "%~dp0"
powershell -ExecutionPolicy Bypass -NoProfile -File "%~dp0UpdateLock.ps1"
