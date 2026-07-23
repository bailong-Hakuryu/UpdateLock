@echo off
chcp 65001 > nul
title UpdateLock 免编译版启动器
powershell -ExecutionPolicy Bypass -NoProfile -File "%~dp0UpdateLock.ps1"
