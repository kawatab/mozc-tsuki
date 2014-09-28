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

#include "base/version.h"

#include "base/util.h"
#include "testing/base/public/gunit.h"

// Import the generated version_def.h.
#include "base/version_def.h"

namespace mozc {

TEST(VersionTest, BasicTest) {
  EXPECT_EQ(version::kMozcVersion, Version::GetMozcVersion());
}

TEST(VersionTest, VersionNumberTest) {
  const int major = Version::GetMozcVersionMajor();
  const int minor = Version::GetMozcVersionMinor();
  const int build_number = Version::GetMozcVersionBuildNumber();
  const int revision = Version::GetMozcVersionRevision();
  EXPECT_EQ(Version::GetMozcVersion(), Util::StringPrintf(
      "%d.%d.%d.%d", major, minor, build_number, revision));
}

TEST(VersionTest, BuildTypeTest) {
  EXPECT_EQ(version::kMozcBuildType, Version::GetMozcBuildType());
}

TEST(VersionTest, CompareVersion) {
  EXPECT_FALSE(Version::CompareVersion("0.0.0.0", "0.0.0.0"));
  EXPECT_FALSE(Version::CompareVersion("1.2.3.4", "1.2.3.4"));
  EXPECT_TRUE(Version::CompareVersion("0.0.0.0", "0.0.0.1"));
  EXPECT_TRUE(Version::CompareVersion("0.0.1.2", "0.1.2.3"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "5.2.3.4"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "1.5.3.4"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "1.2.5.4"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "1.2.3.5"));
  EXPECT_FALSE(Version::CompareVersion("5.2.3.4", "1.2.3.4"));
  EXPECT_FALSE(Version::CompareVersion("1.5.3.4", "1.2.3.4"));
  EXPECT_FALSE(Version::CompareVersion("1.2.5.4", "1.2.3.4"));
  EXPECT_FALSE(Version::CompareVersion("1.2.3.5", "1.2.3.4"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "15.2.3.4"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "1.25.3.4"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "1.2.35.4"));
  EXPECT_TRUE(Version::CompareVersion("1.2.3.4", "1.2.3.45"));
  EXPECT_FALSE(Version::CompareVersion("15.2.3.4", "1.2.3.4"));
  EXPECT_FALSE(Version::CompareVersion("1.25.3.4", "1.2.3.4"));
  EXPECT_FALSE(Version::CompareVersion("1.2.35.4", "1.2.3.4"));
  EXPECT_FALSE(Version::CompareVersion("1.2.3.45", "1.2.3.4"));

  // Always return false if "Unknown" is passed.
  EXPECT_FALSE(Version::CompareVersion("Unknown", "Unknown"));
  EXPECT_FALSE(Version::CompareVersion("0.0.0.0", "(Unknown)"));
  EXPECT_FALSE(Version::CompareVersion("Unknown", "0.0.0.0"));
  EXPECT_FALSE(Version::CompareVersion("0.0.0.0", "Unknown"));
  EXPECT_FALSE(Version::CompareVersion("(Unknown)", "(Unknown)"));
  EXPECT_FALSE(Version::CompareVersion("(Unknown)", "0.0.0.0"));
}

}  // namespace mozc
