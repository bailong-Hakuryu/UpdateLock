# UpdateLock - 微信与通用软件防自动更新/底层锁死免疫工具

[![Language](https://img.shields.io/badge/Language-C%2B%2B17%20%2F%20PowerShell-blue.svg)](https://github.com/bailong-Hakuryu/UpdateLock)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11%2FServer-0078D6.svg)](https://github.com/bailong-Hakuryu/UpdateLock)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Release](https://img.shields.io/badge/Release-v1.1.0-orange.svg)](https://github.com/bailong-Hakuryu/UpdateLock/releases)

`UpdateLock` 是一款专注于 Windows 操作系统的开源软件防自动更新与底层权限锁死免疫工具。通过原生 **Win32 API** 与 **NTFS 访问控制列表 (ACL)** 技术（基于国际化 **WinWorldSid / S-1-1-0**），帮助用户彻底解决微信 4.x / 3.x、WPS、百度网盘、钉钉等软件静默后台强行升级、占用资源或版本强制过期的问题。

---

## ⚠️ 重要免责声明 (Disclaimer)

1. **仅供学习与研究**：本项目（包括但不限于 C++ 源码、JSON 规则库、PowerShell 脚本及批处理启动器）仅作为 **Win32 API 编程、NTFS 权限管理机制及系统安全运维技术** 的学术交流与个人学习测试使用。
2. **非侵入性与安全性**：本项目不包含任何二进制外挂、反编译注入或非法破坏性代码。所有操作均基于 Windows 系统原生的文件权限配置（NTFS ACL）与标准进程管理 API。
3. **风险自负**：使用者在下载、编译或运行本项目前，应充分理解相关操作的效果。任何因非法使用、不当操作或修改软件所导致的软件故障、数据丢失或法律纠纷，**原作者及项目贡献者概不承担任何直接或连带责任**。
4. **尊重版权**：文中提及的微信（WeChat / xwechat）及相关商标版权均归属于其对应的软件著作权人腾讯公司（Tencent Holdings Ltd.）。

---

## 🔥 v1.1.0 核心功能与架构升级

### 1. WinWorldSid (S-1-1-0) 国际化 ACL 权限注入
- 摒弃依赖系统本地语言的字符串名称（如 `L"Everyone"`），全面升级为基于 **Well-Known SID (`WinWorldSid` / `S-1-1-0`)** 注入 `DENY` 拒绝权限（拒绝写入、删除、新建），在中文、英文及任意语言版本的 Windows 系统上均可稳定生效。

### 2. JSON 动态规则引擎 (规则与代码解耦)
- 引入 [`rules.json`](rules.json) 规则库，支持自定义扩展微信 4.x / 3.x、WPS、百度网盘、钉钉、QQ 等更多第三方软件的防更新路径。
- 支持路径模板变量（`%APPDATA%`, `%LOCALAPPDATA%`, `%WECHAT_INSTALL%` 等）。

### 3. 高级 CLI 命令行静默自动化
- 命令行支持无交互运行：
  - `--lock-all`：基于 `rules.json` 自动静默锁死全套开启规则。
  - `--unlock-all`：基于 `rules.json` 一键撤销权限锁。
  - `--check`：命令行输出全套规则与进程状态。
  - `--config <path>`：指定自定义规则 JSON 文件路径。

### 4. 微信 4.x (xwechat) & 3.x 深度防护通道
- **更新可执行文件锁死**：自动寻找 `WeChatUpdate.exe` 与 `WeixinUpdate.exe`，清理源文件并将其替换为具有 NTFS ACL 拒绝写入/删除权限的“免疫文件夹节点”。
- **XPlugin 动态插件目录封堵**：精准锁死 `WeixinUpdate` 与 `UpdateNotify` 等热更新插件路径（微信 4.x 偷偷下载后台更新的核心通道）。
- **更新下载缓存区隔离**：全面加锁 `%APPDATA%\Tencent\xwechat\update`、`%APPDATA%\Tencent\WeChat\All Users\config\update` 与 `%LOCALAPPDATA%\Tencent\WeChatApp\update` 下载目录。

---

## 📖 使用指南

### 模式一：CLI 命令行静默模式 (适合自动化脚本)

```cmd
:: 1. 基于 rules.json 自动静默锁死所有软件更新
UpdateLock.exe --lock-all

:: 2. 基于 rules.json 恢复所有软件更新
UpdateLock.exe --unlock-all

:: 3. 查看全局防护节点状态
UpdateLock.exe --check

:: 4. 使用自定义规则库文件
UpdateLock.exe --lock-all --config D:\my_rules.json
```

### 模式二：普通用户交互模式 (双击运行)

右键单击 **`UpdateLock.bat`**，选择 **【以管理员身份运行】**，在弹出的菜单中选择：
- 输入 `1`：一键锁死微信 4.x / 3.x 自动更新。
- 输入 `2`：一键解锁/恢复微信升级功能。
- 输入 `3`：检查各核心节点防护状态。
- 输入 `4` / `5`：自定义文件/目录路径加锁或解锁。

---

## 🛠️ C++17 编译与构建

项目提供 [`CMakeLists.txt`](CMakeLists.txt) 与自动化编译脚本 [`build.ps1`](build.ps1)。

```powershell
# 使用 CMake 构建
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

GitHub Actions 已配置全自动 CI/CD，当推送 `v*` Tag 时，将全自动生成包含 `UpdateLock.exe`、`rules.json` 及全套脚本的 Release ZIP 发布包。

---

## 🤝 开源协议 (License)

本项目基于 [MIT License](LICENSE) 许可协议开源。
