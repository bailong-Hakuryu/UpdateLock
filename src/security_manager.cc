#include "security_manager.h"
#include <aclapi.h>
#include <sddl.h>
#include <filesystem>
#include <iostream>

#pragma comment(lib, "advapi32.lib")

namespace update_lock::security {

namespace fs = std::filesystem;

bool IsAdminElevated() {
  BOOL is_admin = FALSE;
  HANDLE raw_token = NULL;

  if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
    ScopedHandle token_handle(raw_token);
    TOKEN_ELEVATION elevation{};
    DWORD cb_size = sizeof(TOKEN_ELEVATION);
    if (::GetTokenInformation(token_handle.get(), TokenElevation, &elevation, sizeof(elevation), &cb_size)) {
      is_admin = elevation.TokenIsElevated != 0;
    }
  }
  return is_admin != FALSE;
}

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
  return (dw_attrib != INVALID_FILE_ATTRIBUTES && (dw_attrib & FILE_ATTRIBUTE_DIRECTORY));
}

bool SetFolderAccessDeny(const std::wstring& path) {
  std::wstring clean_path = SanitizePath(path);
  if (clean_path.empty() || !PathExists(clean_path)) {
    return false;
  }

  PACL raw_old_dacl = nullptr;
  PSECURITY_DESCRIPTOR raw_sd = nullptr;

  DWORD dw_res = ::GetNamedSecurityInfoW(
      clean_path.c_str(),
      SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION,
      NULL, NULL, &raw_old_dacl, NULL, &raw_sd
  );

  if (dw_res != ERROR_SUCCESS) {
    return false;
  }

  ScopedLocalAlloc<SECURITY_DESCRIPTOR> sd_holder(static_cast<SECURITY_DESCRIPTOR*>(raw_sd));

  EXPLICIT_ACCESS_W ea{};
  ea.grfAccessPermissions = GENERIC_WRITE | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | DELETE;
  ea.grfAccessMode = DENY_ACCESS;
  ea.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
  ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  ea.Trustee.ptstrName = const_cast<LPWSTR>(L"Everyone");

  PACL raw_new_dacl = nullptr;
  dw_res = ::SetEntriesInAclW(1, &ea, raw_old_dacl, &raw_new_dacl);
  if (dw_res != ERROR_SUCCESS) {
    return false;
  }

  ScopedLocalAlloc<ACL> dacl_holder(raw_new_dacl);

  dw_res = ::SetNamedSecurityInfoW(
      const_cast<LPWSTR>(clean_path.c_str()),
      SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION,
      NULL, NULL, dacl_holder.get(), NULL
  );

  return (dw_res == ERROR_SUCCESS);
}

bool RemoveFolderAccessDeny(const std::wstring& path) {
  std::wstring clean_path = SanitizePath(path);
  if (clean_path.empty() || !PathExists(clean_path)) {
    return false;
  }

  PACL raw_old_dacl = nullptr;
  PSECURITY_DESCRIPTOR raw_sd = nullptr;

  DWORD dw_res = ::GetNamedSecurityInfoW(
      clean_path.c_str(),
      SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION,
      NULL, NULL, &raw_old_dacl, NULL, &raw_sd
  );

  if (dw_res != ERROR_SUCCESS) {
    return false;
  }

  ScopedLocalAlloc<SECURITY_DESCRIPTOR> sd_holder(static_cast<SECURITY_DESCRIPTOR*>(raw_sd));

  EXPLICIT_ACCESS_W ea{};
  ea.grfAccessPermissions = GENERIC_ALL;
  ea.grfAccessMode = REVOKE_ACCESS;
  ea.grfInheritance = OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE;
  ea.Trustee.TrusteeForm = TRUSTEE_IS_NAME;
  ea.Trustee.TrusteeType = TRUSTEE_IS_WELL_KNOWN_GROUP;
  ea.Trustee.ptstrName = const_cast<LPWSTR>(L"Everyone");

  PACL raw_new_dacl = nullptr;
  dw_res = ::SetEntriesInAclW(1, &ea, raw_old_dacl, &raw_new_dacl);
  if (dw_res != ERROR_SUCCESS) {
    return false;
  }

  ScopedLocalAlloc<ACL> dacl_holder(raw_new_dacl);

  dw_res = ::SetNamedSecurityInfoW(
      const_cast<LPWSTR>(clean_path.c_str()),
      SE_FILE_OBJECT,
      DACL_SECURITY_INFORMATION,
      NULL, NULL, dacl_holder.get(), NULL
  );

  return (dw_res == ERROR_SUCCESS);
}

}  // namespace update_lock::security
