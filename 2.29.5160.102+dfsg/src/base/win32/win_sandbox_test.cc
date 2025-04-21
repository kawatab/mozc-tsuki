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

#include "base/win32/win_sandbox.h"

#include "base/win32/scoped_handle.h"
#include "testing/googletest.h"
#include "testing/gunit.h"

namespace mozc {
namespace {

class TestableWinSandbox : public WinSandbox {
 public:
  TestableWinSandbox() = delete;
  TestableWinSandbox(const TestableWinSandbox&) = delete;
  TestableWinSandbox& operator=(const TestableWinSandbox&) = delete;

  // Change access rights.
  using WinSandbox::GetSDDL;
};

void VerifySidContained(const std::vector<Sid> sids,
                        WELL_KNOWN_SID_TYPE expected_well_known_sid) {
  Sid expected_sid(expected_well_known_sid);
  for (size_t i = 0; i < sids.size(); ++i) {
    Sid temp_sid = sids[i];
    if (::EqualSid(expected_sid.GetPSID(), temp_sid.GetPSID())) {
      // Found!
      return;
    }
  }
  ADD_FAILURE() << "Not found. Expected SID: " << expected_well_known_sid;
}

TEST(WinSandboxTest, GetSidsToDisable) {
  HANDLE process_token_ret = nullptr;
  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                     &process_token_ret);
  ScopedHandle process_token(process_token_ret);

  const std::vector<Sid> lockdown = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_LOCKDOWN);
  const std::vector<Sid> restricted = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_RESTRICTED);
  const std::vector<Sid> limited = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_LIMITED);
  const std::vector<Sid> interactive = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_INTERACTIVE);
  const std::vector<Sid> non_admin = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_NON_ADMIN);
  const std::vector<Sid> restricted_same_access = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_RESTRICTED_SAME_ACCESS);
  const std::vector<Sid> unprotect = WinSandbox::GetSidsToDisable(
      process_token.get(), WinSandbox::USER_UNPROTECTED);

  EXPECT_TRUE(restricted.size() == lockdown.size());
  VerifySidContained(lockdown, WinBuiltinUsersSid);

  VerifySidContained(limited, WinAuthenticatedUserSid);

  EXPECT_TRUE(non_admin.size() == interactive.size());

  EXPECT_EQ(restricted_same_access.size(), 0);

  EXPECT_EQ(unprotect.size(), 0);
}

TEST(WinSandboxTest, GetPrivilegesToDisable) {
  HANDLE process_token_ret = nullptr;
  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                     &process_token_ret);
  ScopedHandle process_token(process_token_ret);

  const std::vector<LUID> lockdown = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_LOCKDOWN);
  const std::vector<LUID> restricted = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_RESTRICTED);
  const std::vector<LUID> limited = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_LIMITED);
  const std::vector<LUID> interactive = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_INTERACTIVE);
  const std::vector<LUID> non_admin = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_NON_ADMIN);
  const std::vector<LUID> restricted_same_access =
      WinSandbox::GetPrivilegesToDisable(
          process_token.get(), WinSandbox::USER_RESTRICTED_SAME_ACCESS);
  const std::vector<LUID> unprotect = WinSandbox::GetPrivilegesToDisable(
      process_token.get(), WinSandbox::USER_UNPROTECTED);

  EXPECT_EQ(restricted_same_access.size(), 0);
  EXPECT_EQ(unprotect.size(), 0);
}

TEST(WinSandboxTest, GetSidsToRestrict) {
  HANDLE process_token_ret = nullptr;
  ::OpenProcessToken(::GetCurrentProcess(), TOKEN_ALL_ACCESS,
                     &process_token_ret);
  ScopedHandle process_token(process_token_ret);

  const std::vector<Sid> lockdown = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_LOCKDOWN);
  const std::vector<Sid> restricted = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_RESTRICTED);
  const std::vector<Sid> limited = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_LIMITED);
  const std::vector<Sid> interactive = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_INTERACTIVE);
  const std::vector<Sid> non_admin = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_NON_ADMIN);
  const std::vector<Sid> restricted_same_access = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_RESTRICTED_SAME_ACCESS);
  const std::vector<Sid> unprotect = WinSandbox::GetSidsToRestrict(
      process_token.get(), WinSandbox::USER_UNPROTECTED);

  EXPECT_EQ(lockdown.size(), 1);
  VerifySidContained(lockdown, WinNullSid);

  VerifySidContained(limited, WinBuiltinUsersSid);

  VerifySidContained(interactive, WinBuiltinUsersSid);
}

constexpr wchar_t kDummyUserSID[] = L"S-8";
constexpr wchar_t kDummyGroupSID[] = L"S-9";

std::wstring GetSDDL(WinSandbox::ObjectSecurityType type) {
  return TestableWinSandbox::GetSDDL(type, kDummyUserSID, kDummyGroupSID);
}

TEST(WinSandboxTest, GetSDDLForSharablePipe) {
  EXPECT_EQ(GetSDDL(WinSandbox::kSharablePipe),
            L"O:S-8"
            L"G:S-9"
            L"D:(A;;;;;OW)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;AC)"
            L"(A;;GA;;;S-8)"
            L"S:(ML;;NX;;;LW)");
}

TEST(WinSandboxTest, GetSDDLForLooseSharablePipe) {
  EXPECT_EQ(GetSDDL(WinSandbox::kLooseSharablePipe),
            L"O:S-8"
            L"G:S-9"
            L"D:(A;;;;;OW)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;AC)"
            L"(A;;GA;;;S-8)(A;;GA;;;RC)"
            L"S:(ML;;NX;;;LW)");
}

TEST(WinSandboxTest, GetSDDLForSharableEvent) {
  EXPECT_EQ(GetSDDL(WinSandbox::kSharableEvent),
            L"O:S-8"
            L"G:S-9"
            L"D:(A;;;;;OW)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GX;;;AC)(A;;GA;;;S-8)"
            L"(A;;GX;;;RC)"
            L"S:(ML;;NX;;;LW)");
}

TEST(WinSandboxTest, GetSDDLForSharableMutex) {
  EXPECT_EQ(GetSDDL(WinSandbox::kSharableMutex),
            L"O:S-8"
            L"G:S-9"
            L"D:(A;;;;;OW)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GX;;;AC)(A;;GA;;;S-8)"
            L"(A;;GX;;;RC)"
            L"S:(ML;;NX;;;LW)");
}

TEST(WinSandboxTest, GetSDDLForSharableFileForRead) {
  EXPECT_EQ(GetSDDL(WinSandbox::kSharableFileForRead),
            L"O:S-8"
            L"G:S-9"
            L"D:(A;;;;;OW)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GR;;;AC)(A;;GA;;;S-8)"
            L"(A;;GR;;;RC)"
            L"S:(ML;;NWNX;;;LW)");
}

TEST(WinSandboxTest, GetSDDLForIPCServerProcess) {
  EXPECT_EQ(GetSDDL(WinSandbox::kIPCServerProcess),
            L"O:S-8"
            L"G:S-9"
            L"D:(A;;;;;OW)(A;;GA;;;SY)(A;;GA;;;BA)(A;;0x1000;;;AC)(A;;GA;;;S-8)"
            L"(A;;0x1000;;;RC)");
}

TEST(WinSandboxTest, GetSDDLForPrivateObject) {
  EXPECT_EQ(GetSDDL(WinSandbox::kPrivateObject),
            L"O:S-8"
            L"G:S-9"
            L"D:(A;;;;;OW)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;S-8)");
}

}  // namespace
}  // namespace mozc
