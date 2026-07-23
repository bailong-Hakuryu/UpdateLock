# UpdateLock Universal PowerShell Script (Zero-Compile Edition)
# Open-Source Software Auto-Update Blocker for Windows
$ErrorActionPreference = 'SilentlyContinue'
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding = [System.Text.Encoding]::UTF8

function Test-IsAdmin {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Get-WeChatInstallPath {
    $regPaths = @(
        'HKLM:\SOFTWARE\WOW6432Node\Tencent\WeChat',
        'HKCU:\Software\Tencent\WeChat'
    )
    foreach ($reg in $regPaths) {
        if (Test-Path $reg) {
            $val = (Get-ItemProperty -Path $reg -Name 'InstallPath' -ErrorAction SilentlyContinue).InstallPath
            if ($val -and (Test-Path $val)) { return $val }
        }
    }
    $fallbacks = @('D:\WeChat', 'C:\Program Files\Tencent\WeChat', 'C:\Program Files (x86)\Tencent\WeChat')
    foreach ($fb in $fallbacks) {
        if (Test-Path $fb) { return $fb }
    }
    return 'D:\WeChat'
}

function Set-PathLock ($targetPath, $isExeNode) {
    if ($isExeNode) {
        if (Test-Path $targetPath) {
            if (-not (Get-Item $targetPath).PSIsContainer) {
                Unset-PathLock $targetPath | Out-Null
                Remove-Item $targetPath -Force -ErrorAction SilentlyContinue
            }
        }
        if (-not (Test-Path $targetPath)) {
            New-Item -Path $targetPath -ItemType Directory -Force | Out-Null
        }
    } else {
        if (-not (Test-Path $targetPath)) {
            New-Item -Path $targetPath -ItemType Directory -Force | Out-Null
        }
    }

    $acl = Get-Acl -Path $targetPath
    if ($acl) {
        $rule = New-Object System.Security.AccessControl.FileSystemAccessRule('Everyone', 'Write,Delete,CreateFiles,CreateDirectories', 'Deny')
        $acl.AddAccessRule($rule)
        Set-Acl -Path $targetPath -AclObject $acl
        return $true
    }
    return $false
}

function Unset-PathLock ($targetPath) {
    if (-not (Test-Path $targetPath)) { return $true }
    $acl = Get-Acl -Path $targetPath
    if ($acl) {
        $rules = $acl.GetAccessRules($true, $false, [System.Security.Principal.NTAccount])
        foreach ($r in $rules) {
            if ($r.AccessControlType -eq 'Deny' -and $r.IdentityReference.Value -eq 'Everyone') {
                $acl.RemoveAccessRule($r) | Out-Null
            }
        }
        Set-Acl -Path $targetPath -AclObject $acl
    }
    if ((Get-Item $targetPath).PSIsContainer -and ($targetPath -like '*.exe')) {
        Remove-Item $targetPath -Recurse -Force -ErrorAction SilentlyContinue
    }
    return $true
}

function Lock-WeChat {
    Write-Host "`n[1/3] 正在终止后台静默更新进程..." -ForegroundColor Cyan
    Get-Process -Name 'WeChatUpdate', 'WeixinUpdate' -ErrorAction SilentlyContinue | Stop-Process -Force
    Write-Host "  [+] 已停止后台更新进程。" -ForegroundColor Green

    Write-Host "[2/3] 正在获取微信安装路径与核心节点..." -ForegroundColor Cyan
    $install = Get-WeChatInstallPath
    $appData = $env:APPDATA
    $localAppData = $env:LOCALAPPDATA

    $rules = @(
        @{ Path = "$install\WeChatUpdate.exe"; IsExe = $true; Name = '微信旧版更新文件' },
        @{ Path = "$install\WeixinUpdate.exe"; IsExe = $true; Name = '微信 4.x 更新文件' },
        @{ Path = "$appData\Tencent\xwechat\XPlugin\Plugins\WeixinUpdate"; IsExe = $false; Name = 'xwechat 4.x 热更新插件' },
        @{ Path = "$appData\Tencent\xwechat\XPlugin\Plugins\UpdateNotify"; IsExe = $false; Name = 'xwechat 4.x 更新通知插件' },
        @{ Path = "$appData\Tencent\WeChat\XPlugin\Plugins\WeChatUpdate"; IsExe = $false; Name = 'WeChat 热更新插件' },
        @{ Path = "$appData\Tencent\xwechat\update"; IsExe = $false; Name = 'xwechat 更新下载缓存' },
        @{ Path = "$appData\Tencent\WeChat\All Users\config\update"; IsExe = $false; Name = 'WeChat 配置缓存' },
        @{ Path = "$localAppData\Tencent\WeChatApp\update"; IsExe = $false; Name = 'WeChatApp 本地更新缓存' }
    )

    $success = 0
    foreach ($r in $rules) {
        if (Set-PathLock -targetPath $r.Path -isExeNode $r.IsExe) {
            Write-Host "  [✓] 已锁死: $($r.Name) ($($r.Path))" -ForegroundColor Green
            $success++
        }
    }
    Write-Host "[3/3] 完成: 已成功加锁 $success/$($rules.Count) 个防护通道！" -ForegroundColor Yellow
}

function Unlock-WeChat {
    Write-Host "`n[1/2] 正在解除微信防更新权限锁..." -ForegroundColor Cyan
    $install = Get-WeChatInstallPath
    $appData = $env:APPDATA
    $localAppData = $env:LOCALAPPDATA

    $paths = @(
        "$install\WeChatUpdate.exe",
        "$install\WeixinUpdate.exe",
        "$appData\Tencent\xwechat\XPlugin\Plugins\WeixinUpdate",
        "$appData\Tencent\xwechat\XPlugin\Plugins\UpdateNotify",
        "$appData\Tencent\WeChat\XPlugin\Plugins\WeChatUpdate",
        "$appData\Tencent\xwechat\update",
        "$appData\Tencent\WeChat\All Users\config\update",
        "$localAppData\Tencent\WeChatApp\update"
    )

    foreach ($p in $paths) {
        Unset-PathLock -targetPath $p | Out-Null
        Write-Host "  [✓] 已恢复权限: $p" -ForegroundColor Green
    }
    Write-Host '[2/2] 解锁完成！微信可正常升级。' -ForegroundColor Yellow
}

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host "   UpdateLock 通用防自动更新免疫工具 (PowerShell)" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan

if (-not (Test-IsAdmin)) {
    Write-Host '[!] 警告: 当前未以管理员权限运行！修改 ACL 权限需要管理员权限。' -ForegroundColor Red
} else {
    Write-Host '[✓] 运行环境: 已获得管理员权限 (Administrator)' -ForegroundColor Green
}

while ($true) {
    Write-Host "`n请选择操作:"
    Write-Host "  [1] 一键锁死 微信 4.x / 3.x 自动更新"
    Write-Host "  [2] 一键解锁 / 恢复 微信自动更新"
    Write-Host "  [3] 自定义路径加锁 (通用防更新)"
    Write-Host "  [4] 自定义路径解锁"
    Write-Host "  [0] 退出"
    $opt = Read-Host "输入选项 [0-4]"

    switch ($opt) {
        '1' { Lock-WeChat }
        '2' { Unlock-WeChat }
        '3' {
            $p = Read-Host "请输入绝对路径"
            if ($p) {
                $isExe = $p.EndsWith('.exe', [StringComparison]::OrdinalIgnoreCase)
                if (Set-PathLock -targetPath $p -isExeNode $isExe) { Write-Host '[✓] 成功锁死！' -ForegroundColor Green }
            }
        }
        '4' {
            $p = Read-Host "请输入绝对路径"
            if ($p) { Unset-PathLock -targetPath $p | Out-Null; Write-Host '[✓] 已恢复权限！' -ForegroundColor Green }
        }
        '0' { Write-Host "已退出。"; return }
    }
}
