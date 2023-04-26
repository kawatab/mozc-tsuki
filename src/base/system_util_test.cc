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

#include <cstring>
#include <string>
#include <vector>

#include "base/environ_mock.h"
#include "base/file_util.h"
#include "base/file_util_mock.h"
#include "base/port.h"
#include "base/util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/gunit.h"
#include "absl/strings/match.h"

namespace mozc {

class SystemUtilTest : public testing::Test {};

TEST_F(SystemUtilTest, GetUserProfileDirectory) {
#if defined(OS_CHROMEOS)
  EXPECT_EQ("/mutable", SystemUtil::GetUserProfileDirectory());

#elif defined(OS_WASM)
  EXPECT_TRUE(SystemUtil::GetUserProfileDirectory().empty());

#elif defined(OS_ANDROID)
  EXPECT_TRUE(SystemUtil::GetUserProfileDirectory().empty());

#elif defined(OS_IOS)
#elif defined(OS_WIN)
#elif defined(__APPLE__)
  // TODO(komatsu): write a test.

#elif defined(OS_LINUX)
  EnvironMock environ_mock;
  FileUtilMock file_util_mock;
  SystemUtil::SetUserProfileDirectory("");

  // The default path is "$HOME/.config/mozc".
  EXPECT_FALSE(FileUtil::DirectoryExists("/home/mozcuser/.config/mozc").ok());
  EXPECT_EQ("/home/mozcuser/.config/mozc",
            SystemUtil::GetUserProfileDirectory());
  EXPECT_OK(FileUtil::DirectoryExists("/home/mozcuser/.config/mozc"));

  environ_mock.SetEnv("XDG_CONFIG_HOME", "/tmp/config");

  // Once the dir is initialized, the value is not changed.
  EXPECT_EQ("/home/mozcuser/.config/mozc",
            SystemUtil::GetUserProfileDirectory());

  // Resets the cache by setting an empty string.
  SystemUtil::SetUserProfileDirectory("");

  // $XDG_CONFIG_HOME is already set with "/tmp/config" in above.
  // If $XDG_CONFIG_HOME is specified, "$XDG_CONFIG_HOME/mozc" is used.
  EXPECT_FALSE(FileUtil::DirectoryExists("/tmp/config/mozc").ok());
  EXPECT_EQ("/tmp/config/mozc", SystemUtil::GetUserProfileDirectory());
  EXPECT_OK(FileUtil::DirectoryExists("/tmp/config/mozc"));

  // Resets again.
  SystemUtil::SetUserProfileDirectory("");

  // If "$HOME/.mozc" exists, it is used for backward compatibility.
  EXPECT_OK(FileUtil::CreateDirectory("/home/mozcuser/.mozc"));
  EXPECT_OK(FileUtil::DirectoryExists("/home/mozcuser/.mozc"));
  EXPECT_EQ("/home/mozcuser/.mozc", SystemUtil::GetUserProfileDirectory());
  EXPECT_OK(FileUtil::DirectoryExists("/home/mozcuser/.mozc"));

  // Resets again to avoid side effects to other tests.
  SystemUtil::SetUserProfileDirectory("");

#else  // Platforms
#error Undefined target platform.

#endif  // Platforms
}

TEST_F(SystemUtilTest, IsWindowsX64Test) {
  // just make sure we can compile it.
  SystemUtil::IsWindowsX64();
}

TEST_F(SystemUtilTest, SetIsWindowsX64ModeForTest) {
  SystemUtil::SetIsWindowsX64ModeForTest(
      SystemUtil::IS_WINDOWS_X64_EMULATE_64BIT_MACHINE);
  EXPECT_TRUE(SystemUtil::IsWindowsX64());

  SystemUtil::SetIsWindowsX64ModeForTest(
      SystemUtil::IS_WINDOWS_X64_EMULATE_32BIT_MACHINE);
  EXPECT_FALSE(SystemUtil::IsWindowsX64());

  // Clear the emulation.
  SystemUtil::SetIsWindowsX64ModeForTest(
      SystemUtil::IS_WINDOWS_X64_DEFAULT_MODE);
}

TEST_F(SystemUtilTest, GetTotalPhysicalMemoryTest) {
  EXPECT_GT(SystemUtil::GetTotalPhysicalMemory(), 0);
}

#ifdef OS_ANDROID
TEST_F(SystemUtilTest, GetOSVersionStringTestForAndroid) {
  std::string result = SystemUtil::GetOSVersionString();
  // |result| must start with "Android ".
  EXPECT_TRUE(absl::StartsWith(result, "Android "));
}
#endif  // OS_ANDROID

}  // namespace mozc
