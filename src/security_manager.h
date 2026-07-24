#ifndef UPDATE_LOCK_SECURITY_MANAGER_H_
#define UPDATE_LOCK_SECURITY_MANAGER_H_

#include <windows.h>
#include <string>
#include <memory>

namespace update_lock::security {

// RAII Wrapper for Win32 HANDLE
class ScopedHandle {
 public:
  explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE) : handle_(handle) {}
  ~ScopedHandle() {
    if (IsValid()) {
      ::CloseHandle(handle_);
    }
  }

  ScopedHandle(const ScopedHandle&) = delete;
  ScopedHandle& operator=(const ScopedHandle&) = delete;

  ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = INVALID_HANDLE_VALUE;
  }

  ScopedHandle& operator=(ScopedHandle&& other) noexcept {
    if (this != &other) {
      if (IsValid()) {
        ::CloseHandle(handle_);
      }
      handle_ = other.handle_;
      other.handle_ = INVALID_HANDLE_VALUE;
    }
    return *this;
  }

  [[nodiscard]] bool IsValid() const {
    return handle_ != NULL && handle_ != INVALID_HANDLE_VALUE;
  }

  [[nodiscard]] HANDLE get() const { return handle_; }

 private:
  HANDLE handle_;
};

// RAII Wrapper for Win32 HKEY
class ScopedHKey {
 public:
  explicit ScopedHKey(HKEY hKey = NULL) : hKey_(hKey) {}
  ~ScopedHKey() {
    if (IsValid()) {
      ::RegCloseKey(hKey_);
    }
  }

  ScopedHKey(const ScopedHKey&) = delete;
  ScopedHKey& operator=(const ScopedHKey&) = delete;

  ScopedHKey(ScopedHKey&& other) noexcept : hKey_(other.hKey_) {
    other.hKey_ = NULL;
  }

  ScopedHKey& operator=(ScopedHKey&& other) noexcept {
    if (this != &other) {
      if (IsValid()) {
        ::RegCloseKey(hKey_);
      }
      hKey_ = other.hKey_;
      other.hKey_ = NULL;
    }
    return *this;
  }

  [[nodiscard]] bool IsValid() const {
    return hKey_ != NULL;
  }

  [[nodiscard]] HKEY get() const { return hKey_; }

 private:
  HKEY hKey_;
};

// RAII Wrapper for LocalFree allocated Win32 memory (PSECURITY_DESCRIPTOR, PACL)
template <typename T>
class ScopedLocalAlloc {
 public:
  explicit ScopedLocalAlloc(T* ptr = nullptr) : ptr_(ptr) {}
  ~ScopedLocalAlloc() {
    if (ptr_) {
      ::LocalFree(reinterpret_cast<HLOCAL>(ptr_));
    }
  }

  ScopedLocalAlloc(const ScopedLocalAlloc&) = delete;
  ScopedLocalAlloc& operator=(const ScopedLocalAlloc&) = delete;

  T* get() const { return ptr_; }
  T** operator&() { return &ptr_; }
  explicit operator bool() const { return ptr_ != nullptr; }

 private:
  T* ptr_;
};

// RAII Wrapper for Win32 PSID allocated via FreeSid
class ScopedSid {
 public:
  explicit ScopedSid(PSID sid = nullptr) : sid_(sid) {}
  ~ScopedSid() {
    if (sid_) {
      ::FreeSid(sid_);
    }
  }

  ScopedSid(const ScopedSid&) = delete;
  ScopedSid& operator=(const ScopedSid&) = delete;

  ScopedSid(ScopedSid&& other) noexcept : sid_(other.sid_) {
    other.sid_ = nullptr;
  }

  ScopedSid& operator=(ScopedSid&& other) noexcept {
    if (this != &other) {
      if (sid_) {
        ::FreeSid(sid_);
      }
      sid_ = other.sid_;
      other.sid_ = nullptr;
    }
    return *this;
  }

  [[nodiscard]] PSID get() const { return sid_; }
  explicit operator bool() const { return sid_ != nullptr; }

 private:
  PSID sid_;
};

// Checks if current process has Administrator token elevation
bool IsAdminElevated();

// Formats Win32 GetLastError() code into human-readable string
std::wstring GetLastErrorMessage(DWORD error_code = ::GetLastError());

// Sanitizes and validates absolute path against path traversal vulnerability
std::wstring SanitizePath(const std::wstring& raw_path);

// Applies NTFS Deny ACL rule (Write, Delete, Create Files/Folders) using Well-Known SID (WinWorldSid / S-1-1-0)
bool SetFolderAccessDeny(const std::wstring& path);

// Removes NTFS Deny ACL rule and restores permissions
bool RemoveFolderAccessDeny(const std::wstring& path);

// Helper methods
bool PathExists(const std::wstring& path);
bool IsDirectory(const std::wstring& path);

}  // namespace update_lock::security

#endif  // UPDATE_LOCK_SECURITY_MANAGER_H_
