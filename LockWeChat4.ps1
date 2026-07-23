# 微信 4.x (xwechat) 深度防自动更新锁死程序
$ErrorActionPreference = 'SilentlyContinue'

Write-Host '== 微信 4.x (xwechat) 深度防自动更新锁死程序 ==' -ForegroundColor Cyan

# 1. 结束所有更新进程
Get-Process -Name 'WeChatUpdate', 'WeixinUpdate' | Stop-Process -Force
Write-Host '[+] 已经停止后台更新进程 WeChatUpdate / WeixinUpdate' -ForegroundColor Green

# 2. 扫描并处理 D:\WeChat 下所有 WeixinUpdate.exe 和 WeChatUpdate.exe
$exeFiles = Get-ChildItem -Path 'D:\WeChat' -Recurse -Filter '*Update*.exe'
foreach ($f in $exeFiles) {
    $path = $f.FullName
    if (Test-Path $path) {
        Remove-Item $path -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item -Path $path -ItemType Directory -Force | Out-Null
    
    $acl = Get-Acl -Path $path
    if ($acl) {
        $rule = New-Object System.Security.AccessControl.FileSystemAccessRule('Everyone', 'Write,Delete,ChangePermissions', 'Deny')
        $acl.AddAccessRule($rule)
        Set-Acl -Path $path -AclObject $acl
    }
    Write-Host "[✓] 已锁死更新可执行节点: $path" -ForegroundColor Green
}

# 3. 彻底封死 XPlugin 动态更新插件目录 (微信 4.x 偷偷下载 WeixinUpdate 的核心)
$xpluginDirs = @(
    "$env:APPDATA\Tencent\xwechat\XPlugin\Plugins\WeixinUpdate",
    "$env:APPDATA\Tencent\xwechat\XPlugin\Plugins\UpdateNotify",
    "$env:APPDATA\Tencent\WeChat\XPlugin\Plugins\WeChatUpdate",
    "$env:APPDATA\Tencent\WeChat\XPlugin\Plugins\UpdateNotify"
)

foreach ($dir in $xpluginDirs) {
    if (Test-Path $dir) {
        Remove-Item "$dir\*" -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        New-Item -Path $dir -ItemType Directory -Force | Out-Null
    }
    $acl = Get-Acl -Path $dir
    if ($acl) {
        $rule = New-Object System.Security.AccessControl.FileSystemAccessRule('Everyone', 'Write,CreateFiles,CreateDirectories', 'Deny')
        $acl.AddAccessRule($rule)
        Set-Acl -Path $dir -AclObject $acl
    }
    Write-Host "[✓] 已锁死 XPlugin 热更新插件目录: $dir" -ForegroundColor Green
}

# 4. 彻底封死更新下载缓存目录
$updateDirs = @(
    "$env:APPDATA\Tencent\xwechat\update",
    "$env:APPDATA\Tencent\WeChat\All Users\config\update",
    "$env:LOCALAPPDATA\Tencent\WeChatApp\update"
)

foreach ($dir in $updateDirs) {
    if (Test-Path $dir) {
        Remove-Item "$dir\*" -Recurse -Force -ErrorAction SilentlyContinue
    } else {
        New-Item -Path $dir -ItemType Directory -Force | Out-Null
    }
    $acl = Get-Acl -Path $dir
    if ($acl) {
        $rule = New-Object System.Security.AccessControl.FileSystemAccessRule('Everyone', 'Write,CreateFiles,CreateDirectories', 'Deny')
        $acl.AddAccessRule($rule)
        Set-Acl -Path $dir -AclObject $acl
    }
    Write-Host "[✓] 已锁死更新缓存目录: $dir" -ForegroundColor Green
}

Write-Host "`n[★] 微信 4.x 全套静默更新通道已完美封死！" -ForegroundColor Yellow
