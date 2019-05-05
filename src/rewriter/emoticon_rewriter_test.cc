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

#include "rewriter/emoticon_rewriter.h"

#include <cstddef>
#include <memory>
#include <string>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

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

bool HasEmoticon(const Segments &segments) {
  CHECK_EQ(segments.segments_size(), 1);
  for (size_t i = 0; i < segments.segment(0).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(0).candidate(i);
    if (Util::StartsWith(candidate.description, "顔文字")) {
      return true;
    }
  }
  return false;
}

class EmoticonRewriterTest : public ::testing::Test {
 protected:
  testing::MockDataManager mock_data_manager_;

 private:
  testing::ScopedTmpUserProfileDirectory scoped_profile_dir_;
};

TEST_F(EmoticonRewriterTest, BasicTest) {
  std::unique_ptr<EmoticonRewriter> emoticon_rewriter =
      EmoticonRewriter::CreateFromDataManager(mock_data_manager_);

  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  ConversionRequest request;
  request.set_config(&config);
  {
    config.set_use_emoticon_conversion(true);

    Segments segments;
    AddSegment("test", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    AddSegment("かお", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));

    AddSegment("かおもじ", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));

    AddSegment("にこにこ", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));

    AddSegment("ふくわらい", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_TRUE(HasEmoticon(segments));
  }

  {
    config.set_use_emoticon_conversion(false);

    Segments segments;
    AddSegment("test", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    AddSegment("かお", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    AddSegment("かおもじ", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    AddSegment("にこにこ", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));

    AddSegment("ふくわらい", "test", &segments);
    emoticon_rewriter->Rewrite(request, &segments);
    EXPECT_FALSE(HasEmoticon(segments));
  }
}

TEST_F(EmoticonRewriterTest, MobileEnvironmentTest) {
  std::unique_ptr<EmoticonRewriter> rewriter =
      EmoticonRewriter::CreateFromDataManager(mock_data_manager_);

  commands::Request request;
  ConversionRequest convreq;
  convreq.set_request(&request);

  {
    request.set_mixed_conversion(true);
    EXPECT_EQ(RewriterInterface::ALL, rewriter->capability(convreq));
  }

  {
    request.set_mixed_conversion(false);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter->capability(convreq));
  }
}

}  // namespace
}  // namespace mozc
