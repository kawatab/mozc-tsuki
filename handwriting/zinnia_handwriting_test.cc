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

// Test of Zinnia handwriting module.

#include "handwriting/zinnia_handwriting.h"

#include <string>
#include <vector>

#include "base/file_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);

namespace mozc {
namespace handwriting {

class ZinniaHandwritingTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    const string filepath = FileUtil::JoinPath(
        FLAGS_test_srcdir,
        "handwriting-ja.model");
    zinnia_.reset(new ZinniaHandwriting(filepath));
  }

  scoped_ptr<ZinniaHandwriting> zinnia_;
};

TEST_F(ZinniaHandwritingTest, Recognize) {
  // Initialize a horizontal line like "一".
  Strokes strokes;
  Stroke stroke;
  stroke.push_back(make_pair(0.2, 0.5));
  stroke.push_back(make_pair(0.8, 0.5));
  strokes.push_back(stroke);

  vector<string> results;
  const HandwritingStatus status = zinnia_->Recognize(strokes, &results);
  EXPECT_EQ(HANDWRITING_NO_ERROR, status);
  // "一"
  EXPECT_EQ("\xE4\xB8\x80", results[0]);
}

TEST_F(ZinniaHandwritingTest, Commit) {
  Strokes strokes;
  string result;
  // Always returns HANDWRITING_NO_ERROR in the current implementation.
  EXPECT_EQ(HANDWRITING_NO_ERROR, zinnia_->Commit(strokes, result));
}

}  // namespace handwriting
}  // namespace mozc
