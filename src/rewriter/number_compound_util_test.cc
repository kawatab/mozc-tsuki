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

#include "rewriter/number_compound_util.h"

#include <memory>

#include "base/port.h"
#include "base/serialized_string_array.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "testing/base/public/gunit.h"

using mozc::dictionary::POSMatcher;

namespace mozc {
namespace number_compound_util {

TEST(NumberCompoundUtilTest, SplitStringIntoNumberAndCounterSuffix) {
  std::unique_ptr<uint32[]> buf;
  const StringPiece data = SerializedStringArray::SerializeToBuffer(
      {
          "デシベル",
          "回",
          "階",
      },
      &buf);
  SerializedStringArray suffix_array;
  ASSERT_TRUE(suffix_array.Init(data));

  // Test cases for splittable compounds.
  struct {
    const char* input;
    const char* expected_number;
    const char* expected_suffix;
    uint32 expected_script_type;
  } kSplittableCases[] = {
      {
          "一階",
          "一",
          "階",
          number_compound_util::KANJI,
      },
      {
          "壱階",
          "壱",
          "階",
          number_compound_util::OLD_KANJI,
      },
      {
          "三十一回",
          "三十一",
          "回",
          number_compound_util::KANJI,
      },
      {
          "三十一",
          "三十一",
          "",
          number_compound_util::KANJI,
      },
      {
          "デシベル",
          "",
          "デシベル",
      },
      {
          "回",
          "",
          "回",
      },
      {
          "階",
          "",
          "階",
      },
  };
  for (size_t i = 0; i < arraysize(kSplittableCases); ++i) {
    StringPiece actual_number, actual_suffix;
    uint32 actual_script_type = 0;
    EXPECT_TRUE(SplitStringIntoNumberAndCounterSuffix(
        suffix_array,
        kSplittableCases[i].input, &actual_number, &actual_suffix,
        &actual_script_type));
    EXPECT_EQ(kSplittableCases[i].expected_number, actual_number);
    EXPECT_EQ(kSplittableCases[i].expected_suffix, actual_suffix);
    EXPECT_EQ(kSplittableCases[i].expected_script_type, actual_script_type);
  }

  // Test cases for unsplittable compounds.
  const char* kUnsplittableCases[] = {
      "回八",
      "Google",
      "ア一階",
      "八億九千万600七十４デシベル",
  };
  for (size_t i = 0; i < arraysize(kUnsplittableCases); ++i) {
    StringPiece actual_number, actual_suffix;
    uint32 actual_script_type = 0;
    EXPECT_FALSE(SplitStringIntoNumberAndCounterSuffix(
        suffix_array,
        kUnsplittableCases[i], &actual_number, &actual_suffix,
        &actual_script_type));
  }
}

TEST(NumberCompoundUtilTest, IsNumber) {
  std::unique_ptr<uint32[]> buf;
  const StringPiece data = SerializedStringArray::SerializeToBuffer(
      {
          "回",
          "階",
      },
      &buf);
  SerializedStringArray suffix_array;
  ASSERT_TRUE(suffix_array.Init(data));

  const testing::MockDataManager data_manager;
  const POSMatcher pos_matcher(data_manager.GetPOSMatcherData());

  Segment::Candidate c;

  c.Init();
  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetNumberId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c.Init();
  c.lid = pos_matcher.GetKanjiNumberId();
  c.rid = pos_matcher.GetKanjiNumberId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c.Init();
  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetCounterSuffixWordId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c.Init();
  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetParallelMarkerId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c.Init();
  c.value = "一階";
  c.content_value = "一階";
  c.lid = pos_matcher.GetNumberId();
  c.rid = pos_matcher.GetNumberId();
  EXPECT_TRUE(IsNumber(suffix_array, pos_matcher, c));

  c.Init();
  c.lid = pos_matcher.GetAdverbId();
  c.rid = pos_matcher.GetAdverbId();
  EXPECT_FALSE(IsNumber(suffix_array, pos_matcher, c));
}

}  // namespace number_compound_util
}  // namespace mozc
