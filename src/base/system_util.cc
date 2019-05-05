// Copyright 2010-2018, Google Inc.
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

#ifdef OS_WIN
#include <Windows.h>
#include <LMCons.h>
#include <Sddl.h>
#include <ShlObj.h>
#include <VersionHelpers.h>
#else  // OS_WIN
#include <pwd.h>
#include <sys/mman.h>
#include <unistd.h>
#ifdef OS_MACOSX
#include <sys/stat.h>
#include <sys/sysctl.h>
#endif  // OS_MACOSX
#endif  // OS_WIN

#ifdef OS_MACOSX
#include <cerrno>
#endif  // OS_MACOSX
#include <cstdlib>
#include <cstring>
#ifdef OS_WIN
#include <memory>
#endif  // OS_WIN
#include <sstream>
#include <string>

#include "base/const.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/singleton.h"
#include "base/util.h"
#include "base/win_util.h"

#ifdef OS_ANDROID
#include "base/android_util.h"
#endif  // OS_ANDROID

#ifdef OS_WIN
using std::unique_ptr;
#endif  // OS_WIN

namespace mozc {

namespace {
class UserProfileDirectoryImpl {
 public:
  UserProfileDirectoryImpl();
  string get() { return dir_; }
  void set(const string &dir) { dir_ = dir; }
 private:
  string dir_;
};

#ifdef OS_WIN
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class LocalAppDataDirectoryCache {
 public:
  LocalAppDataDirectoryCache() : result_(E_FAIL) {
    result_ = SafeTryGetLocalAppData(&path_);
  }
  HRESULT result() const {
    return result_;
  }
  const bool succeeded() const {
    return SUCCEEDED(result_);
  }
  const string &path() const {
    return path_;
  }

 private:
  // b/5707813 implies that TryGetLocalAppData causes an exception and makes
  // Singleton<LocalAppDataDirectoryCache> invalid state which results in an
  // infinite spin loop in CallOnce. To prevent this, the constructor of
  // LocalAppDataDirectoryCache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryGetLocalAppData.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryGetLocalAppData(string *dir) {
    __try {
      return TryGetLocalAppData(dir);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryGetLocalAppData(string *dir) {
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

  static HRESULT TryGetLocalAppDataForAppContainer(string *dir) {
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
    if (Util::WideToUTF8(path, dir) == 0) {
      return E_FAIL;
    }
    return S_OK;
  }

  static HRESULT TryGetLocalAppDataLow(string *dir) {
    if (dir == nullptr) {
      return E_FAIL;
    }
    dir->clear();

    wchar_t *task_mem_buffer = nullptr;
    const HRESULT result = ::SHGetKnownFolderPath(
        FOLDERID_LocalAppDataLow, 0, nullptr, &task_mem_buffer);
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

    string path;
    if (Util::WideToUTF8(wpath, &path) == 0) {
      return E_UNEXPECTED;
    }

    *dir = path;
    return S_OK;
  }

  HRESULT result_;
  string path_;
};
#endif  // OS_WIN

UserProfileDirectoryImpl::UserProfileDirectoryImpl() {
#ifdef MOZC_USE_PEPPER_FILE_IO
  // In NaCl, we can't call FileUtil::CreateDirectory() nor
  // FileUtil::DirectoryExists(). So we just set dir_ here.
  dir_ = "/";
  return;
#else  // MOZC_USE_PEPPER_FILE_IO
  string dir;

#ifdef OS_WIN
  DCHECK(SUCCEEDED(Singleton<LocalAppDataDirectoryCache>::get()->result()));
  dir = Singleton<LocalAppDataDirectoryCache>::get()->path();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, kCompanyNameInEnglish);
  FileUtil::CreateDirectory(dir);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, kProductNameInEnglish);

#elif defined(OS_MACOSX)
  dir = MacUtil::GetApplicationSupportDirectory();
#ifdef GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, "Google");
  // The permission of ~/Library/Application Support/Google seems to be 0755.
  // TODO(komatsu): nice to make a wrapper function.
  ::mkdir(dir.c_str(), 0755);
  dir = FileUtil::JoinPath(dir, "JapaneseInput");
#else  //  GOOGLE_JAPANESE_INPUT_BUILD
  dir = FileUtil::JoinPath(dir, "Mozc");
#endif  //  GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(OS_ANDROID)
  // For android, we do nothing here because user profile directory,
  // of which the path depends on active user,
  // is injected from Java layer.

#else  // !OS_WIN && !OS_MACOSX && !OS_ANDROID
  char buf[1024];
  struct passwd pw, *ppw;
  const uid_t uid = geteuid();
  CHECK_EQ(0, getpwuid_r(uid, &pw, buf, sizeof(buf), &ppw))
      << "Can't get passwd entry for uid " << uid << ".";
  CHECK_LT(0, strlen(pw.pw_dir))
      << "Home directory for uid " << uid << " is not set.";
  dir = FileUtil::JoinPath(pw.pw_dir, ".mozc");
#endif  // !OS_WIN && !OS_MACOSX && !OS_ANDROID

  FileUtil::CreateDirectory(dir);
  if (!FileUtil::DirectoryExists(dir)) {
    LOG(ERROR) << "Failed to create directory: " << dir;
  }

  // set User profile directory
  dir_ = dir;
#endif  // MOZC_USE_PEPPER_FILE_IO
}

}  // namespace

string SystemUtil::GetUserProfileDirectory() {
  return Singleton<UserProfileDirectoryImpl>::get()->get();
}

string SystemUtil::GetLoggingDirectory() {
#ifdef OS_MACOSX
  string dir = MacUtil::GetLoggingDirectory();
  FileUtil::CreateDirectory(dir);
  return dir;
#else  // OS_MACOSX
  return GetUserProfileDirectory();
#endif  // OS_MACOSX
}

void SystemUtil::SetUserProfileDirectory(const string &path) {
  Singleton<UserProfileDirectoryImpl>::get()->set(path);
}

#ifdef OS_WIN
namespace {
// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class ProgramFilesX86Cache {
 public:
  ProgramFilesX86Cache() : result_(E_FAIL) {
    result_ = SafeTryProgramFilesPath(&path_);
  }
  const bool succeeded() const {
    return SUCCEEDED(result_);
  }
  const HRESULT result() const {
    return result_;
  }
  const string &path() const {
    return path_;
  }

 private:
  // b/5707813 implies that the Shell API causes an exception in some cases.
  // In order to avoid potential infinite loops in CallOnce. the constructor of
  // ProgramFilesX86Cache must be exception free.
  // Note that __try and __except does not guarantees that any destruction
  // of internal C++ objects when a non-C++ exception occurs except that
  // /EHa compiler option is specified.
  // Since Mozc uses /EHs option in common.gypi, we must admit potential
  // memory leakes when any non-C++ exception occues in TryProgramFilesPath.
  // See http://msdn.microsoft.com/en-us/library/1deeycx5.aspx
  static HRESULT __declspec(nothrow) SafeTryProgramFilesPath(string *path) {
    __try {
      return TryProgramFilesPath(path);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
      return E_UNEXPECTED;
    }
  }

  static HRESULT TryProgramFilesPath(string *path) {
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
    const HRESULT result = ::SHGetFolderPathW(
        nullptr, CSIDL_PROGRAM_FILESX86, nullptr,
        SHGFP_TYPE_CURRENT, program_files_path_buffer);
#elif defined(_M_IX86)
    // In 32-bit processes (such as server, renderer, and other binaries),
    // CSIDL_PROGRAM_FILES always points 32-bit Program Files directory
    // even if they are running in 64-bit Windows.
    const HRESULT result = ::SHGetFolderPathW(
        nullptr, CSIDL_PROGRAM_FILES, nullptr,
        SHGFP_TYPE_CURRENT, program_files_path_buffer);
#else  // !_M_X64 && !_M_IX86
#error "Unsupported CPU architecture"
#endif  // _M_X64, _M_IX86, and others
    if (FAILED(result)) {
      return result;
    }

    string program_files;
    if (Util::WideToUTF8(program_files_path_buffer, &program_files) == 0) {
      return E_FAIL;
    }
    *path = program_files;
    return S_OK;
  }
  HRESULT result_;
  string path_;
};
}  // namespace
#endif  // OS_WIN

string SystemUtil::GetServerDirectory() {
#ifdef OS_WIN
  DCHECK(SUCCEEDED(Singleton<ProgramFilesX86Cache>::get()->result()));
#if defined(GOOGLE_JAPANESE_INPUT_BUILD)
  return FileUtil::JoinPath(
      FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                         kCompanyNameInEnglish),
      kProductNameInEnglish);
#else  // GOOGLE_JAPANESE_INPUT_BUILD
  return FileUtil::JoinPath(Singleton<ProgramFilesX86Cache>::get()->path(),
                            kProductNameInEnglish);
#endif  // GOOGLE_JAPANESE_INPUT_BUILD

#elif defined(OS_MACOSX)
  return MacUtil::GetServerDirectory();

#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL)
#if defined(MOZC_SERVER_DIRECTORY)
  return MOZC_SERVER_DIRECTORY;
#else
  return "/usr/lib/mozc";
#endif  // MOZC_SERVER_DIRECTORY

#endif  // OS_WIN, OS_MACOSX, OS_LINUX, ...
}

string SystemUtil::GetServerPath() {
  const string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcServerName);
}

string SystemUtil::GetRendererPath() {
  const string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcRenderer);
}

string SystemUtil::GetToolPath() {
  const string server_path = GetServerDirectory();
  // if server path is empty, return empty path
  if (server_path.empty()) {
    return "";
  }
  return FileUtil::JoinPath(server_path, kMozcTool);
}

string SystemUtil::GetDocumentDirectory() {
#if defined(OS_MACOSX)
  return GetServerDirectory();
#elif defined(MOZC_DOCUMENT_DIRECTORY)
  return MOZC_DOCUMENT_DIRECTORY;
#else
  return FileUtil::JoinPath(GetServerDirectory(), "documents");
#endif  // OS_MACOSX
}

string SystemUtil::GetCrashReportDirectory() {
  const char kCrashReportDirectory[] = "CrashReports";
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kCrashReportDirectory);
}

string SystemUtil::GetUserNameAsString() {
#ifdef MOZC_USE_PEPPER_FILE_IO
  LOG(ERROR) << "SystemUtil::GetUserNameAsString() is not implemented in NaCl.";
  return "username";

#elif defined(OS_WIN)  // MOZC_USE_PEPPER_FILE_IO
  wchar_t wusername[UNLEN + 1];
  DWORD name_size = UNLEN + 1;
  // Call the same name Windows API.  (include Advapi32.lib).
  // TODO(komatsu, yukawa): Add error handling.
  // TODO(komatsu, yukawa): Consider the case where the current thread is
  //   or will be impersonated.
  const BOOL result = ::GetUserName(wusername, &name_size);
  DCHECK_NE(FALSE, result);
  string username;
  Util::WideToUTF8(&wusername[0], &username);
  return username;

#elif defined(OS_ANDROID)  // OS_WIN
  // Android doesn't seem to support getpwuid_r.
  struct passwd *ppw = getpwuid(geteuid());
  CHECK(ppw != NULL);
  return ppw->pw_name;

#else  // OS_ANDROID
  // OS_MACOSX, OS_LINUX or OS_NACL
  struct passwd pw, *ppw;
  char buf[1024];
  CHECK_EQ(0, getpwuid_r(geteuid(), &pw, buf, sizeof(buf), &ppw));
  return pw.pw_name;
#endif  // !MOZC_USE_PEPPER_FILE_IO && !OS_WIN && !OS_ANDROID
}

#ifdef OS_WIN
namespace {

class UserSidImpl {
 public:
  UserSidImpl();
  const string &get() { return sid_; }
 private:
  string sid_;
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
  unique_ptr<char[]> buf(new char[length]);
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

  Util::WideToUTF8(p_sid_user_name, &sid_);

  ::LocalFree(p_sid_user_name);
  ::CloseHandle(htoken);
}

}  // namespace
#endif  // OS_WIN

string SystemUtil::GetUserSidAsString() {
#ifdef OS_WIN
  return Singleton<UserSidImpl>::get()->get();
#else  // OS_WIN
  return GetUserNameAsString();
#endif  // OS_WIN
}

#ifdef OS_WIN
namespace {

string GetObjectNameAsString(HANDLE handle) {
  if (handle == nullptr) {
    LOG(ERROR) << "Unknown handle";
    return "";
  }

  DWORD size = 0;
  if (::GetUserObjectInformationA(handle, UOI_NAME, nullptr, 0, &size) ||
      ERROR_INSUFFICIENT_BUFFER != ::GetLastError()) {
    LOG(ERROR) << "GetUserObjectInformationA() failed: "
               << ::GetLastError();
    return "";
  }

  if (size == 0) {
    LOG(ERROR) << "buffer size is 0";
    return "";
  }

  unique_ptr<char[]> buf(new char[size]);
  DWORD return_size = 0;
  if (!::GetUserObjectInformationA(handle, UOI_NAME, buf.get(),
                                   size, &return_size)) {
    LOG(ERROR) << "::GetUserObjectInformationA() failed: "
               << ::GetLastError();
    return "";
  }

  if (return_size <= 1) {
    LOG(ERROR) << "result buffer size is too small";
    return "";
  }

  char *result = buf.get();
  result[return_size - 1] = '\0';  // just make sure NULL terminated

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
// See http://blogs.adobe.com/asset/2012/10/new-security-capabilities-in-adobe-reader-and-acrobat-xi-now-available.html
string GetInputDesktopName() {
  const HDESK desktop_handle =
      ::OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
  if (desktop_handle == nullptr) {
    return "";
  }
  const string desktop_name = GetObjectNameAsString(desktop_handle);
  ::CloseDesktop(desktop_handle);
  return desktop_name;
}

string GetProcessWindowStationName() {
  // We must not close the returned value of GetProcessWindowStation().
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms683225.aspx
  const HWINSTA window_station = ::GetProcessWindowStation();
  if (window_station == nullptr) {
    return "";
  }

  return GetObjectNameAsString(window_station);
}

string GetSessionIdString() {
  uint32 session_id = 0;
  if (!GetCurrentSessionId(&session_id)) {
    return "";
  }
  return std::to_string(session_id);
}

}  // namespace
#endif  // OS_WIN

string SystemUtil::GetDesktopNameAsString() {
#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL)
  const char *display = getenv("DISPLAY");
  if (display == NULL) {
    return "";
  }
  return display;

#elif defined(OS_MACOSX)
  return "";

#elif defined(OS_WIN)
  const string &session_id = GetSessionIdString();
  if (session_id.empty()) {
    DLOG(ERROR) << "Failed to retrieve session id";
    return "";
  }

  const string &window_station_name = GetProcessWindowStationName();
  if (window_station_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve window station name";
    return "";
  }

  const string &desktop_name = GetInputDesktopName();
  if (desktop_name.empty()) {
    DLOG(ERROR) << "Failed to retrieve desktop name";
    return "";
  }

  return (session_id + "." + window_station_name + "." + desktop_name);
#endif  // OS_LINUX, OS_MACOSX, OS_WIN, ...
}

#ifdef OS_WIN
namespace {

// TODO(yukawa): Use API wrapper so that unit test can emulate any case.
class SystemDirectoryCache {
 public:
  SystemDirectoryCache() : system_dir_(nullptr) {
    const UINT copied_len_wo_null_if_success =
        ::GetSystemDirectory(path_buffer_, arraysize(path_buffer_));
    if (copied_len_wo_null_if_success >= arraysize(path_buffer_)) {
      // Function failed.
      return;
    }
    DCHECK_EQ(L'\0', path_buffer_[copied_len_wo_null_if_success]);
    system_dir_ = path_buffer_;
  }
  const bool succeeded() const {
    return system_dir_ != nullptr;
  }
  const wchar_t *system_dir() const {
    return system_dir_;
  }
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
#else
  return false;
#endif  // OS_WIN
}

bool SystemUtil::IsWindows8OrLater() {
#ifdef OS_WIN
  static const bool result = ::IsWindows8OrGreater();
  return result;
#else
  return false;
#endif  // OS_WIN
}

bool SystemUtil::IsWindows8_1OrLater() {
#ifdef OS_WIN
  static const bool result = ::IsWindows8Point1OrGreater();
  return result;
#else
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
  return (system_info.wProcessorArchitecture ==
          PROCESSOR_ARCHITECTURE_AMD64);
#else
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

string SystemUtil::GetMSCTFAsmCacheReadyEventName() {
  const string &session_id = GetSessionIdString();
  if (session_id.empty()) {
    DLOG(ERROR) << "Failed to retrieve session id";
    return "";
  }

  const string &desktop_name = GetInputDesktopName();

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
string SystemUtil::GetOSVersionString() {
#ifdef OS_WIN
  string ret = "Windows";
  OSVERSIONINFOEX osvi = { 0 };
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
#elif defined(OS_MACOSX)
  const string ret = "MacOSX " + MacUtil::GetOSVersionString();
  // TODO(toshiyuki): get more specific info
  return ret;
#elif defined(OS_LINUX) || defined(OS_NACL)
  const string ret = "Linux";
  return ret;
#else  // !OS_WIN && !OS_MACOSX && !OS_LINUX
  const string ret = "Unknown";
  return ret;
#endif  // OS_WIN, OS_MACOSX, OS_LINUX
}

void SystemUtil::DisableIME() {
#ifdef OS_WIN
  // Note that ImmDisableTextFrameService API is no longer supported on
  // Windows Vista and later.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd318537.aspx
  ::ImmDisableIME(-1);
#endif  // OS_WIN
}

uint64 SystemUtil::GetTotalPhysicalMemory() {
#if defined(OS_WIN)
  MEMORYSTATUSEX memory_status = { sizeof(MEMORYSTATUSEX) };
  if (!::GlobalMemoryStatusEx(&memory_status)) {
    return 0;
  }
  return memory_status.ullTotalPhys;
#elif defined(OS_MACOSX)
  int mib[] = { CTL_HW, HW_MEMSIZE };
  uint64 total_memory = 0;
  size_t size = sizeof(total_memory);
  const int error =
      sysctl(mib, arraysize(mib), &total_memory, &size, NULL, 0);
  if (error == -1) {
    const int error = errno;
    LOG(ERROR) << "sysctl with hw.memsize failed. "
               << "errno: " << error;
    return 0;
  }
  return total_memory;
#elif defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_NACL)
#if defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  const long page_size = sysconf(_SC_PAGESIZE);
  const long number_of_phyisical_pages = sysconf(_SC_PHYS_PAGES);
  if (number_of_phyisical_pages < 0) {
    // likely to be overflowed.
    LOG(FATAL) << number_of_phyisical_pages << ", " << page_size;
    return 0;
  }
  return static_cast<uint64>(number_of_phyisical_pages) * page_size;
#else  // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
  return 0;
#endif  // defined(_SC_PAGESIZE) && defined(_SC_PHYS_PAGES)
#else  // !(defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX))
#error "unknown platform"
#endif  // OS_WIN, OS_MACOSX, OS_LINUX
}

}  // namespace mozc
