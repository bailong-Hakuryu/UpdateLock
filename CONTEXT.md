# UpdateLock Domain Context

## Project Domain Terms

- **Immunization Node (免疫节点)**: Replacing a Target `.exe` file with an empty directory and applying an NTFS `DENY` ACL rule to prevent the application from re-creating or modifying the updater executable.
- **WinWorldSid (`S-1-1-0`)**: The Windows Well-Known SID for the `Everyone` group, used in ACL access entries to achieve locale-independent permission control across all language editions of Windows.
- **Rule Database (`rules.json`)**: A JSON configuration file mapping applications (WeChat, WPS, BaiduNetDisk, etc.) to target paths with environment variable expansion (`%APPDATA%`, `%LOCALAPPDATA%`, `%WECHAT_INSTALL%`).
- **Process Manager**: Component using Win32 `CreateToolhelp32Snapshot` snapshot API to detect and terminate running updater processes (e.g. `WeChatUpdate.exe`, `WeixinUpdate.exe`) prior to ACL modification.
