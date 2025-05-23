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

#include "base/mmap.h"

#include <cstring>
#include <memory>
#include <string>

#include "base/file_util.h"
#include "base/util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "absl/flags/flag.h"

namespace mozc {
namespace {

TEST(MmapTest, MmapTest) {
  const std::string filename =
      FileUtil::JoinPath(absl::GetFlag(FLAGS_test_tmpdir), "test.db");

  constexpr size_t kFileNameSize[] = {1, 100, 1024, 8192};
  for (int i = 0; i < std::size(kFileNameSize); ++i) {
    ASSERT_OK(FileUtil::UnlinkIfExists(filename));
    auto buf = std::make_unique<char[]>(kFileNameSize[i]);
    memset(buf.get(), 0, kFileNameSize[i]);
    ASSERT_OK(FileUtil::SetContents(
        filename, absl::string_view(buf.get(), kFileNameSize[i])));

    Util::GetRandomSequence(buf.get(), kFileNameSize[i]);

    // Write Test
    {
      Mmap mmap;
      EXPECT_TRUE(mmap.Open(filename.c_str(), "r+"));
      memcpy(mmap.begin(), buf.get(), kFileNameSize[i]);

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(buf[j], mmap[j]);
      }

      memset(mmap.begin(), 0, kFileNameSize[i]);

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(0, mmap[j]);
      }

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        mmap[j] = buf[j];
      }

      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(buf[j], mmap[j]);
      }
    }

    // Read test
    {
      Mmap mmap;
      EXPECT_TRUE(mmap.Open(filename.c_str(), "r"));
      for (int j = 0; j < kFileNameSize[i]; ++j) {
        EXPECT_EQ(buf[j], mmap[j]);
      }
    }

    EXPECT_OK(FileUtil::Unlink(filename));
  }
}

TEST(MmapTest, MaybeMLockTest) {
  const size_t data_len = 32;
  std::unique_ptr<void, void (*)(void*)> addr(malloc(data_len), &free);
  if (Mmap::IsMLockSupported()) {
    ASSERT_EQ(0, Mmap::MaybeMLock(addr.get(), data_len));
    EXPECT_EQ(0, Mmap::MaybeMUnlock(addr.get(), data_len));
  } else {
    EXPECT_EQ(-1, Mmap::MaybeMLock(addr.get(), data_len));
    EXPECT_EQ(-1, Mmap::MaybeMUnlock(addr.get(), data_len));
  }
}

}  // namespace
}  // namespace mozc
