#ifndef UPDATE_LOCK_PROCESS_MANAGER_H_
#define UPDATE_LOCK_PROCESS_MANAGER_H_

#include <windows.h>
#include <tlhelp32.h>
#include <string>
#include <vector>

namespace update_lock::process {

// Terminates all running processes matching any target process name
int TerminateProcessesByName(const std::vector<std::wstring>& process_names);

// Checks if a target process is running
bool IsProcessRunning(const std::wstring& process_name);

}  // namespace update_lock::process

#endif  // UPDATE_LOCK_PROCESS_MANAGER_H_
