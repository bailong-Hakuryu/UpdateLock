#include "immunizer.h"
#include "security_manager.h"
#include "process_manager.h"
#include <windows.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace update_lock::immunizer {

std::wstring GetEnvVar(const std::wstring& var_name) {
  DWORD size = ::GetEnvironmentVariableW(var_name.c_str(), nullptr, 0);
  if (size == 0) return L"";
  std::wstring res(size, L'\0');
  DWORD copied = ::GetEnvironmentVariableW(var_name.c_str(), res.data(), size);
  if (copied > 0 && copied < size) {
    res.resize(copied);
    return res;
  }
  return L"";
}

std::wstring QueryWeChatInstallPath() {
  wchar_t buffer[MAX_PATH] = {0};
  DWORD dw_type = REG_SZ;
  DWORD dw_size = sizeof(buffer);

  // 1. Try HKLM 64-bit / WOW64 registry key
  HKEY raw_key = NULL;
  if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Tencent\\WeChat", 0, KEY_READ, &raw_key) == ERROR_SUCCESS) {
    security::ScopedHKey key_holder(raw_key);
    if (::RegQueryValueExW(key_holder.get(), L"InstallPath", NULL, &dw_type, reinterpret_cast<LPBYTE>(buffer), &dw_size) == ERROR_SUCCESS) {
      return std::wstring(buffer);
    }
  }

  // 2. Try HKCU registry key
  raw_key = NULL;
  dw_size = sizeof(buffer);
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Tencent\\WeChat", 0, KEY_READ, &raw_key) == ERROR_SUCCESS) {
    security::ScopedHKey key_holder(raw_key);
    if (::RegQueryValueExW(key_holder.get(), L"InstallPath", NULL, &dw_type, reinterpret_cast<LPBYTE>(buffer), &dw_size) == ERROR_SUCCESS) {
      return std::wstring(buffer);
    }
  }

  // 3. Fallback locations
  std::vector<std::wstring> fallbacks = {
      L"D:\\WeChat",
      L"C:\\Program Files\\Tencent\\WeChat",
      L"C:\\Program Files (x86)\\Tencent\\WeChat"
  };

  for (const auto& path : fallbacks) {
    if (security::PathExists(path)) {
      return path;
    }
  }

  return L"D:\\WeChat";
}

std::vector<TargetRule> BuildWeChatRules() {
  std::vector<TargetRule> rules;
  std::wstring install_path = QueryWeChatInstallPath();
  std::wstring app_data = GetEnvVar(L"APPDATA");
  std::wstring local_app_data = GetEnvVar(L"LOCALAPPDATA");

  if (!install_path.empty()) {
    rules.push_back({install_path + L"\\WeChatUpdate.exe", RuleType::kExecutableNode, L"微信旧版主更新可执行节点"});
    rules.push_back({install_path + L"\\WeixinUpdate.exe", RuleType::kExecutableNode, L"微信 4.x 主更新可执行节点"});
  }

  if (!app_data.empty()) {
    rules.push_back({app_data + L"\\Tencent\\xwechat\\XPlugin\\Plugins\\WeixinUpdate", RuleType::kDirectoryNode, L"xwechat 4.x 核心热更新插件目录"});
    rules.push_back({app_data + L"\\Tencent\\xwechat\\XPlugin\\Plugins\\UpdateNotify", RuleType::kDirectoryNode, L"xwechat 4.x 更新通知插件目录"});
    rules.push_back({app_data + L"\\Tencent\\WeChat\\XPlugin\\Plugins\\WeChatUpdate", RuleType::kDirectoryNode, L"WeChat 旧版热更新插件目录"});
    rules.push_back({app_data + L"\\Tencent\\WeChat\\XPlugin\\Plugins\\UpdateNotify", RuleType::kDirectoryNode, L"WeChat 旧版更新通知目录"});

    rules.push_back({app_data + L"\\Tencent\\xwechat\\update", RuleType::kDirectoryNode, L"xwechat 更新下载缓存目录"});
    rules.push_back({app_data + L"\\Tencent\\WeChat\\All Users\\config\\update", RuleType::kDirectoryNode, L"WeChat 全局更新配置缓存目录"});
  }

  if (!local_app_data.empty()) {
    rules.push_back({local_app_data + L"\\Tencent\\WeChatApp\\update", RuleType::kDirectoryNode, L"WeChatApp 本地更新缓存目录"});
  }

  return rules;
}

bool LockRule(const TargetRule& rule) {
  std::wstring clean_path = security::SanitizePath(rule.path);
  if (clean_path.empty()) return false;

  if (rule.type == RuleType::kExecutableNode) {
    if (security::PathExists(clean_path)) {
      if (!security::IsDirectory(clean_path)) {
        security::RemoveFolderAccessDeny(clean_path);
        ::DeleteFileW(clean_path.c_str());
      }
    }
    if (!security::PathExists(clean_path)) {
      ::CreateDirectoryW(clean_path.c_str(), NULL);
    }
  } else {
    if (!security::PathExists(clean_path)) {
      fs::create_directories(clean_path);
    }
  }

  return security::SetFolderAccessDeny(clean_path);
}

bool UnlockRule(const TargetRule& rule) {
  std::wstring clean_path = security::SanitizePath(rule.path);
  if (clean_path.empty() || !security::PathExists(clean_path)) {
    return true;
  }

  bool ok = security::RemoveFolderAccessDeny(clean_path);

  if (rule.type == RuleType::kExecutableNode && security::IsDirectory(clean_path)) {
    ::RemoveDirectoryW(clean_path.c_str());
  }

  return ok;
}

void LockWeChatAutoUpdate() {
  std::wcout << L"\n[1/3] 正在终止后台静默更新进程..." << std::endl;
  int killed = process::TerminateProcessesByName({L"WeChatUpdate.exe", L"WeixinUpdate.exe"});
  std::wcout << L"  [+] 已停止 " << killed << L" 个后台更新进程实例。" << std::endl;

  std::wcout << L"[2/3] 正在对核心节点实施 NTFS ACL 免疫加锁..." << std::endl;
  auto rules = BuildWeChatRules();
  int success_count = 0;

  for (const auto& rule : rules) {
    if (LockRule(rule)) {
      std::wcout << L"  [✓] 已成功锁死: " << rule.description << std::endl;
      std::wcout << L"      路径: " << rule.path << std::endl;
      success_count++;
    } else {
      std::wcout << L"  [!] 锁死失败: " << rule.path << std::endl;
    }
  }

  std::wcout << L"[3/3] 完成: 已成功加锁 " << success_count << L"/" << rules.size() << L" 个更新通道！" << std::endl;
}

void UnlockWeChatAutoUpdate() {
  std::wcout << L"\n[1/2] 正在解除微信防更新权限锁..." << std::endl;
  auto rules = BuildWeChatRules();
  int unlocked_count = 0;

  for (const auto& rule : rules) {
    if (UnlockRule(rule)) {
      std::wcout << L"  [✓] 已恢复权限: " << rule.description << std::endl;
      unlocked_count++;
    } else {
      std::wcout << L"  [!] 解锁失败: " << rule.path << std::endl;
    }
  }

  std::wcout << L"[2/2] 完成: 已成功解锁 " << unlocked_count << L"/" << rules.size() << L" 个节点，微信恢复正常升级能力。" << std::endl;
}

void CheckWeChatProtectionStatus() {
  std::wcout << L"\n== 微信自动更新防护状态检查 ==" << std::endl;

  bool is_running = process::IsProcessRunning(L"WeChatUpdate.exe") || process::IsProcessRunning(L"WeixinUpdate.exe");
  std::wcout << L"[进程状态] 后台更新进程: " << (is_running ? L"❌ 正在运行 (未安全锁死)" : L"✅ 未运行 (安全)") << std::endl;

  auto rules = BuildWeChatRules();
  std::wcout << L"[节点状态] 共监控 " << rules.size() << L" 个核心节点:" << std::endl;

  for (const auto& rule : rules) {
    bool exists = security::PathExists(rule.path);
    bool is_dir = security::IsDirectory(rule.path);
    std::wcout << L"  - " << rule.description << std::endl;
    std::wcout << L"    路径: " << rule.path << std::endl;
    if (!exists) {
      std::wcout << L"    状态: ⚪ 未建节点" << std::endl;
    } else if (rule.type == RuleType::kExecutableNode && is_dir) {
      std::wcout << L"    状态: 🔒 已替换为免疫文件夹" << std::endl;
    } else {
      std::wcout << L"    状态: 📁 目录节点已存在" << std::endl;
    }
  }
}

bool LockCustomPath(const std::wstring& raw_path, bool is_exe_node) {
  TargetRule rule;
  rule.path = raw_path;
  rule.type = is_exe_node ? RuleType::kExecutableNode : RuleType::kDirectoryNode;
  rule.description = L"用户自定义免疫节点";
  return LockRule(rule);
}

bool UnlockCustomPath(const std::wstring& raw_path) {
  TargetRule rule;
  rule.path = raw_path;
  rule.type = RuleType::kExecutableNode;
  rule.description = L"用户自定义解锁节点";
  return UnlockRule(rule);
}

}  // namespace update_lock::immunizer
