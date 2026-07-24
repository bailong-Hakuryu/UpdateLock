#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include "security_manager.h"
#include "process_manager.h"
#include "immunizer.h"
#include "version.h"

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace {

// ---------------------------------------------------------------------------
// Console setup
// ---------------------------------------------------------------------------
void SetupConsole() {
  ::SetConsoleOutputCP(CP_UTF8);
  ::SetConsoleCP(CP_UTF8);

  HANDLE hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut != INVALID_HANDLE_VALUE) {
    DWORD dw_mode = 0;
    if (::GetConsoleMode(hOut, &dw_mode)) {
      dw_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
      ::SetConsoleMode(hOut, dw_mode);
    }
  }
}

void PrintHeader() {
  std::wcout
      << L"==========================================================================" << std::endl
      << L"    UpdateLock v" << UPDATELOCK_VERSION_W
      << L" | 开源软件防自动更新/底层免疫工具 (Win32 ACL)" << std::endl
      << L"==========================================================================" << std::endl
      << L" 特性: WinWorldSid ACL 防御 | 自动发现路径 | JSON 规则库 | CLI 自动化 " << std::endl
      << L"--------------------------------------------------------------------------" << std::endl;

  if (!update_lock::security::IsAdminElevated()) {
    std::wcout << L"\x1b[31m[!] 警告: 未以【管理员权限】运行！修改 NTFS ACL 需要管理员身份。\x1b[0m\n"
               << std::endl;
  } else {
    std::wcout << L"\x1b[32m[\u2713] 运行环境: 已获得管理员权限 (Administrator Elevated)\x1b[0m\n"
               << std::endl;
  }
}

void ShowMenu() {
  std::wcout << L"\n请选择要执行的操作:" << std::endl;
  std::wcout << L"  [1] 一键锁死 微信 4.x / 3.x 全套自动更新通道" << std::endl;
  std::wcout << L"  [2] 一键解锁 / 恢复 微信自动更新能力" << std::endl;
  std::wcout << L"  [3] 检查 微信自动更新防护与节点状态" << std::endl;
  std::wcout << L"  [4] 自定义软件更新文件/目录加锁 (通用防更新)" << std::endl;
  std::wcout << L"  [5] 自定义软件更新文件/目录解锁" << std::endl;
  std::wcout << L"  [0] 退出程序" << std::endl;
  std::wcout << L"输入选项序号 [0-5]: ";
}

// ---------------------------------------------------------------------------
// Returns the directory of the running executable (for locating rules.json).
// ---------------------------------------------------------------------------
std::wstring GetExeDir() {
  wchar_t buf[MAX_PATH] = {0};
  DWORD n = ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
  if (n == 0) return L".";
  std::wstring path(buf, n);
  auto slash = path.rfind(L'\\');
  if (slash != std::wstring::npos) path.resize(slash);
  return path;
}

// ---------------------------------------------------------------------------
// CLI help text
// ---------------------------------------------------------------------------
void PrintHelp(const wchar_t* exe_name) {
  std::wcout
      << L"\n使用方式 / Usage:\n"
      << L"  " << exe_name << L" [options]\n\n"
      << L"选项 / Options:\n"
      << L"  (无参数)           启动交互式菜单 (默认模式)\n"
      << L"  --lock-all         从 rules.json 加锁所有 enabled 软件的更新通道\n"
      << L"  --unlock-all       从 rules.json 解锁所有 enabled 软件的更新通道\n"
      << L"  --check            从 rules.json 检查所有节点的防护状态\n"
      << L"  --config <path>    指定 rules.json 路径（默认: 可执行文件同目录）\n"
      << L"  --help, -h         显示此帮助信息\n"
      << L"  --version, -v      显示版本信息\n\n"
      << L"示例 / Examples:\n"
      << L"  UpdateLock.exe --lock-all\n"
      << L"  UpdateLock.exe --lock-all --config D:\\myrules.json\n"
      << L"  UpdateLock.exe --unlock-all\n"
      << L"  UpdateLock.exe --check\n"
      << std::endl;
}

// ---------------------------------------------------------------------------
// Interactive menu loop
// ---------------------------------------------------------------------------
void RunInteractive() {
  int choice = -1;
  while (true) {
    ShowMenu();
    if (!(std::wcin >> choice)) {
      std::wcin.clear();
      std::wcin.ignore(1000, L'\n');
      continue;
    }

    if (choice == 0) {
      std::wcout << L"\n感谢使用 UpdateLock 开源项目，程序已安全退出。" << std::endl;
      break;
    }

    switch (choice) {
      case 1:
        update_lock::immunizer::LockWeChatAutoUpdate();
        break;
      case 2:
        update_lock::immunizer::UnlockWeChatAutoUpdate();
        break;
      case 3:
        update_lock::immunizer::CheckWeChatProtectionStatus();
        break;
      case 4: {
        std::wcin.ignore();
        std::wstring custom_path;
        std::wcout << L"\n请输入要锁死的更新文件或目录绝对路径:\n> ";
        std::getline(std::wcin, custom_path);
        if (!custom_path.empty()) {
          bool is_exe = (custom_path.size() >= 4 &&
                         custom_path.substr(custom_path.size() - 4) == L".exe");
          if (update_lock::immunizer::LockCustomPath(custom_path, is_exe)) {
            std::wcout << L"\x1b[32m[\u2713] 自定义路径已成功加锁免疫！\x1b[0m" << std::endl;
          } else {
            std::wcout << L"\x1b[31m[!] 加锁失败，请确认路径与管理员权限。\x1b[0m" << std::endl;
          }
        }
        break;
      }
      case 5: {
        std::wcin.ignore();
        std::wstring custom_path;
        std::wcout << L"\n请输入要恢复权限的绝对路径:\n> ";
        std::getline(std::wcin, custom_path);
        if (!custom_path.empty()) {
          if (update_lock::immunizer::UnlockCustomPath(custom_path)) {
            std::wcout << L"\x1b[32m[\u2713] 自定义路径已成功解锁！\x1b[0m" << std::endl;
          } else {
            std::wcout << L"\x1b[31m[!] 解锁失败。\x1b[0m" << std::endl;
          }
        }
        break;
      }
      default:
        std::wcout << L"无效选项，请重新输入。" << std::endl;
        break;
    }
    std::wcout << L"\n--------------------------------------------------------------------------"
               << std::endl;
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int wmain(int argc, wchar_t* argv[]) {
  SetupConsole();
  PrintHeader();

  // Default rules.json path: same directory as the executable.
  std::wstring json_path = GetExeDir() + L"\\rules.json";

  // Parse CLI arguments
  bool cli_lock_all   = false;
  bool cli_unlock_all = false;
  bool cli_check      = false;
  bool cli_help       = false;
  bool cli_version    = false;

  for (int i = 1; i < argc; ++i) {
    std::wstring arg = argv[i];
    if (arg == L"--lock-all") {
      cli_lock_all = true;
    } else if (arg == L"--unlock-all") {
      cli_unlock_all = true;
    } else if (arg == L"--check") {
      cli_check = true;
    } else if ((arg == L"--config") && i + 1 < argc) {
      json_path = argv[++i];
    } else if (arg == L"--help" || arg == L"-h") {
      cli_help = true;
    } else if (arg == L"--version" || arg == L"-v") {
      cli_version = true;
    } else {
      std::wcerr << L"未知参数: " << arg
                 << L"  运行 --help 查看使用说明。" << std::endl;
      return 1;
    }
  }

  if (cli_version) {
    std::wcout << L"UpdateLock v" << UPDATELOCK_VERSION_W << std::endl;
    return 0;
  }

  if (cli_help) {
    PrintHelp(argv[0]);
    return 0;
  }

  // --- Non-interactive CLI mode ---
  if (cli_lock_all || cli_unlock_all || cli_check) {
    if (cli_lock_all) {
      std::wcout << L"[CLI] --lock-all  规则文件: " << json_path << std::endl;
      update_lock::immunizer::LockAllFromJson(json_path);
    }
    if (cli_unlock_all) {
      std::wcout << L"[CLI] --unlock-all  规则文件: " << json_path << std::endl;
      update_lock::immunizer::UnlockAllFromJson(json_path);
    }
    if (cli_check) {
      update_lock::immunizer::CheckAllFromJson(json_path);
    }
    return 0;
  }

  // --- Interactive mode (default) ---
  RunInteractive();
  return 0;
}
