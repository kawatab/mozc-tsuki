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

#ifndef MOZC_BASE_WIN_UTIL_H_
#define MOZC_BASE_WIN_UTIL_H_

#if defined(OS_WIN)
#include <windows.h>

#include <string>

#include "base/port.h"
#include "testing/base/public/gunit_prod.h"
// for FRIEND_TEST()

namespace mozc {
class WinUtil {
 public:
  // Load a DLL which has the specified base-name and is located in the
  // system directory.
  // If the function succeeds, the return value is a handle to the module.
  // You should call FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE LoadSystemLibrary(const std::wstring &base_filename);

  // Load a DLL which has the specified base-name and is located in the
  // Mozc server directory.
  // If the function succeeds, the return value is a handle to the module.
  // You should call FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE LoadMozcLibrary(const std::wstring &base_filename);

  // If a DLL which has the specified base-name and located in the system
  // directory is loaded in the caller process, retrieve its module handle.
  // If the function succeeds, the return value is a handle to the module
  // without incrementing its reference count so that you should not call
  // FreeLibrary with the handle.
  // If the function fails, the return value is NULL.
  static HMODULE GetSystemModuleHandle(const std::wstring &base_filename);

  // A variant ot GetSystemModuleHandle except that this method increments
  // reference count of the target DLL.
  static HMODULE GetSystemModuleHandleAndIncrementRefCount(
      const std::wstring &base_filename);

  // Retrieve whether the calling thread hold loader lock or not.
  // Return true if the state is retrieved successfully.
  // Otherwise, the state of loader lock is unknown.
  // NOTE: |lock_held| may be false if the DLL is loaded as
  // implicit link.
  static bool IsDLLSynchronizationHeld(bool *lock_held);

  // Encapsulates the process of converting HWND into a fixed-size integer.
  static uint32 EncodeWindowHandle(HWND window_handle);
  static HWND DecodeWindowHandle(uint32 window_handle_value);

  // Compares |lhs| with |rhs| by CompareStringOrdinal API and returns the
  // result.  If |ignore_case| is true, this function uses system upper-case
  // table for case-insensitive equality like Win32 path names or registry
  // names.
  // Although this function ignores the rest part of given string when NUL
  // character is found, you should not pass such a string in principle.
  static bool SystemEqualString(
      const std::wstring &lhs, const std::wstring &rhs, bool ignore_case);

  // Returns true if succeeds to determine whether the current process has
  // a process token which seems to be one for service process.  Otherwise,
  // returns false.
  static bool IsServiceProcess(bool *is_service);

  // Returns true if succeeds to determine whether the current process has
  // a thread token which seems to be one for service thread.  Otherwise,
  // returns false.
  static bool IsServiceThread(bool *is_service);

  // This is a utility function to check IsServiceProcess and IsServiceThread
  // for the current process and thread.
  static bool IsServiceAccount(bool *is_service);

  // Returns true if succeeds to determine whether the |hToken| is one of the
  // known service or not.  Otherwise, returns false.
  static bool IsServiceUser(HANDLE hToken, bool *is_service);

  // Returns true if succeeds to determine whether the process specified by
  // |process_handle| is in immersive mode or not. Otherwise, returns false.
  static bool IsProcessImmersive(HANDLE process_handle, bool *is_immersive);

  // Returns true if succeeds to determine whether the process specified by
  // |process_handle| is running with RestrictedToken or not. Otherwise,
  // returns false.
  static bool IsProcessRestricted(HANDLE process_handle, bool *is_restricted);

  // Returns true if succeeds to determine whether the process specified by
  // |process_handle| is running under AppContainer sandbox environment or not.
  // Otherwise, returns false.
  static bool IsProcessInAppContainer(HANDLE process_handle,
                                      bool *in_appcontainer);

  // Returns true if |info| is filled with a valid file information that
  // describes |path|. |path| can be a directory or a file.
  static bool GetFileSystemInfoFromPath(const std::wstring &path,
                                        BY_HANDLE_FILE_INFORMATION *info);

  // Returns true if |left_path| and |right_path| are the same file system
  // object. This method takes hard-link into consideration.
  // Returns false if either |left_path| or |right_path| does not exist even
  // when |left_path| == |right_path|.
  static bool AreEqualFileSystemObject(const std::wstring &left_path,
                                       const std::wstring &right_path);

  // Returns true if the file or directory specified by |dos_path| exists and
  // its NT path is retrieved as |nt_path|. This function can work only on
  // Vista and later.
  static bool GetNtPath(const std::wstring &dos_path, std::wstring *nt_path);

  // Returns true if the process specified by |pid| exists and its *initial*
  // NT path is retrieved as |nt_path|. Note that even when the process path is
  // renamed after the process is launched, the *initial* path is retrieved.
  // This is important when MSI changes paths of executables.
  static bool GetProcessInitialNtPath(DWORD pid, std::wstring *nt_path);

  // Returns true if input settings is shared among applications on Windows 8.
  // Returns false otherwise.
  // http://msdn.microsoft.com/en-us/library/windows/desktop/hh994466.aspx
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ms724947.aspx
  static bool IsPerUserInputSettingsEnabled();

  // Returns true if the current process is restricted or in AppContainer.
  static bool IsProcessSandboxed();

  // Execute ShellExecute API with given parameters on the system directory,
  // which is expected to be more appropriate than tha directory where the
  // executable exist, because installer can rename the executable to another
  // directory and delete the application directory.
  static bool ShellExecuteInSystemDir(
      const wchar_t *verb,
      const wchar_t *file,
      const wchar_t *parameters);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WinUtil);
};

// Initializes COM in the constructor (STA), and uninitializes COM in the
// destructor.
class ScopedCOMInitializer {
 public:
  ScopedCOMInitializer();
  ScopedCOMInitializer::~ScopedCOMInitializer();

  // Returns the error code from CoInitialize(NULL)
  // (called in constructor)
  inline HRESULT error_code() const {
    return hr_;
  }

 protected:
  HRESULT hr_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedCOMInitializer);
};

}  // namespace mozc

#endif  // OS_WIN
#endif  // MOZC_BASE_WIN_UTIL_H_
