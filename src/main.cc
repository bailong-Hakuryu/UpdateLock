#include <iostream>
#include <string>
#include <windows.h>
#include "security_manager.h"
#include "process_manager.h"
#include "immunizer.h"

namespace {

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
  std::wcout << L"==========================================================================" << std::endl;
  std::wcout << L"    UpdateLock 开源软件防自动更新/通用底层免疫工具 (Google C++ 架构)      " << std::endl;
  std::wcout << L"==========================================================================" << std::endl;
  std::wcout << L" 特性: Win32 NTFS ACL 权限级防御 | 自动发现路径 | 一键锁死/恢复 | 进程查杀 " << std::endl;
  std::wcout << L"--------------------------------------------------------------------------" << std::endl;

  if (!update_lock::security::IsAdminElevated()) {
    std::wcout << L"\x1b[31m[!] 警告: 当前未以【管理员权限】运行！\x1b[0m" << std::endl;
    std::wcout << L"\x1b[31m    修改 NTFS ACL 访问控制权限与停止系统进程需要管理员权限，请右键选择『以管理员身份运行』。\x1b[0m\n" << std::endl;
  } else {
    std::wcout << L"\x1b[32m[✓] 运行环境状态: 已获得管理员权限 (Administrator Elevated)\x1b[0m\n" << std::endl;
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

}  // namespace

int main() {
  SetupConsole();
  PrintHeader();

  int choice = -1;
  while (true) {
    ShowMenu();
    if (!(std::cin >> choice)) {
      std::cin.clear();
      std::cin.ignore(1000, '\n');
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
          bool is_exe = (custom_path.size() >= 4 && custom_path.substr(custom_path.size() - 4) == L".exe");
          if (update_lock::immunizer::LockCustomPath(custom_path, is_exe)) {
            std::wcout << L"\x1b[32m[✓] 自定义路径已成功加锁免疫！\x1b[0m" << std::endl;
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
            std::wcout << L"\x1b[32m[✓] 自定义路径已成功解锁！\x1b[0m" << std::endl;
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
    std::wcout << L"\n--------------------------------------------------------------------------" << std::endl;
  }

  return 0;
}
