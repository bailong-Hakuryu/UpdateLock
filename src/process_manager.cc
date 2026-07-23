#include "process_manager.h"
#include "security_manager.h"
#include <cwctype>
#include <iostream>

namespace update_lock::process {

namespace {
bool EqualsIgnoreCase(const std::wstring& s1, const std::wstring& s2) {
  if (s1.size() != s2.size()) return false;
  for (size_t i = 0; i < s1.size(); ++i) {
    if (std::towlower(s1[i]) != std::towlower(s2[i])) return false;
  }
  return true;
}
}  // namespace

int TerminateProcessesByName(const std::vector<std::wstring>& process_names) {
  int terminated_count = 0;
  HANDLE raw_snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (raw_snap == INVALID_HANDLE_VALUE) {
    return 0;
  }

  security::ScopedHandle snap_holder(raw_snap);

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(PROCESSENTRY32W);

  if (::Process32FirstW(snap_holder.get(), &pe)) {
    do {
      std::wstring exe_name = pe.szExeFile;
      for (const auto& target : process_names) {
        if (EqualsIgnoreCase(exe_name, target)) {
          HANDLE raw_proc = ::OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
          if (raw_proc != NULL) {
            security::ScopedHandle proc_holder(raw_proc);
            if (::TerminateProcess(proc_holder.get(), 0)) {
              terminated_count++;
            }
          }
          break;
        }
      }
    } while (::Process32NextW(snap_holder.get(), &pe));
  }

  return terminated_count;
}

bool IsProcessRunning(const std::wstring& process_name) {
  HANDLE raw_snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (raw_snap == INVALID_HANDLE_VALUE) {
    return false;
  }

  security::ScopedHandle snap_holder(raw_snap);

  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(PROCESSENTRY32W);
  bool found = false;

  if (::Process32FirstW(snap_holder.get(), &pe)) {
    do {
      if (EqualsIgnoreCase(pe.szExeFile, process_name)) {
        found = true;
        break;
      }
    } while (::Process32NextW(snap_holder.get(), &pe));
  }

  return found;
}

}  // namespace update_lock::process
