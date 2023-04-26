// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/system_util.h"

#include <cstdint>

#include "absl/status/status.h"

#ifdef OS_WIN
// clang-format off
#include <Windows.h>
#include <LMCons.h>
#include <Sddl.h>
#include <ShlObj.h>
#include <VersionHelpers.h>
// clang-format on
#else  // OS_WIN
#include <pwd.h>
#include <sys/mman.h>
#include <unistd.h>
#endif  // OS_WIN

#ifdef __APPLE__
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <cerrno>
#endif  // __APPLE__

#include <cstdlib>
#include <cstring>

#ifdef OS_WIN
#include <memory>
#endif  // OS_WIN

#include <sstream>
#include <string>

#ifdef OS_ANDROID
#include "base/android_util.h"
#endif  // OS_ANDROID

#include "base/const.h"
#include "base/environ.h"
#include "base/file_util.h"
#include "base/logging.h"

#ifdef __APPLE__
#include "base/mac_util.h"
#endif  // __APPLE__

#include "base/singleton.h"
#include "base/util.h"

#ifdef OS_WIN
#include "base/win_util.h"
#endif  // OS_WIN
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace {

class UserProfileDirectoryImpl final {
 public:
  UserProfileDirectoryImpl() = default;
  ~UserProfileDirectoryImpl() = default;

  std::string GetDir();
  void SetDir(const std::string &dir);

 private:
  std::string GetUserProfileDirectory() const;

  std::string dir_;
  absl::Mutex mutex_;
};

std::string UserProfileDirectoryImpl::GetDir() {
  absl::MutexLock l(&mutex_);
  if (!dir_.empty()) {
    return dir_;
  }
  const std::string dir = GetUserProfileDirectory();
  if (absl::Status s = FileUtil::CreateDirectory(dir);
      !s.ok() && !absl::IsAlreadyExists(s)) {
    LOG(ERROR) << "Failed to create directory: " << dir << ": " << s;
  }
  if (absl::Status s = FileUtil::DirectoryExists(dir); !s.ok()) {
    LOG(ERROR) << "User profile directory doesn't exist: " << dir << ": " << s;
  }

  dir_ = dir;
  return dir_;
}

void UserProfileDirectoryImpl::SetDir(const std::string &dir) {
  absl::MutexLock l(&mutex_);
  dir_ = dir;
}

#ifdef OS_WIN
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class LocalAppDataDirectoryCache {
 public:
  LocalAppDataDirectoryCache() : result_(E_FAIL) {
    result_ = SafeTryGetLocalAppData(&path_);
  }
  HRESULT result() const { return result_; }
  const bool succeeded() const { return SUCCEEDED(result_); }
  const std::string &path() const { return path_; }

 private:
  // b/5707813 implies that TryGetLocalAppData causes an exception and makes
  // Singleton<LocalAppDataDirectoryCache> invalid state which results in an
  // infinite spin loop in call_once. To prevent this, the constructor of
  // LocalAppDataDirectoryCache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryGetLocalAppData.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryGetLocalAppData(std::string *dir) {
    __try {
      return TryGetLocalAppData(dir);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryGetLocalAppData(std::string *dir) {
    if (dir == nullptr) {
      return E_FAIL;
    }
    dir->clear();

    bool in_app_container = false;
    if (!WinUtil::IsProcessInAppContainer(::GetCurrentProcess(),
                                          &in_app_container)) {
      return E_FAIL;
    }
    if (in_app_container) {
      return TryGetLocalAppDataForAppContainer(dir);
    }
    return TryGetLocalAppDataLow(dir);
  }

  static HRESULT TryGetLocalAppDataForAppContainer(std::string *dir) {
    // User profiles for processes running under AppContainer seem to be as
    // follows, while the scheme is not officially documented.
    //   "%LOCALAPPDATA%\Packages\<package sid>\..."
    // Note: You can also obtain this path by GetAppContainerFolderPath API.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/hh448543.aspx
    // Anyway, here we use heuristics to obtain "LocalLow" folder path.
    // TODO(yukawa): Establish more reliable way to obtain the path.
    wchar_t config[MAX_PATH] = {};
    const HRESULT result = ::SHGetFolderPathW(
        nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, &config[0]);
    if (FAILED(result)) {
      return result;
    }
    std::wstring path = config;
    const std::wstring::size_type local_pos = path.find(L"\\Packages\\");
    if (local_pos == std::wstring::npos) {
      return E_FAIL;
    }
    path.erase(local_pos);
    path += L"Low";
    if (Util::WideToUtf8(path, dir) == 0) {
      return E_FAIL;
    }
    return S_OK;
  }

  static HRESULT TryGetLocalAppDataLow(std::string *dir) {
    if (dir == nullptr) {
      return E_FAIL;
    }
    dir->clear();

    wchar_t *task_mem_buffer = nullptr;
    const HRESULT result = ::SHGetKnownFolderPath(FOLDERID_LocalAppDataLow, 0,
                                                  nullptr, &task_mem_buffer);
    if (FAILED(result)) {
      if (task_mem_buffer != nullptr) {
        ::CoTaskMemFree(task_mem_buffer);
      }
      return result;
    }

    if (task_mem_buffer == nullptr) {
      return E_UNEXPECTED;
    }

    std::wstring wpath = task_mem_buffer;
    ::CoTaskMemFree(task_mem_buffer);

    std::string path;
    if (Util::WideToUtf8(wpath, &path) == 0) {
      return E_UNEXPECTED;
    }

    *dir = path;
    return S_OK;
  }

  HRESULT result_;
  std::string path_;
};
#endif  // OS_WIN

std::string UserProfileDirectoryImpl::GetUserProfileDirectory() const {
#if defined(OS_CHROMEOS)
  // TODO(toka): Must use passed in user profile dir which passed in. If mojo
  // platform the user profile is determined on runtime.
  // It's hack, the user profile dir should be passed in. Although the value in
  // NaCL platform is correct.
  return "/mutable";

#elif defined(OS_WASM)
  // Do nothing for WebAssembly.
  return "";

#elif defined(OS_ANDROID)
  // For android, we do nothing here because user profile directory,
  // of which the path depends on active user,
  // is injected from Java layer.
  return "";

#elif defined(OS_IOS)
  // OS_IOS block must be placed before __APPLE__ because both macros are
  // currently defined on iOS.
  //
  // On iOS, use Caches directory instead of Application Spport directory
  // because the support directory doesn't exist by default.  Also, it is backed
  // up by iTunes and iCloud.
  return FileUtil::JoinPath({MacUtil::GetCachesDirectory(), kProductPrefix});

#elif defined(OS_WIN)
  DCHECK(SUCCEEDED(Singleton<LocalAppDataDirectoryCache>::get()->result()));
  std::string dir = Singleton<LocalAppDataDirectoryCache>::get()->path();

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, kCompanyNameInEnglish);
  if (absl::Status s = FileUtil::CreateDirectory(dir); !s.ok()) {
    LOG(ERROR) << s;
  }
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  return FileUtil::JoinPath(dir, kProductNameInEnglish);

#elif defined(__APPLE__)
  std::string dir = MacUtil::GetApplicationSupportDirectory();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, "Google");
  // The permission of ~/Library/Application Support/Google seems to be 0755.
  // TODO(komatsu): nice to make a wrapper function.
  ::mkdir(dir.c_str(), 0755);
  return FileUtil::JoinPath(dir, "JapaneseInput");
#else   //  GOOGLE_JAPANESE_INPUT_BUILD
  return FileUtil::JoinPath(dir, "Mozc");
#endif  //  GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(OS_LINUX)
  // 1. If "$HOME/.mozc" already exists,
  //    use "$HOME/.mozc" for backward compatibility.
  // 2. If $XDG_CONFIG_HOME is defined
  //    use "$XDG_CONFIG_HOME/mozc".
  // 3. Otherwise
  //    use "$HOME/.config/mozc" as the default value of $XDG_CONFIG_HOME
  // https://specifications.freedesktop.org/basedir-spec/basedir-spec-latest.html
  const char *home = Environ::GetEnv("HOME");
  if (home == nullptr) {
    char buf[1024];
    struct passwd pw, *ppw;
    const uid_t uid = geteuid();
    CHECK_EQ(0, getpwuid_r(uid, &pw, buf, sizeof(buf), &ppw))
        << "Can't get passwd entry for uid " << uid << ".";
    CHECK_LT(0, strlen(pw.pw_dir))
        << "Home directory for uid " << uid << " is not set.";
    return FileUtil::JoinPath(pw.pw_dir, ".mozc");
  }

  const std::string old_dir = FileUtil::JoinPath(home, ".mozc");
  if (FileUtil::DirectoryExists(old_dir).ok()) {
    return old_dir;
  }

  const char *xdg_config_home = Environ::GetEnv("XDG_CONFIG_HOME");
  if (xdg_config_home) {
    return FileUtil::JoinPath(xdg_config_home, "mozc");
  }
  return FileUtil::JoinPath(home, ".config/mozc");

#else  // Supported platforms
#error Undefined target platform.

#endif  // Platforms
}

}  // namespace

std::string SystemUtil::GetUserProfileDirectory() {
  return Singleton<UserProfileDirectoryImpl>::get()->GetDir();
}

std::string SystemUtil::GetLoggingDirectory() {
#ifdef __APPLE__
  std::string dir = MacUtil::GetLoggingDirectory();
  if (absl::Status s = FileUtil::CreateDirectory(dir); !s.ok()) {
    LOG(ERROR) << s;
  }
  return dir;
#else   // __APPLE__
  return GetUserProfileDirectory();
#endif  // __APPLE__
}

void SystemUtil::SetUserProfileDirectory(const std::string &path) {
  Singleton<UserProfileDirectoryImpl>::get()->SetDir(path);
}

#ifdef OS_WIN
namespace {
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class ProgramFilesX86Cache {
 public:
  ProgramFilesX86Cache() : result_(E_FAIL) {
    result_ = SafeTryProgramFilesPath(&path_);
  }
  const bool succeeded() const { return SUCCEEDED(result_); }
  const HRESULT result() const { return result_; }
  const std::string &path() const { return path_; }

 private:
  // b/5707813 implies that the Shell API causes an exception in some cases.
  // In order to avoid potential infinite loops in call_once. the constructor
  // of ProgramFilesX86Cache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryProgramFilesPath.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow)
      SafeTryProgramFilesPath(std::string *path) {
    __try {
      return TryProgramFilesPath(path);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryProgramFilesPath(std::string *path) {
    if (path == nullptr) {
      return E_FAIL;
    }
    path->clear();

    wchar_t program_files_path_buffer[MAX_PATH] = {};
#if defined(_M_X64)
    // In 64-bit processes (such as Text Input Prosessor DLL for 64-bit apps),
    // CSIDL_PROGRAM_FILES points 64-bit Program Files directory. In this case,
    // we should use CSIDL_PROGRAM_FILESX86 to find server, renderer, and other
    // binaries' path.
    const HRESULT result =
        ::SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr,
                           SHGFP_TYPE_CURRENT, program_files_path_buffer);
#elif defined(_M_IX86)
    // In 32-bit processes (such as server, renderer, and other binaries),
    // CSIDL_PROGRAM_FILES always points 32-bit Program Files directory
    // even if they are running in 64-bit Windows.
    const HRESULT result =
        ::SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr,
                           SHGFP_TYPE_CURRENT, program_files_path_buffer);
#else  // !_M_X64 && !_M_IX86
#error "Unsupported CPU architecture"
#endif  // _M_X64, _M_IX86, and others
    if (FAILED(result)) {
      return result;
    }

    std::string program_files;
    if (Util::WideToUtf8(program_files_path_buffer, &program_files) == 0) {
      return E_FAIL;
    }
    *path = program_files;
    return S_OK;
  }
  HRESULT result_;
  std::string path_;
};
}  // namespace
#endif  // OS_WIN

std::string SystemUtil::GetServerDirectory() {
#ifdef OS_WIN
  DCHECK(SUCCEEDED(Singleton<ProgramFilesX86Cache>::get()->result()));
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return FileUtil::JoinPath(
      FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                         kCompanyNameInEnglish),
      kProductNameInEnglish);
#else   // GOOGLE_JAPANESE_INPUT_BUILD
  return FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                            kProductNameInEnglish);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
#endif  // OS_WIN

#if defined(__APPLE__)
  return MacUtil::GetServerDirectory();
#endif  // __APPLE__

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_WASM)
#ifndef MOZC_SERVER_DIR
#define MOZC_SERVER_DIR "/usr/lib/mozc"
#endif  // MOZC_SERVER_DIR
  return MOZC_SERVER_DIR;
#endif  // OS_LINUX || OS_ANDROID || OS_WASM

  // If none of the above platforms is specified, the compiler raises an error
  // because of no return value.
}

std::string SystemUtil::GetServerPath() {
  const std::string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcServerName);
}

std::string SystemUtil::GetRendererPath() {
  const std::string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcRenderer);
}

std::string SystemUtil::GetToolPath() {
  const std::string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcTool);
}

std::string SystemUtil::GetDocumentDirectory() {
#if defined(OS_LINUX)

#ifndef MOZC_DOCUMENT_DIR
#define MOZC_DOCUMENT_DIR "/usr/lib/mozc/documents"
#endif  // MOZC_DOCUMENT_DIR
  return MOZC_DOCUMENT_DIR;

#elif defined(__APPLE__)
  return GetServerDirectory();
#else   // OS_LINUX, __APPLE__
  return FileUtil::JoinPath(GetServerDirectory(), "documents");
#endif  // OS_LINUX, __APPLE__
}

std::string SystemUtil::GetCrashReportDirectory() {
  constexpr char kCrashReportDirectory[] = "CrashReports";
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kCrashReportDirectory);
}

std::string SystemUtil::GetUserNameAsString() {
#if defined(OS_WIN)
  wchar_t wusername[UNLEN + 1];
  DWORD name_size = UNLEN + 1;
  // Call the same name Windows API.  (include Advapi32.lib).
  // TODO(komatsu, yukawa): Add error handling.
  // TODO(komatsu, yukawa): Consider the case where the current thread is
  //   or will be impersonated.
  const BOOL result = ::GetUserName(wusername, &name_size);
  DCHECK_NE(FALSE, result);
  std::string username;
  Util::WideToUtf8(&wusername[0], &username);
  return username;
#endif  // OS_WIN

#if defined(OS_ANDROID)
  // Android doesn't seem to support getpwuid_r.
  struct passwd *ppw = getpwuid(geteuid());
  CHECK(ppw != nullptr);
  return ppw->pw_name;
#endif  // OS_ANDROID

#if defined(__APPLE__) || defined(OS_LINUX) || defined(OS_WASM)
  struct passwd pw, *ppw;
  char buf[1024];
  CHECK_EQ(0, getpwuid_r(geteuid(), &pw, buf, sizeof(buf), &ppw));
  return pw.pw_name;
#endif  // __APPLE__ || OS_LINUX || OS_WASM

  // If none of the above platforms is specified, the compiler raises an error
  // because of no return value.
}

#ifdef OS_WIN
namespace {

class UserSidImpl {
 public:
  UserSidImpl();
  const std::string &get() { return sid_; }

 private:
  std::string sid_;
};

UserSidImpl::UserSidImpl() {
  HANDLE htoken = nullptr;
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &htoken)) {
    sid_ = SystemUtil::GetUserNameAsString();
    LOG(ERROR) << "OpenProcessToken failed: " << ::GetLastError();
    return;
  }

  DWORD length = 0;
  ::GetTokenInformation(htoken, TokenUser, nullptr, 0, &length);
  std::unique_ptr<char[]> buf(new char[length]);
  PTOKEN_USER p_user = reinterpret_cast<PTOKEN_USER>(buf.get());

  if (length == 0 ||
      !::GetTokenInformation(htoken, TokenUser, p_user, length, &length)) {
    ::CloseHandle(htoken);
    sid_ = SystemUtil::GetUserNameAsString();
    LOG(ERROR) << "OpenTokenInformation failed: " << ::GetLastError();
    return;
  }

  LPTSTR p_sid_user_name;
  if (!::ConvertSidToStringSidW(p_user->User.Sid, &p_sid_user_name)) {
    ::CloseHandle(htoken);
    sid_ = SystemUtil::GetUserNameAsString();
    LOG(ERROR) << "ConvertSidToStringSidW failed: " << ::GetLastError();
    return;
  }

  Util::WideToUtf8(p_sid_user_name, &sid_);

  ::LocalFree(p_sid_user_name);
  ::CloseHandle(htoken);
}

}  // namespace
#endif  // OS_WIN

std::string SystemUtil::GetUserSidAsString() {
#ifdef OS_WIN
  return Singleton<UserSidImpl>::get()->get();
#else   // OS_WIN
  return GetUserNameAsString();
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {

std::string GetObjectNameAsString(HANDLE handle) {
  if (handle == nullptr) {
    LOG(ERROR) << "Unknown handle";
    return "";
  }

  DWORD size = 0;
  if (::GetUserObjectInformationA(handle, UOI_NAME, nullptr, 0, &size) ||
      ERROR_INSUFFICIENT_BUFFER != ::GetLastError()) {
    LOG(ERROR) << "GetUserObjectInformationA() failed: " << ::GetLastError();
    return "";
  }

  if (size == 0) {
    LOG(ERROR) << "buffer size is 0";
    return "";
  }

  std::unique_ptr<char[]> buf(new char[size]);
  DWORD return_size = 0;
  if (!::GetUserObjectInformationA(handle, UOI_NAME, buf.get(), size,
                                   &return_size)) {
    LOG(ERROR) << "::GetUserObjectInformationA() failed: " << ::GetLastError();
    return "";
  }

  if (return_size <= 1) {
    LOG(ERROR) << "result buffer size is too small";
    return "";
  }

  char *result = buf.get();
  result[return_size - 1] = '\0';  // just make sure nullptr terminated

  return result;
}

bool GetCurrentSessionId(uint32 *session_id) {
  DCHECK(session_id);
  *session_id = 0;
  DWORD id = 0;
  if (!::ProcessIdToSessionId(::GetCurrentProcessId(), &id)) {
    LOG(ERROR) << "cannot get session id: " << ::GetLastError();
    return false;
  }
  static_assert(sizeof(DWORD) == sizeof(uint32),
                "DWORD and uint32 must be equivalent");
  *session_id = static_cast<uint32>(id);
  return true;
}

// Here we use the input desktop instead of the desktop associated with the
// current thread. One reason behind this is that some applications such as
// Adobe Reader XI use multiple desktops in a process. Basically the input
// desktop is most appropriate and important desktop for our use case.
// See
// http://blogs.adobe.com/asset/2012/10/new-security-capabilities-in-adobe-reader-and-acrobat-xi-now-available.html
std::string GetInputDesktopName() {
  const HDESK desktop_handle =
      ::OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
  if (desktop_handle == nullptr) {
    return "";
  }
  const std::string desktop_name = GetObjectNameAsString(desktop_handle);
  ::CloseDesktop(desktop_handle);
  return desktop_name;
}

std::string GetProcessWindowStationName() {
  // We must not close the returned value of GetProcessWindowStation().
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683225.aspx
  const HWINSTA window_station = ::GetProcessWindowStation();
  if (window_station == nullptr) {
    return "";
  }

  return GetObjectNameAsString(window_station);
}

std::string GetSessionIdString() {
  uint32 session_id = 0;
  if (!GetCurrentSessionId(&session_id)) {
    return "";
  }
  return std::to_string(session_id);
}

}  // namespace
#endif  // OS_WIN

std::string SystemUtil::GetDesktopNameAsString() {
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_WASM)
  const char *display = Environ::GetEnv("DISPLAY");
  if (display == nullptr) {
    return "";
  }
  return display;
#endif  // OS_LINUX || OS_ANDROID || OS_WASM

#if defined(__APPLE__)
  return "";
#endif  // __APPLE__

#if defined(OS_WIN)
  const std::string &session_id = GetSessionIdString();
  if (session_id.empty()) {
    DLOG(ERROR) << "Failed to retrieve session id";
    return "";
  }

  const std::string &window_station_name = GetProcessWindowStationName();
  if (window_station_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve window station name";
    return "";
  }

  const std::string &desktop_name = GetInputDesktopName();
  if (desktop_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve desktop name";
    return "";
  }

  return (session_id + "." + window_station_name + "." + desktop_name);
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {

// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class SystemDirectoryCache {
 public:
  SystemDirectoryCache() : system_dir_(nullptr) {
    const UINT copied_len_wo_null_if_success =
        ::GetSystemDirectory(path_buffer_, std::size(path_buffer_));
    if (copied_len_wo_null_if_success >= std::size(path_buffer_)) {
      // Function failed.
      return;
    }
    DCHECK_EQ(L'\0', path_buffer_[copied_len_wo_null_if_success]);
    system_dir_ = path_buffer_;
  }
  const bool succeeded() const { return system_dir_ != nullptr; }
  const wchar_t *system_dir() const { return system_dir_; }

 private:
  wchar_t path_buffer_[MAX_PATH];
  wchar_t *system_dir_;
};

}  // namespace

// TODO(team): Support other platforms.
bool SystemUtil::EnsureVitalImmutableDataIsAvailable() {
  if (!Singleton<SystemDirectoryCache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<ProgramFilesX86Cache>::get()->succeeded()) {
    return false;
  }
  if (!Singleton<LocalAppDataDirectoryCache>::get()->succeeded()) {
    return false;
  }
  return true;
}
#endif  // OS_WIN

bool SystemUtil::IsWindows7OrLater() {
#ifdef OS_WIN
  static const bool result = ::IsWindows7OrGreater();
  return result;
#else   // OS_WIN
  return false;
#endif  // OS_WIN
}

bool SystemUtil::IsWindows8OrLater() {
#ifdef OS_WIN
  static const bool result = ::IsWindows8OrGreater();
  return result;
#else   // OS_WIN
  return false;
#endif  // OS_WIN
}

bool SystemUtil::IsWindows8_1OrLater() {
#ifdef OS_WIN
  static const bool result = ::IsWindows8Point1OrGreater();
  return result;
#else   // OS_WIN
  return false;
#endif  // OS_WIN
}

namespace {
volatile mozc::SystemUtil::IsWindowsX64Mode g_is_windows_x64_mode =
    mozc::SystemUtil::IS_WINDOWS_X64_DEFAULT_MODE;
}  // namespace

bool SystemUtil::IsWindowsX64() {
  switch (g_is_windows_x64_mode) {
    case IS_WINDOWS_X64_EMULATE_32BIT_MACHINE:
      return false;
    case IS_WINDOWS_X64_EMULATE_64BIT_MACHINE:
      return true;
    case IS_WINDOWS_X64_DEFAULT_MODE:
      // handled below.
      break;
    default:
      // Should never reach here.
      DLOG(FATAL) << "Unexpected mode specified.  mode = "
                  << g_is_windows_x64_mode;
      // handled below.
      break;
  }

#ifdef OS_WIN
  SYSTEM_INFO system_info = {};
  // This function never fails.
  ::GetNativeSystemInfo(&system_info);
  return (system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64);
#else   // OS_WIN
  return false;
#endif  // OS_WIN
}

void SystemUtil::SetIsWindowsX64ModeForTest(IsWindowsX64Mode mode) {
  g_is_windows_x64_mode = mode;
  switch (g_is_windows_x64_mode) {
    case IS_WINDOWS_X64_EMULATE_32BIT_MACHINE:
    case IS_WINDOWS_X64_EMULATE_64BIT_MACHINE:
    case IS_WINDOWS_X64_DEFAULT_MODE:
      // Known mode. OK.
      break;
    default:
      DLOG(FATAL) << "Unexpected mode specified.  mode = "
                  << g_is_windows_x64_mode;
      break;
  }
}

#ifdef OS_WIN
const wchar_t *SystemUtil::GetSystemDir() {
  DCHECK(Singleton<SystemDirectoryCache>::get()->succeeded());
  return Singleton<SystemDirectoryCache>::get()->system_dir();
}

std::string SystemUtil::GetMSCTFAsmCacheReadyEventName() {
  const std::string &session_id = GetSessionIdString();
  if (session_id.empty()) {
    DLOG(ERROR) << "Failed to retrieve session id";
    return "";
  }

  const std::string &desktop_name = GetInputDesktopName();

  if (desktop_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve desktop name";
    return "";
  }

  // Compose "Local\MSCTF.AsmCacheReady.<desktop name><session #>".
  return ("Local\\MSCTF.AsmCacheReady." + desktop_name + session_id);
}
#endif  // OS_WIN

// TODO(toshiyuki): move this to the initialization module and calculate
// version only when initializing.
std::string SystemUtil::GetOSVersionString() {
#ifdef OS_WIN
  std::string ret = "Windows";
  OSVERSIONINFOEX osvi = {0};
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
  if (GetVersionEx(reinterpret_cast<OSVERSIONINFO *>(&osvi))) {
    ret += ".";
    ret += std::to_string(static_cast<uint32>(osvi.dwMajorVersion));
    ret += ".";
    ret += std::to_string(static_cast<uint32>(osvi.dwMinorVersion));
    ret += "." + std::to_string(osvi.wServicePackMajor);
    ret += "." + std::to_string(osvi.wServicePackMinor);
  } else {
    LOG(WARNING) << "GetVersionEx failed";
  }
  return ret;
#elif defined(__APPLE__)
  const std::string ret = "MacOSX " + MacUtil::GetOSVersionString();
  // TODO(toshiyuki): get more specific info
  return ret;
#elif defined(OS_LINUX)
  const std::string ret = "Linux";
  return ret;
#else   // !OS_WIN && !__APPLE__ && !OS_LINUX
  const std::string ret = "Unknown";
  return ret;
#endif  // OS_WIN, __APPLE__, OS_LINUX
}

void SystemUtil::DisableIME() {
#ifdef OS_WIN
  // Note that ImmDisableTextFrameService API is no longer supported on
  // Windows Vista and later.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd318537.aspx
  ::ImmDisableIME(-1);
#endif  // OS_WIN
}

uint64_t SystemUtil::GetTotalPhysicalMemory() {
#if defined(OS_WIN)
  MEMORYSTATUSEX memory_status = {sizeof(MEMORYSTATUSEX)};
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    return 0;
  }
  return memory_status.ullTotalPhys;
#endif  // OS_WIN

#if defined(__APPLE__)
  int mib[] = {CTL_HW, HW_MEMSIZE};
  uint64 total_memory = 0;
  size_t size = sizeof(total_memory);
  const int error =
      sysctl(mib, std::size(mib), &total_memory, &size, nullptr, 0);
  if (error == -1) {
    const int error = errno;
    LOG(ERROR) << "sysctl with hw.memsize failed. "
               << "errno: " << error;
    return 0;
  }
  return total_memory;
#endif  // __APPLE__

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_WASM)
#if defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  const int32_t page_size = sysconf(_SC_PAGESIZE);
  const int32_t number_of_phyisical_pages = sysconf(_SC_PHYS_PAGES);
  if (number_of_phyisical_pages < 0) {
    // likely to be overflowed.
    LOG(FATAL) << number_of_phyisical_pages << ", " << page_size;
    return 0;
  }
  return static_cast<uint64_t>(number_of_phyisical_pages) * page_size;
#else   // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  return 0;
#endif  // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
#endif  // OS_LINUX || OS_ANDROID || OS_WASM

  // If none of the above platforms is specified, the compiler raises an error
  // because of no return value.
}

}  // namespace mozc
