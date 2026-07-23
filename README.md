# UpdateLock - 微信与通用软件防自动更新/底层锁死免疫工具

[![Language](https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20PowerShell-blue.svg)](file:///d:/微信过低版本)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%2FServer-0078D6.svg)](file:///d:/微信过低版本)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)

`UpdateLock` 是一款专注于 Windows 操作系统的开源软件防自动更新与底层权限锁死免疫工具。通过原生 **Win32 API** 与 **NTFS 访问控制列表 (ACL)** 技术，帮助用户彻底解决微信 4.x / 3.x 等软件静默后台强行升级、占用资源或版本强制过期的问题。

---

## ⚠️ 重要免责声明 (Disclaimer)

1. **仅供学习与研究**：本项目（包括但不限于 C++ 源码、PowerShell 脚本及批处理启动器）仅作为 **Win32 API 编程、NTFS 权限管理机制及系统安全运维技术** 的学术交流与个人学习测试使用。
2. **非侵入性与安全性**：本项目不包含任何二进制外挂、反编译注入或非法破坏性代码。所有操作均基于 Windows 系统原生的文件权限配置（NTFS ACL）与标准进程管理 API。
3. **风险自负**：使用者在下载、编译或运行本项目前，应充分理解相关操作的效果。任何因非法使用、不当操作或修改软件所导致的软件故障、数据丢失或法律纠纷，**原作者及项目贡献者概不承担任何直接或连带责任**。
4. **尊重版权**：文中提及的微信（WeChat / xwechat）及相关商标版权均归属于其对应的软件著作权人腾讯公司（Tencent Holdings Ltd.）。

---

## 🔥 核心项目功能

### 1. 微信 4.x (xwechat) & 3.x 深度防护通道
- **更新可执行文件锁死**：自动寻找 `WeChatUpdate.exe` 与 `WeixinUpdate.exe`，清理源文件并将其替换为具有 NTFS ACL 拒绝写入/删除权限的“免疫文件夹节点”。
- **XPlugin 动态插件目录封堵**：精准锁死 `WeixinUpdate` 与 `UpdateNotify` 等热更新插件路径（微信 4.x 偷偷下载后台更新的核心通道）。
- **更新下载缓存区隔离**：全面加锁 `%APPDATA%\Tencent\xwechat\update`、`%APPDATA%\Tencent\WeChat\All Users\config\update` 与 `%LOCALAPPDATA%\Tencent\WeChatApp\update` 下载目录。

### 2. 进程智能查杀引擎
- 内置 `ProcessManager` 模块，在执行加锁或解除锁死前，基于 `CreateToolhelp32Snapshot` 快照引擎自动扫描并强行终止所有后台静默更新进程，防止文件被占用。

### 3. 底层 NTFS ACL 拒绝权限 (Deny Write/Delete)
- 区别于简单的“只读”文件属性（只读属性易被更新程序静默取消），本项目直接调用 Win32 `SetNamedSecurityInfoW` API，为 `Everyone` 用户组注入 `DENY` 拒绝权限（拒绝写入、删除、新建文件/目录），确保不可被程序篡改。

### 4. 智能路径自动识别 (零硬编码)
- 内置注册表与环境变量识别引擎，自动查询 `HKLM` 与 `HKCU` 下的 `SOFTWARE\WOW6432Node\Tencent\WeChat` 路径，精准适配 C 盘、D 盘或自定义安装目录的用户。

### 5. 支持完全可逆的“一键解锁恢复”
- 提供完整的 ACL 撤销机制。当用户未来希望主动升级软件时，只需运行“一键解锁”，程序将自动恢复访问权限并清除免疫节点，使软件重获正常更新能力。

### 6. 通用第三方软件拓展免疫
- 支持任意自定义路径（支持 `.exe` 文件或文件夹）的快速加锁与解锁，方便扩展至其他经常强行静默更新的第三方软件。

---

## 📖 全面使用方法指南

本项目提供 **小白用户免编译模式**、**开发者 C++ 源码编译模式** 以及 **高级 CLI 模式**，可满足不同背景用户的需求：

### 模式一：普通用户免编译双击模式 (推荐，零门槛)

对于没有 C++ 编译环境的普通用户，**无需敲击任何命令**，只需以下三步：

1. **下载并解压**：下载本项目 ZIP 源码包或 Release 包并解压到任意目录。
2. **双击运行启动器**：右键单击目录下的 **`UpdateLock.bat`**，选择 **【以管理员身份运行】**（或直接双击，在弹出的 UAC 管理员提示框中点击【是】）。
3. **菜单交互**：在弹出的中文主菜单中输入数字选项：
   - 输入 `1` 并回车：一键锁死微信 4.x / 3.x 自动更新。
   - 输入 `2` 并回车：一键解锁/恢复微信升级功能。
   - 输入 `3` 并回车：查看当前各核心节点的防护状态。
   - 输入 `4` 并回车：输入任意自定义软件更新文件路径进行锁死。
   - 输入 `0` 并回车：退出程序。

---

### 模式二：开发者 C++17 源码编译模式

如果您希望体验由原生 C++17 编写的高性能版本，请确保电脑已安装 Visual Studio (勾选 C++ 桌面开发) 或 MinGW-w64：

#### 途径 A：使用 PowerShell 构建脚本
```powershell
# 1. 以管理员身份打开 PowerShell，进入项目根目录
cd d:\微信过低版本

# 2. 执行自动编译脚本
.\build.ps1

# 3. 编译成功后，运行生成的 UpdateLock.exe
.\build\UpdateLock.exe
```

#### 途径 B：使用 CMake 构建
```bash
# 1. 创建并进入构建目录
mkdir build && cd build

# 2. 生成 CMake 构建配置
cmake ..

# 3. 编译生成 Release 版本
cmake --build . --config Release
```

---

### 模式三：高级 PowerShell 命令行模式

如果您倾向于使用 PowerShell 原生脚本：

```powershell
# 使用 -ExecutionPolicy Bypass 绕过系统执行策略
powershell -ExecutionPolicy Bypass -File .\UpdateLock.ps1
```

---

## ❓ 常见问题解答 (FAQ)

#### Q1：为什么运行程序时需要【管理员权限 (Administrator)】？
> **答**：修改 NTFS 文件系统的访问控制列表 (ACL) 以及终止系统后台运行的更新进程是 Windows 的高权限操作。只有获得管理员权限，Win32 API 才能成功为目标文件夹注入 `Deny` 拒绝规则。

#### Q2：锁定后，如果我想把微信升级到最新版本该怎么办？
> **答**：本项目具有完全可逆的“解锁”功能。运行 `UpdateLock.bat` 并选择选项 `[2] 一键解锁`，程序会彻底撤销权限锁并清除免疫节点。完成软件官方升级后，可再次运行选项 `[1]` 重新锁死。

#### Q3：为什么使用 C++ 版本的 `UpdateLock.exe` 比传统批处理更好？
> **答**：C++ 版本基于原生 Win32 API 架构，严格遵循 Google C++ 编程规范与 RAII 资源管理，执行效率极高、内存占用低，且不受系统 PowerShell 版本或脚本执行策略（Execution Policy）的限制。

---

## 🤝 开源贡献与协议

- **License**：本项目基于 [MIT License](LICENSE) 许可协议开源。
- **Contributions**：欢迎提交 Pull Request、报告 Issue 或提出新功能建议！
