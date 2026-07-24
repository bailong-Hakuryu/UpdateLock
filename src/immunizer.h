#ifndef UPDATE_LOCK_IMMUNIZER_H_
#define UPDATE_LOCK_IMMUNIZER_H_

#include <string>
#include <vector>
#include <windows.h>

namespace update_lock::immunizer {

enum class RuleType {
  kExecutableNode,
  kDirectoryNode
};

struct TargetRule {
  std::wstring path;
  RuleType type;
  std::wstring description;
  std::wstring app_id;  // e.g. "WeChat_4x"
};

// ---------------------------------------------------------------------------
// Environment / registry helpers
// ---------------------------------------------------------------------------
std::wstring GetEnvVar(const std::wstring& var_name);
std::wstring QueryWeChatInstallPath();

// ---------------------------------------------------------------------------
// Rule building
// ---------------------------------------------------------------------------

// Expands path templates (e.g. %APPDATA%, %WECHAT_INSTALL%) in a rule set.
std::wstring ExpandPathTemplate(const std::wstring& tmpl,
                                const std::wstring& wechat_install_path);

// Loads rules from rules.json (next to the executable or at |json_path|).
// Returns default built-in WeChat rules if the file cannot be read.
std::vector<TargetRule> LoadRulesFromJson(const std::wstring& json_path,
                                          bool enabled_only = true);

// Built-in fallback rules (WeChat 3.x / 4.x).
std::vector<TargetRule> BuildWeChatRules();

// ---------------------------------------------------------------------------
// Lock / unlock core
// ---------------------------------------------------------------------------
bool LockRule(const TargetRule& rule);
bool UnlockRule(const TargetRule& rule);

// ---------------------------------------------------------------------------
// High-level operations (interactive menu)
// ---------------------------------------------------------------------------
void LockWeChatAutoUpdate();
void UnlockWeChatAutoUpdate();
void CheckWeChatProtectionStatus();

// ---------------------------------------------------------------------------
// Generic custom-path API
// ---------------------------------------------------------------------------
bool LockCustomPath(const std::wstring& raw_path, bool is_exe_node);
bool UnlockCustomPath(const std::wstring& raw_path);

// ---------------------------------------------------------------------------
// Batch operations (used by CLI --lock-all / --unlock-all)
// ---------------------------------------------------------------------------
int  LockAllFromJson(const std::wstring& json_path);
int  UnlockAllFromJson(const std::wstring& json_path);
void CheckAllFromJson(const std::wstring& json_path);

}  // namespace update_lock::immunizer

#endif  // UPDATE_LOCK_IMMUNIZER_H_
