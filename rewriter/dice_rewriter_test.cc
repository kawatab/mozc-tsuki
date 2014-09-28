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

#include "rewriter/dice_rewriter.h"

#include <cstddef>
#include <string>

#include "base/system_util.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

// DiceRewriter inserts a candidate with this description.
// "出た目の数"
const char kDescription[] =
    "\xE5\x87\xBA\xE3\x81\x9F\xE7\x9B\xAE\xE3\x81\xAE\xE6\x95\xB0";

// "さいころ"
const char kKey[] = "\xE3\x81\x95\xE3\x81\x84\xE3\x81\x93\xE3\x82\x8D";

// Candidate window size.
const int kPageSize = 9;

void AddCandidate(const string &key, const string &value, Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->value = value;
  candidate->content_value = value;
  candidate->content_key = key;
}

void AddSegment(const string &key, Segments *segments) {
  Segment *segment = segments->push_back_segment();
  segment->set_key(key);
}

// Make a segments which has some dummy candidates.
void MakeSegments(Segments *segments,
                  const string &key,
                  const int &num_segment,
                  const int &num_dummy_candidate) {
  segments->Clear();

  AddSegment(key, segments);
  for (int i = 1; i < num_segment; ++i) {
    AddSegment(key, segments);
  }

  Segment *segment = segments->mutable_segment(0);
  for (int i = 0; i < num_dummy_candidate; ++i) {
    AddCandidate("test_key", "test_value", segment);
  }
}

// Return a number of dice number in candidates.
int CountDiceNumber(const Segment &segment) {
  int count_dice_number = 0;
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    if (candidate.description == kDescription) {
      ++count_dice_number;
    }
  }
  return count_dice_number;
}

bool HasValidValue(const Segment::Candidate &candidate) {
  if ("1" == candidate.value ||
      "2" == candidate.value ||
      "3" == candidate.value ||
      "4" == candidate.value ||
      "5" == candidate.value ||
      "6" == candidate.value) {
    return true;
  }

  return false;
}

size_t GetDiceNumberIndex(const Segment &segment) {
  size_t dice_number_index = segment.candidates_size();
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).description == kDescription) {
      dice_number_index = i;
      break;
    }
  }
  return dice_number_index;
}
}  // namespace

class DiceRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

// Test candidate insert positions.
TEST_F(DiceRewriterTest, InsertTest) {
  DiceRewriter dice_rewriter;
  Segments segments;
  const ConversionRequest request;

  // Check a dice number index with some mock candidates.
  for (int candidates_size = 1;
       candidates_size <= kPageSize;
       ++candidates_size) {
    MakeSegments(&segments, kKey, 1, candidates_size);

    EXPECT_TRUE(dice_rewriter.Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());

    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(1, CountDiceNumber(segment));

    size_t dice_number_index = GetDiceNumberIndex(segment);
    EXPECT_LE(1, dice_number_index);
    EXPECT_GT(kPageSize, dice_number_index);

    EXPECT_TRUE(HasValidValue(segment.candidate(dice_number_index)));
  }
}

// Test cases for no insertions.
TEST_F(DiceRewriterTest, IgnoringTest) {
  DiceRewriter dice_rewriter;
  Segments segments;
  const ConversionRequest request;

  // Candidates size is 0.
  MakeSegments(&segments, kKey, 1, 0);
  EXPECT_FALSE(dice_rewriter.Rewrite(request, &segments));

  // Segments key is not matched.
  MakeSegments(&segments, "dice", 1, 1);
  EXPECT_FALSE(dice_rewriter.Rewrite(request, &segments));

  // Segments size is more than 1.
  MakeSegments(&segments, kKey, 2, 1);
  EXPECT_FALSE(dice_rewriter.Rewrite(request, &segments));
}
}  // namespace mozc
