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

#include "rewriter/user_boundary_history_rewriter.h"

#include <cstddef>
#include <cstring>
#include <string>

#include "base/file_util.h"
#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"


DECLARE_string(test_tmpdir);

namespace mozc {
namespace {
void SetBoundedSegments(Segments *segments, bool fixed) {
  Segment *segment = segments->add_segment();
  // "たん"
  segment->set_key("\xe3\x81\x9f\xe3\x82\x93");
  if (fixed) {
    segment->set_segment_type(Segment::FIXED_VALUE);
  }
  Segment::Candidate *candidate = segment->add_candidate();
  // "たん"
  candidate->value = "\xe3\x81\x9f\xe3\x82\x93";
  // "たん"
  candidate->content_value = "\xe3\x81\x9f\xe3\x82\x93";

  segment = segments->add_segment();
  // "ぽぽ"
  segment->set_key("\xe3\x81\xbd\xe3\x81\xbd");
  if (fixed) {
    segment->set_segment_type(Segment::FIXED_VALUE);
  }
  candidate = segment->add_candidate();
  // "ぽぽ"
  candidate->value = "\xe3\x81\xbd\xe3\x81\xbd";
  // "ぽぽ"
  candidate->content_value = "\xe3\x81\xbd\xe3\x81\xbd";
}

void SetSegments(Segments *segments, bool fixed) {
  Segment *segment = segments->add_segment();
  // "たんぽぽ"
  segment->set_key("\xe3\x81\x9f\xe3\x82\x93\xe3\x81\xbd\xe3\x81\xbd");
  if (fixed) {
    segment->set_segment_type(Segment::FIXED_VALUE);
  }
  Segment::Candidate *candidate = segment->add_candidate();
  // "たんぽぽ"
  candidate->value = "\xe3\x81\x9f\xe3\x82\x93\xe3\x81\xbd\xe3\x81\xbd";
  // "たんぽぽ"
  candidate->content_value = "\xe3\x81\x9f\xe3\x82\x93\xe3\x81\xbd\xe3\x81\xbd";
}

void SetIncognito(bool incognito) {
  config::Config input;
  config::ConfigHandler::GetConfig(&input);
  input.set_incognito_mode(incognito);
  config::ConfigHandler::SetConfig(input);
  EXPECT_EQ(incognito, GET_CONFIG(incognito_mode));
}

void SetLearningLevel(config::Config::HistoryLearningLevel level) {
  config::Config input;
  config::ConfigHandler::GetConfig(&input);
  input.set_history_learning_level(level);
  config::ConfigHandler::SetConfig(input);
  EXPECT_EQ(level, GET_CONFIG(history_learning_level));
}
}  // namespace

class UserBoundaryHistoryRewriterTest : public testing::Test {
 protected:
  UserBoundaryHistoryRewriterTest() {}

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  virtual void TearDown() {
    UserBoundaryHistoryRewriter rewriter(&mock_);
    // clear history
    rewriter.Clear();
    // reset config of test_tmpdir.
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);
  }

  ConverterMock &mock() {
    return mock_;
  }

  const ConversionRequest default_request_;

 private:
  ConverterMock mock_;

  DISALLOW_COPY_AND_ASSIGN(UserBoundaryHistoryRewriterTest);
};

TEST_F(UserBoundaryHistoryRewriterTest, CreateFile) {
  UserBoundaryHistoryRewriter rewriter(&mock());
  const string history_file = FLAGS_test_tmpdir + "/boundary.db";
  EXPECT_TRUE(FileUtil::FileExists(history_file));
}

// "たんぽぽ" -> "たん|ぽぽ"
TEST_F(UserBoundaryHistoryRewriterTest, Rewrite1) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str = bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);

  EXPECT_TRUE(rewriter.Rewrite(default_request_, &segments));
  const string segments_str = segments.DebugString();

  // "たんぽぽ" -> "たん|ぽぽ"
  // make length of segment 0 from 4 to 2|2
  Segments input_seg;
  size_t input_start, input_size;
  uint8 *input_array;
  size_t input_array_size;
  mock().GetResizeSegment2(&input_seg, &input_start, &input_size,
                          &input_array, &input_array_size);
  const string input_seg_str = input_seg.DebugString();
  EXPECT_EQ(segments_str, input_seg_str);
  EXPECT_EQ(0, input_start);
  EXPECT_EQ(1, input_size);
  uint8 resize_array[8];
  memset(resize_array, 0, sizeof(resize_array));
  resize_array[0] = static_cast<uint8>(2);
  resize_array[1] = static_cast<uint8>(2);
  EXPECT_LT(2, input_array_size);
  for (int i = 0; i < input_array_size; ++i) {
    EXPECT_EQ(resize_array[i], input_array[i]);
    if (input_array[i] == 0) {
      break;
    }
  }
}

// "たん|ぽぽ" -> "たんぽぽ"
TEST_F(UserBoundaryHistoryRewriterTest, Rewrite2) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments segments;
  SetSegments(&segments, true);
  segments.set_resized(true);
  segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &segments);
  const string segments_str = segments.DebugString();

  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, false);
  bounded_segments.set_user_history_enabled(true);

  EXPECT_TRUE(rewriter.Rewrite(default_request_, &bounded_segments));
  const string bounded_segments_str = bounded_segments.DebugString();

  // "たん|ぽぽ" -> "たんぽぽ"
  // make length of segment 0 from 2 to 4
  Segments input_seg;
  size_t input_start, input_size;
  uint8 *input_array;
  size_t input_array_size;
  mock().GetResizeSegment2(&input_seg, &input_start, &input_size,
                          &input_array, &input_array_size);
  const string input_seg_str = input_seg.DebugString();
  EXPECT_EQ(bounded_segments_str, input_seg_str);
  EXPECT_EQ(0, input_start);
  EXPECT_EQ(2, input_size);
  uint8 resize_array[8];
  memset(resize_array, 0, sizeof(resize_array));
  resize_array[0] = static_cast<uint8>(4);
  EXPECT_LT(1, input_array_size);
  for (int i = 0; i < input_array_size; ++i) {
    EXPECT_EQ(resize_array[i], input_array[i]);
    if (input_array[i] == 0) {
      break;
    }
  }
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenIncognito) {
  SetIncognito(true);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str = bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);
  const string segments_str = segments.DebugString();

  SetIncognito(false);  // no_incognito when rewrite
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str = segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenReadOnly) {
  SetIncognito(false);
  SetLearningLevel(config::Config::READ_ONLY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str = bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);
  const string segments_str = segments.DebugString();

  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str = segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenDisableUserHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(false);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str = bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);
  const string segments_str = segments.DebugString();

  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str = segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoInsertWhenNotResized) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(false);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str = bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);
  const string segments_str =segments.DebugString();

  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str = segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteAfterClear) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str = bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);

  // clear history
  rewriter.Clear();

  const string segments_str = segments.DebugString();
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str = segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenIncognito) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str = bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);

  const string segments_str = segments.DebugString();
  SetIncognito(true);
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str = segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenNoHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str =bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);

  const string segments_str =segments.DebugString();
  SetLearningLevel(config::Config::NO_HISTORY);
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str =segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenDisabledUserHistory) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str =bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(false);

  const string segments_str =segments.DebugString();
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str =segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}

TEST_F(UserBoundaryHistoryRewriterTest, NoRewriteWhenAlreadyResized) {
  SetIncognito(false);
  SetLearningLevel(config::Config::DEFAULT_HISTORY);
  UserBoundaryHistoryRewriter rewriter(&mock());
  Segments bounded_segments;
  SetBoundedSegments(&bounded_segments, true);
  bounded_segments.set_resized(true);
  bounded_segments.set_user_history_enabled(true);

  rewriter.Finish(default_request_, &bounded_segments);
  const string bounded_segments_str =bounded_segments.DebugString();

  Segments segments;
  SetSegments(&segments, false);
  segments.set_user_history_enabled(true);
  segments.set_resized(true);

  const string segments_str = segments.DebugString();
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));

  const string segments_rewrited_str = segments.DebugString();
  EXPECT_EQ(segments_str, segments_rewrited_str);
}
}  // namespace mozc
