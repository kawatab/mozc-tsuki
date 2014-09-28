// Copyright 2010-2014, Google Inc.
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

#ifndef MOZC_BASE_WIN_SANDBOX_H_
#define MOZC_BASE_WIN_SANDBOX_H_

// skip all unless OS_WIN
#ifdef OS_WIN

#include <Windows.h>
#include <AccCtrl.h>

#include <string>
#include <vector>

#include "base/port.h"
#include "base/scoped_handle.h"

namespace mozc {
class Sid {
 public:
  explicit Sid(const SID *sid);
  explicit Sid(WELL_KNOWN_SID_TYPE type);
  const SID *GetPSID() const;
  SID *GetPSID();
  wstring GetName() const;
  wstring GetAccountName() const;

 private:
  BYTE sid_[SECURITY_MAX_SID_SIZE];
};

class WinSandbox {
 public:
  // This emum is not compatible with the same name enum in Chromium sandbox
  // library.  This num has INTEGRITY_LEVEL_MEDIUM_PLUS and lacks of
  // INTEGRITY_LEVEL_MEDIUM_LOW and INTEGRITY_LEVEL_BELOW_LOW, which are not
  // listed the predefined SID page on Microsoft KB page.
  // http://msdn.microsoft.com/en-us/library/cc980032.aspx
  // http://support.microsoft.com/kb/243330
  // c.f.) http://src.chromium.org/viewvc/chrome/trunk/src/sandbox/src/security_level.h?view=markup
  enum IntegrityLevel {
    INTEGRITY_LEVEL_SYSTEM,
    INTEGRITY_LEVEL_HIGH,
    INTEGRITY_LEVEL_MEDIUM_PLUS,
    INTEGRITY_LEVEL_MEDIUM,
    INTEGRITY_LEVEL_LOW,
    INTEGRITY_LEVEL_UNTRUSTED,
    INTEGRITY_LEVEL_LAST,
  };

  // Clone of sandbox library's constants.
  // http://src.chromium.org/viewvc/chrome/trunk/src/sandbox/src/security_level.h?view=markup
  enum TokenLevel {
    USER_LOCKDOWN = 0,
    USER_RESTRICTED,
    USER_LIMITED,
    USER_INTERACTIVE,
    USER_NON_ADMIN,
    USER_RESTRICTED_SAME_ACCESS,
    USER_UNPROTECTED,
  };

  // Make a security attributes that only permit current user and system
  // to access the target resource.
  // return true if a valid securityAttributes is generated.
  // Please call ::LocalFree() to release the attributes.
  //
  // Usage:
  // SECURITY_ATTRIBUTES security_attributes;
  // if (!MakeSecurityAttributes(WinSandbox::kSharablePipe,
  //                             &security_attributes)) {
  //  LOG(ERROR) << "Cannot make SecurityAttributes";
  //  return;
  // }
  // handle_ = ::CreateNamedPipe(..
  //                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
  //                             ...
  //                             0,
  //                             &security_attributes);
  // ::LocalFree(security_attributes.lpSecurityDescriptor);
  enum ObjectSecurityType {
    // Used for an object that is inaccessible from lower sandbox level.
    kPrivateObject = 0,
    // Used for a namedpipe object that is accessible from lower sandbox level.
    kSharablePipe,
    // Used for a namedpipe object that is accessible from lower sandbox level
    // including processes with restricted tokens.
    kLooseSharablePipe,
    // Used for an event object that is accessible from lower sandbox level.
    kSharableEvent,
    // Used for a mutex object that is accessible from lower sandbox level.
    kSharableMutex,
    // Used for a file object that can be read from lower sandbox level.
    kSharableFileForRead,
    // Used for an IPC process object that is queriable from lower sandbox
    // level.
    kIPCServerProcess,
  };
  static bool MakeSecurityAttributes(ObjectSecurityType shareble_object_type,
                                     SECURITY_ATTRIBUTES *security_descriptor);

  // Adds an ACE represented by |known_sid| and |access| to the dacl of the
  // kernel object referenced by |object|. |inheritance_flag| is a set of bit
  // flags that determines whether other containers or objects can inherit the
  // ACE from the primary object to which the ACL is attached.
  // This method is basically compatible with the same name function in the
  // Chromium sandbox library except for |inheritance_flag|.
  static bool AddKnownSidToKernelObject(HANDLE object, const SID *known_sid,
                                        DWORD inheritance_flag,
                                        ACCESS_MASK access_mask);

  struct SecurityInfo {
    SecurityInfo();
    TokenLevel primary_level;
    TokenLevel impersonation_level;
    IntegrityLevel integrity_level;
    uint32 creation_flags;
    bool use_locked_down_job;
    bool allow_ui_operation;
    bool in_system_dir;
  };

  // Spawn a process specified by path as the specified integriy level and job
  // level.
  // Return true if process is successfully launched.
  // if pid is specified, pid of child process is set.
  static bool SpawnSandboxedProcess(const string &path,
                                    const string &arg,
                                    const SecurityInfo &info,
                                    DWORD *pid);

  // Following three methods returns corresponding list of SID or LUID for
  // CreateRestrictedToken API, depending on given |effective_token| and
  // |security_level|.  These methods emulates CreateRestrictedToken
  // method in the Chromium sandbox library.
  // http://src.chromium.org/viewvc/chrome/trunk/src/sandbox/src/restricted_token_utils.cc?view=markup
  static vector<Sid> GetSidsToDisable(HANDLE effective_token,
                                      TokenLevel security_level);
  static vector<LUID> GetPrivilegesToDisable(HANDLE effective_token,
                                             TokenLevel security_level);
  static vector<Sid> GetSidsToRestrict(HANDLE effective_token,
                                       TokenLevel security_level);

  // Returns true if a restricted token handle is successfully assigned into
  // |restricted_token|.
  static bool GetRestrictedTokenHandle(
      HANDLE original_token,
      TokenLevel security_level,
      IntegrityLevel integrity_level,
      ScopedHandle* restricted_token);

  // Returns true if a restricted token handle for impersonation is
  // successfully assigned into |restricted_token|.
  static bool GetRestrictedTokenHandleForImpersonation(
      HANDLE original_token,
      TokenLevel security_level,
      IntegrityLevel integrity_level,
      ScopedHandle* restricted_token);

 protected:
  // Returns SDDL for given |shareble_object_type|.
  // This method is placed here for unit testing.
  static wstring GetSDDL(ObjectSecurityType shareble_object_type,
                         const wstring &token_user_sid,
                         const wstring &token_primary_group_sid,
                         bool is_windows_vista_or_later,
                         bool is_windows_8_or_later);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WinSandbox);
};

}  // namespace mozc
#endif  // OS_WIN
#endif  // MOZC_BASE_WIN_SANDBOX_H_
