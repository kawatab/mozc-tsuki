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

#include "rewriter/fortune_rewriter.h"

#include <cstddef>
#include <string>

#include "base/logging.h"
#include "base/system_util.h"
#include "converter/segments.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

void AddSegment(const string &key, const string &value,
                Segments *segments) {
  segments->Clear();
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;
}

bool HasFortune(const Segments &segments) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    if ("今日の運勢" == candidate.description) {
      // has valid value?
      if ("大吉" == candidate.value ||
          "吉" == candidate.value ||
          "中吉" == candidate.value ||
          "小吉" == candidate.value ||
          "末吉" == candidate.value ||
          "凶" == candidate.value) {
        return true;
      }
    }
  }
  return false;
}

class FortuneRewriterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(FortuneRewriterTest, BasicTest) {
  FortuneRewriter fortune_rewriter;
  const ConversionRequest request;

  Segments segments;
  AddSegment("test", "test", &segments);
  fortune_rewriter.Rewrite(request, &segments);
  EXPECT_FALSE(HasFortune(segments));

  AddSegment("おみくじ", "test", &segments);
  fortune_rewriter.Rewrite(request, &segments);
  EXPECT_TRUE(HasFortune(segments));
}

}  // namespace
}  // namespace mozc
