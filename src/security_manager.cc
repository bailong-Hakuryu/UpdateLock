#include "security_manager.h"
#include <aclapi.h>
#include <sddl.h>
#include <filesystem>
#include <iostream>

#pragma comment(lib, "advapi32.lib")

namespace update_lock::security {

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::wstring GetLastErrorMessage(DWORD error_code) {
  if (error_code == ERROR_SUCCESS) return L"ERROR_SUCCESS";
  LPWSTR msg_buf = nullptr;
  DWORD size = ::FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr, error_code,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      reinterpret_cast<LPWSTR>(&msg_buf), 0, nullptr);
  if (size == 0 || msg_buf == nullptr) {
    return L"Unknown error (" + std::to_wstring(error_code) + L")";
  }
  std::wstring result(msg_buf, size);
  ::LocalFree(msg_buf);
  // Trim trailing newline characters
  while (!result.empty() && (result.back() == L'\n' || result.back() == L'\r')) {
    result.pop_back();
  }
  result += L" (0x" + [error_code]() {
    wchar_t buf[12];
    ::swprintf_s(buf, L"%08X", error_code);
    return std::wstring(buf);
  }() + L")";
  return result;
}

// ---------------------------------------------------------------------------
// Builds a Well-Known SID for the "Everyone" group (WinWorldSid / S-1-1-0).
// This works correctly on all Windows language editions, unlike the
// locale-dependent name string approach.
// Returns an allocated SID via AllocateAndInitializeSid; caller must
// manage lifetime via ScopedSid.
// ---------------------------------------------------------------------------
static PSID BuildWorldSid() {
  SID_IDENTIFIER_AUTHORITY world_auth = SECURITY_WORLD_SID_AUTHORITY;
  PSID raw_sid = nullptr;
  if (!::AllocateAndInitializeSid(
          &world_auth, 1,
          SECURITY_WORLD_RID,
          0, 0, 0, 0, 0, 0, 0,
          &raw_sid)) {
    return nullptr;
  }
  return raw_sid;
}

// ---------------------------------------------------------------------------
// Admin check
// ---------------------------------------------------------------------------

bool IsAdminElevated() {
  BOOL is_admin = FALSE;
  HANDLE raw_token = NULL;

  if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
    ScopedHandle token_handle(raw_token);
    TOKEN_ELEVATION elevation{};
    DWORD cb_size = sizeof(TOKEN_ELEVATION);
    if (::GetTokenInformation(token_handle.get(), TokenElevation,
                              &elevation, sizeof(elevation), &cb_size)) {
      is_admin = elevation.TokenIsElevated != 0;
    }
  }
  return is_admin != FALSE;
}

// ---------------------------------------------------------------------------
// Path helpers
// ---------------------------------------------------------------------------

std::wstring SanitizePath(const std::wstring& raw_path) {
  if (raw_path.empty()) return L"";

  // Prevent path traversal sequences
  if (raw_path.find(L"..") != std::wstring::npos) {
    return L"";
  }

  try {
    fs::path p(raw_path);
    return p.lexically_normal().wstring();
  } catch (...) {
    return L"";
  }
}

bool PathExists(const std::wstring& path) {
  DWORD dw_attrib = ::GetFileAttributesW(path.c_str());
  return (dw_attrib != INVALID_FILE_ATTRIBUTES);
}

bool IsDirectory(const std::wstring& path) {
  DWORD dw_attrib = ::GetFileAttributesW(path.c_str());
  return (dw_attrib != INVALID_FILE_ATTRIBUTES &&
          (dw_attrib & FILE_ATTRIBUTE_DIRECTORY));
}

// ---------------------------------------------------------------------------
// SetFolderAccessDeny
// Injects a DENY ACE using the WinWorldSid (S-1-1-0) instead of the
// locale-dependent "Everyone" name, ensuring correct operation on all
// Windows language editions.
// ---------------------------------------------------------------------------
bool SetFolderAccessDeny(const std::wstring& path) {
  std::wstring clean_path = SanitizePath(path);
  if (clean_path.empty() || !PathExists(clean_path)) {
    std::wcerr << L"[ACL] SetFolderAccessDeny: invalid or missing path: "
               << path << std::endl;
    return false;
  }

  // Build Well-Known World SID (Everyone / S-1-1-0)
  ScopedSid world_sid(BuildWorldSid());
  if (!world_sid) {
    DWORD err = ::GetLastError();
    std::wcerr << L"[ACL] AllocateAndInitializeSid failed: "
               << GetLastErrorMessage(err) << std::endl;
    return false;
  }

  PACL raw_old_dacl = nullptr;
  PSECURITY_DESCRIPTOR raw_sd = nullptr;

  DWORD dw_res = ::GetNamedSecurityInfoW(
      clean_path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
      NULL, NULL, &raw_old_dacl, NULL, &raw_sd);

  if (dw_res != ERROR_SUCCESS) {
    std::wcerr << L"[ACL] GetNamedSecurityInfoW failed: "
               << GetLastErrorMessage(dw_res) << std::endl;
    return false;
  }

  ScopedLocalAlloc<SECURITY_DESCRIPTOR> sd_holder(
      static_cast<SECURITY_DESCRIPTOR*>(raw_sd));

  EXPLICIT_ACCESS_W ea{};
  ea.grfAccessPermissions =
      GENERIC_WRITE | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | DELETE;
  ea.grfAccessMode = DENY_ACCESS;
  ea.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
  // Use SID-based trustee — locale-independent
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  ea.Trustee.ptstrName = reinterpret_cast<LPWSTR>(world_sid.get());

  PACL raw_new_dacl = nullptr;
  dw_res = ::SetEntriesInAclW(1, &ea, raw_old_dacl, &raw_new_dacl);
  if (dw_res != ERROR_SUCCESS) {
    std::wcerr << L"[ACL] SetEntriesInAclW (deny) failed: "
               << GetLastErrorMessage(dw_res) << std::endl;
    return false;
  }

  ScopedLocalAlloc<ACL> dacl_holder(raw_new_dacl);

  dw_res = ::SetNamedSecurityInfoW(
      const_cast<LPWSTR>(clean_path.c_str()), SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION, NULL, NULL, dacl_holder.get(), NULL);

  if (dw_res != ERROR_SUCCESS) {
    std::wcerr << L"[ACL] SetNamedSecurityInfoW (deny) failed: "
               << GetLastErrorMessage(dw_res) << std::endl;
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// RemoveFolderAccessDeny
// Revokes DENY ACEs granted to the WinWorldSid.
// ---------------------------------------------------------------------------
bool RemoveFolderAccessDeny(const std::wstring& path) {
  std::wstring clean_path = SanitizePath(path);
  if (clean_path.empty() || !PathExists(clean_path)) {
    return false;
  }

  ScopedSid world_sid(BuildWorldSid());
  if (!world_sid) {
    DWORD err = ::GetLastError();
    std::wcerr << L"[ACL] AllocateAndInitializeSid failed: "
               << GetLastErrorMessage(err) << std::endl;
    return false;
  }

  PACL raw_old_dacl = nullptr;
  PSECURITY_DESCRIPTOR raw_sd = nullptr;

  DWORD dw_res = ::GetNamedSecurityInfoW(
      clean_path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION,
      NULL, NULL, &raw_old_dacl, NULL, &raw_sd);

  if (dw_res != ERROR_SUCCESS) {
    std::wcerr << L"[ACL] GetNamedSecurityInfoW failed: "
               << GetLastErrorMessage(dw_res) << std::endl;
    return false;
  }

  ScopedLocalAlloc<SECURITY_DESCRIPTOR> sd_holder(
      static_cast<SECURITY_DESCRIPTOR*>(raw_sd));

  EXPLICIT_ACCESS_W ea{};
  ea.grfAccessPermissions = GENERIC_ALL;
  ea.grfAccessMode = REVOKE_ACCESS;
  ea.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
  ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  ea.Trustee.ptstrName = reinterpret_cast<LPWSTR>(world_sid.get());

  PACL raw_new_dacl = nullptr;
  dw_res = ::SetEntriesInAclW(1, &ea, raw_old_dacl, &raw_new_dacl);
  if (dw_res != ERROR_SUCCESS) {
    std::wcerr << L"[ACL] SetEntriesInAclW (revoke) failed: "
               << GetLastErrorMessage(dw_res) << std::endl;
    return false;
  }

  ScopedLocalAlloc<ACL> dacl_holder(raw_new_dacl);

  dw_res = ::SetNamedSecurityInfoW(
      const_cast<LPWSTR>(clean_path.c_str()), SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION, NULL, NULL, dacl_holder.get(), NULL);

  if (dw_res != ERROR_SUCCESS) {
    std::wcerr << L"[ACL] SetNamedSecurityInfoW (revoke) failed: "
               << GetLastErrorMessage(dw_res) << std::endl;
    return false;
  }
  return true;
}

}  // namespace update_lock::security
