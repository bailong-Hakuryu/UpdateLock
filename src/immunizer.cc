#include "immunizer.h"
#include "security_manager.h"
#include "process_manager.h"
#include <windows.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace update_lock::immunizer {

// ===========================================================================
// Minimal JSON helpers (no external dependency)
// We parse only the subset of JSON that rules.json produces.
// ===========================================================================

namespace json_parse {

// Extract the string value of a key from a JSON object fragment.
// Returns L"" if not found.
static std::wstring GetStringW(const std::string& src, const std::string& key) {
  std::string search = "\"" + key + "\"";
  auto pos = src.find(search);
  if (pos == std::string::npos) return L"";
  pos = src.find(':', pos + search.size());
  if (pos == std::string::npos) return L"";
  pos = src.find('"', pos);
  if (pos == std::string::npos) return L"";
  ++pos;
  auto end = src.find('"', pos);
  if (end == std::string::npos) return L"";
  std::string narrow = src.substr(pos, end - pos);
  // Convert UTF-8 narrow string to wide
  int n = ::MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(),
                                 static_cast<int>(narrow.size()), nullptr, 0);
  if (n <= 0) return L"";
  std::wstring wide(n, L'\0');
  ::MultiByteToWideChar(CP_UTF8, 0, narrow.c_str(),
                         static_cast<int>(narrow.size()), &wide[0], n);
  return wide;
}

static bool GetBool(const std::string& src, const std::string& key, bool def) {
  std::string search = "\"" + key + "\"";
  auto pos = src.find(search);
  if (pos == std::string::npos) return def;
  pos = src.find(':', pos + search.size());
  if (pos == std::string::npos) return def;
  auto pos2 = src.find_first_not_of(" \t\r\n", pos + 1);
  if (pos2 == std::string::npos) return def;
  return src.substr(pos2, 4) == "true";
}

// Split JSON array content into individual object strings (shallow).
static std::vector<std::string> SplitObjects(const std::string& arr) {
  std::vector<std::string> out;
  int depth = 0;
  size_t start = std::string::npos;
  for (size_t i = 0; i < arr.size(); ++i) {
    if (arr[i] == '{') {
      if (depth == 0) start = i;
      ++depth;
    } else if (arr[i] == '}') {
      --depth;
      if (depth == 0 && start != std::string::npos) {
        out.push_back(arr.substr(start, i - start + 1));
        start = std::string::npos;
      }
    }
  }
  return out;
}

// Extract the JSON array text for a given key (top-level only).
static std::string GetArrayContent(const std::string& src, const std::string& key) {
  std::string search = "\"" + key + "\"";
  auto pos = src.find(search);
  if (pos == std::string::npos) return "";
  pos = src.find('[', pos + search.size());
  if (pos == std::string::npos) return "";
  int depth = 0;
  size_t start = pos;
  for (size_t i = pos; i < src.size(); ++i) {
    if (src[i] == '[') ++depth;
    else if (src[i] == ']') {
      --depth;
      if (depth == 0) return src.substr(start, i - start + 1);
    }
  }
  return "";
}

}  // namespace json_parse

// ===========================================================================
// Environment & registry helpers
// ===========================================================================

std::wstring GetEnvVar(const std::wstring& var_name) {
  DWORD size = ::GetEnvironmentVariableW(var_name.c_str(), nullptr, 0);
  if (size == 0) return L"";
  std::wstring res(size, L'\0');
  DWORD copied = ::GetEnvironmentVariableW(var_name.c_str(), &res[0], size);
  if (copied > 0 && copied < res.size()) {
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
  if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"SOFTWARE\\WOW6432Node\\Tencent\\WeChat", 0, KEY_READ,
                      &raw_key) == ERROR_SUCCESS) {
    security::ScopedHKey key_holder(raw_key);
    if (::RegQueryValueExW(key_holder.get(), L"InstallPath", NULL, &dw_type,
                           reinterpret_cast<LPBYTE>(buffer),
                           &dw_size) == ERROR_SUCCESS) {
      return std::wstring(buffer);
    }
  }

  // 2. Try HKCU registry key
  raw_key = NULL;
  dw_size = sizeof(buffer);
  if (::RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Tencent\\WeChat", 0,
                      KEY_READ, &raw_key) == ERROR_SUCCESS) {
    security::ScopedHKey key_holder(raw_key);
    if (::RegQueryValueExW(key_holder.get(), L"InstallPath", NULL, &dw_type,
                           reinterpret_cast<LPBYTE>(buffer),
                           &dw_size) == ERROR_SUCCESS) {
      return std::wstring(buffer);
    }
  }

  // 3. Fallback locations
  std::vector<std::wstring> fallbacks = {
      L"D:\\WeChat",
      L"C:\\Program Files\\Tencent\\WeChat",
      L"C:\\Program Files (x86)\\Tencent\\WeChat"};

  for (const auto& p : fallbacks) {
    if (security::PathExists(p)) return p;
  }

  return L"D:\\WeChat";
}

// ===========================================================================
// Path template expansion
// ===========================================================================

std::wstring ExpandPathTemplate(const std::wstring& tmpl,
                                const std::wstring& wechat_install_path) {
  std::wstring result = tmpl;
  auto replace_all = [&](const std::wstring& from, const std::wstring& to) {
    size_t pos = 0;
    while ((pos = result.find(from, pos)) != std::wstring::npos) {
      result.replace(pos, from.size(), to);
      pos += to.size();
    }
  };

  replace_all(L"%WECHAT_INSTALL%", wechat_install_path);
  replace_all(L"%APPDATA%",       GetEnvVar(L"APPDATA"));
  replace_all(L"%LOCALAPPDATA%",  GetEnvVar(L"LOCALAPPDATA"));
  replace_all(L"%TEMP%",          GetEnvVar(L"TEMP"));
  replace_all(L"%USERPROFILE%",   GetEnvVar(L"USERPROFILE"));
  return result;
}

// ===========================================================================
// JSON rule loading
// ===========================================================================

std::vector<TargetRule> LoadRulesFromJson(const std::wstring& json_path,
                                          bool enabled_only) {
  // Read UTF-8 file
  std::ifstream file(json_path);
  if (!file.is_open()) {
    std::wcerr << L"[rules] Cannot open rules file: " << json_path
               << L" - falling back to built-in WeChat rules." << std::endl;
    return BuildWeChatRules();
  }

  std::string content((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

  std::vector<TargetRule> rules;
  std::wstring wechat_install = QueryWeChatInstallPath();

  // Parse top-level "rules" array
  std::string rules_arr = json_parse::GetArrayContent(content, "rules");
  if (rules_arr.empty()) {
    std::wcerr << L"[rules] JSON 'rules' array not found; using built-in rules." << std::endl;
    return BuildWeChatRules();
  }

  auto app_objects = json_parse::SplitObjects(rules_arr);
  for (const auto& app_obj : app_objects) {
    bool enabled = json_parse::GetBool(app_obj, "enabled", false);
    if (enabled_only && !enabled) continue;

    std::wstring app_id = json_parse::GetStringW(app_obj, "app");
    std::wstring display = json_parse::GetStringW(app_obj, "display_name");

    std::string targets_arr = json_parse::GetArrayContent(app_obj, "targets");
    if (targets_arr.empty()) continue;

    auto target_objects = json_parse::SplitObjects(targets_arr);
    for (const auto& tgt_obj : target_objects) {
      std::wstring type_str = json_parse::GetStringW(tgt_obj, "type");
      std::wstring path_tmpl = json_parse::GetStringW(tgt_obj, "path_template");
      std::wstring desc = json_parse::GetStringW(tgt_obj, "description");

      if (path_tmpl.empty()) continue;

      TargetRule rule;
      rule.app_id = app_id;
      rule.description = desc.empty() ? (display + L" 节点") : desc;
      rule.path = ExpandPathTemplate(path_tmpl, wechat_install);
      rule.type = (type_str == L"exe_node") ? RuleType::kExecutableNode
                                            : RuleType::kDirectoryNode;
      rules.push_back(std::move(rule));
    }
  }

  if (rules.empty()) {
    std::wcerr << L"[rules] No enabled rules found in JSON; using built-in rules." << std::endl;
    return BuildWeChatRules();
  }

  return rules;
}

// ===========================================================================
// Built-in fallback rule set (WeChat 3.x / 4.x)
// ===========================================================================

std::vector<TargetRule> BuildWeChatRules() {
  std::vector<TargetRule> rules;
  std::wstring install_path = QueryWeChatInstallPath();
  std::wstring app_data     = GetEnvVar(L"APPDATA");
  std::wstring local_app    = GetEnvVar(L"LOCALAPPDATA");

  if (!install_path.empty()) {
    rules.push_back({install_path + L"\\WeChatUpdate.exe",
                     RuleType::kExecutableNode, L"微信旧版主更新可执行节点", L"WeChat_3x"});
    rules.push_back({install_path + L"\\WeixinUpdate.exe",
                     RuleType::kExecutableNode, L"微信 4.x 主更新可执行节点", L"WeChat_4x"});
  }

  if (!app_data.empty()) {
    rules.push_back({app_data + L"\\Tencent\\xwechat\\XPlugin\\Plugins\\WeixinUpdate",
                     RuleType::kDirectoryNode, L"xwechat 4.x 核心热更新插件目录", L"WeChat_4x"});
    rules.push_back({app_data + L"\\Tencent\\xwechat\\XPlugin\\Plugins\\UpdateNotify",
                     RuleType::kDirectoryNode, L"xwechat 4.x 更新通知插件目录", L"WeChat_4x"});
    rules.push_back({app_data + L"\\Tencent\\WeChat\\XPlugin\\Plugins\\WeChatUpdate",
                     RuleType::kDirectoryNode, L"WeChat 旧版热更新插件目录", L"WeChat_3x"});
    rules.push_back({app_data + L"\\Tencent\\WeChat\\XPlugin\\Plugins\\UpdateNotify",
                     RuleType::kDirectoryNode, L"WeChat 旧版更新通知目录", L"WeChat_3x"});
    rules.push_back({app_data + L"\\Tencent\\xwechat\\update",
                     RuleType::kDirectoryNode, L"xwechat 更新下载缓存目录", L"WeChat_4x"});
    rules.push_back({app_data + L"\\Tencent\\WeChat\\All Users\\config\\update",
                     RuleType::kDirectoryNode, L"WeChat 全局更新配置缓存目录", L"WeChat_3x"});
  }

  if (!local_app.empty()) {
    rules.push_back({local_app + L"\\Tencent\\WeChatApp\\update",
                     RuleType::kDirectoryNode, L"WeChatApp 本地更新缓存目录", L"WeChat_3x"});
  }

  return rules;
}

// ===========================================================================
// Lock / unlock primitives
// ===========================================================================

bool LockRule(const TargetRule& rule) {
  std::wstring clean_path = security::SanitizePath(rule.path);
  if (clean_path.empty()) return false;

  if (rule.type == RuleType::kExecutableNode) {
    // If a regular file exists here, remove it and replace with a dir node.
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
    return true;  // Already absent — treat as success.
  }

  bool ok = security::RemoveFolderAccessDeny(clean_path);

  if (rule.type == RuleType::kExecutableNode &&
      security::IsDirectory(clean_path)) {
    ::RemoveDirectoryW(clean_path.c_str());
  }

  return ok;
}

// ===========================================================================
// Interactive high-level operations
// ===========================================================================

void LockWeChatAutoUpdate() {
  std::wcout << L"\n[1/3] 正在终止后台静默更新进程..." << std::endl;
  int killed = process::TerminateProcessesByName(
      {L"WeChatUpdate.exe", L"WeixinUpdate.exe"});
  std::wcout << L"  [+] 已停止 " << killed << L" 个后台更新进程实例。"
             << std::endl;

  std::wcout << L"[2/3] 正在对核心节点实施 NTFS ACL 免疫加锁..." << std::endl;
  auto rules = BuildWeChatRules();
  int success_count = 0;

  for (const auto& rule : rules) {
    if (LockRule(rule)) {
      std::wcout << L"  [\u2713] 已成功锁死: " << rule.description << std::endl;
      std::wcout << L"      路径: " << rule.path << std::endl;
      ++success_count;
    } else {
      std::wcout << L"  [!] 锁死失败: " << rule.path << std::endl;
    }
  }

  std::wcout << L"[3/3] 完成: 已成功加锁 " << success_count << L"/"
             << rules.size() << L" 个更新通道！" << std::endl;
}

void UnlockWeChatAutoUpdate() {
  std::wcout << L"\n[1/2] 正在解除微信防更新权限锁..." << std::endl;
  auto rules = BuildWeChatRules();
  int unlocked = 0;

  for (const auto& rule : rules) {
    if (UnlockRule(rule)) {
      std::wcout << L"  [\u2713] 已恢复权限: " << rule.description << std::endl;
      ++unlocked;
    } else {
      std::wcout << L"  [!] 解锁失败: " << rule.path << std::endl;
    }
  }

  std::wcout << L"[2/2] 完成: 已成功解锁 " << unlocked << L"/"
             << rules.size()
             << L" 个节点，微信恢复正常升级能力。" << std::endl;
}

void CheckWeChatProtectionStatus() {
  std::wcout << L"\n== 微信自动更新防护状态检查 ==" << std::endl;

  bool is_running =
      process::IsProcessRunning(L"WeChatUpdate.exe") ||
      process::IsProcessRunning(L"WeixinUpdate.exe");
  std::wcout << L"[进程状态] 后台更新进程: "
             << (is_running ? L"\u274c 正在运行 (未安全锁死)"
                            : L"\u2705 未运行 (安全)")
             << std::endl;

  auto rules = BuildWeChatRules();
  std::wcout << L"[节点状态] 共监控 " << rules.size()
             << L" 个核心节点:" << std::endl;

  for (const auto& rule : rules) {
    bool exists = security::PathExists(rule.path);
    bool is_dir = security::IsDirectory(rule.path);
    std::wcout << L"  - " << rule.description << std::endl;
    std::wcout << L"    路径: " << rule.path << std::endl;
    if (!exists) {
      std::wcout << L"    状态: \u26aa 未建节点" << std::endl;
    } else if (rule.type == RuleType::kExecutableNode && is_dir) {
      std::wcout << L"    状态: \U0001f512 已替换为免疫文件夹" << std::endl;
    } else {
      std::wcout << L"    状态: \U0001f4c1 目录节点已存在" << std::endl;
    }
  }
}

// ===========================================================================
// Custom path API
// ===========================================================================

bool LockCustomPath(const std::wstring& raw_path, bool is_exe_node) {
  TargetRule rule;
  rule.path = raw_path;
  rule.type =
      is_exe_node ? RuleType::kExecutableNode : RuleType::kDirectoryNode;
  rule.description = L"用户自定义免疫节点";
  return LockRule(rule);
}

bool UnlockCustomPath(const std::wstring& raw_path) {
  TargetRule rule;
  rule.path = raw_path;
  rule.type = RuleType::kExecutableNode;  // type only affects directory-removal step
  rule.description = L"用户自定义解锁节点";
  return UnlockRule(rule);
}

// ===========================================================================
// Batch CLI operations
// ===========================================================================

int LockAllFromJson(const std::wstring& json_path) {
  auto rules = LoadRulesFromJson(json_path, /*enabled_only=*/true);
  int killed = process::TerminateProcessesByName(
      {L"WeChatUpdate.exe", L"WeixinUpdate.exe"});
  if (killed > 0) {
    std::wcout << L"[+] 已停止 " << killed << L" 个后台更新进程。" << std::endl;
  }
  int success = 0;
  for (const auto& rule : rules) {
    if (LockRule(rule)) {
      std::wcout << L"  [\u2713] 锁死: " << rule.description
                 << L"  [" << rule.app_id << L"]" << std::endl;
      ++success;
    } else {
      std::wcout << L"  [!] 失败: " << rule.path << std::endl;
    }
  }
  std::wcout << L"完成: " << success << L"/" << rules.size()
             << L" 个节点加锁成功。" << std::endl;
  return success;
}

int UnlockAllFromJson(const std::wstring& json_path) {
  auto rules = LoadRulesFromJson(json_path, /*enabled_only=*/true);
  int success = 0;
  for (const auto& rule : rules) {
    if (UnlockRule(rule)) {
      std::wcout << L"  [\u2713] 解锁: " << rule.description << std::endl;
      ++success;
    } else {
      std::wcout << L"  [!] 失败: " << rule.path << std::endl;
    }
  }
  std::wcout << L"完成: " << success << L"/" << rules.size()
             << L" 个节点解锁成功。" << std::endl;
  return success;
}

void CheckAllFromJson(const std::wstring& json_path) {
  auto rules = LoadRulesFromJson(json_path, /*enabled_only=*/true);
  std::wcout << L"\n== UpdateLock 全局防护状态 == (来源: " << json_path << L")"
             << std::endl;
  for (const auto& rule : rules) {
    bool exists = security::PathExists(rule.path);
    bool is_dir = security::IsDirectory(rule.path);
    std::wcout << L"  [" << rule.app_id << L"] " << rule.description << std::endl;
    std::wcout << L"    路径: " << rule.path << std::endl;
    if (!exists) {
      std::wcout << L"    状态: \u26aa 未建节点" << std::endl;
    } else if (rule.type == RuleType::kExecutableNode && is_dir) {
      std::wcout << L"    状态: \U0001f512 已锁死 (免疫文件夹)" << std::endl;
    } else {
      std::wcout << L"    状态: \U0001f4c1 目录节点存在" << std::endl;
    }
  }
}

}  // namespace update_lock::immunizer
