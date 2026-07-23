#ifndef UPDATE_LOCK_IMMUNIZER_H_
#define UPDATE_LOCK_IMMUNIZER_H_

#include <string>
#include <vector>

namespace update_lock::immunizer {

enum class RuleType {
  kExecutableNode,
  kDirectoryNode
};

struct TargetRule {
  std::wstring path;
  RuleType type;
  std::wstring description;
};

std::wstring GetEnvVar(const std::wstring& var_name);
std::wstring QueryWeChatInstallPath();
std::vector<TargetRule> BuildWeChatRules();

bool LockRule(const TargetRule& rule);
bool UnlockRule(const TargetRule& rule);

void LockWeChatAutoUpdate();
void UnlockWeChatAutoUpdate();
void CheckWeChatProtectionStatus();

bool LockCustomPath(const std::wstring& raw_path, bool is_exe_node);
bool UnlockCustomPath(const std::wstring& raw_path);

}  // namespace update_lock::immunizer

#endif  // UPDATE_LOCK_IMMUNIZER_H_
