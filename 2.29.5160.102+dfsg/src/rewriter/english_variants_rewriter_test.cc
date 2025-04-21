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

#include "rewriter/english_variants_rewriter.h"

#include <cstddef>
#include <string>
#include <vector>

#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "absl/strings/string_view.h"

namespace mozc {

class EnglishVariantsRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  bool GetRankFromValue(const Segment &segment, const absl::string_view value,
                        int *rank) {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        *rank = static_cast<int>(i);
        return true;
      }
    }
    return false;
  }
};

TEST_F(EnglishVariantsRewriterTest, ExpandEnglishVariants) {
  EnglishVariantsRewriter rewriter;
  std::vector<std::string> variants;

  EXPECT_TRUE(rewriter.ExpandEnglishVariants("foo", &variants));
  EXPECT_EQ(variants.size(), 2);
  EXPECT_EQ(variants[0], "Foo");
  EXPECT_EQ(variants[1], "FOO");

  EXPECT_TRUE(rewriter.ExpandEnglishVariants("Bar", &variants));
  EXPECT_EQ(variants.size(), 2);
  EXPECT_EQ(variants[0], "bar");
  EXPECT_EQ(variants[1], "BAR");

  EXPECT_TRUE(rewriter.ExpandEnglishVariants("HOGE", &variants));
  EXPECT_EQ(variants.size(), 2);
  EXPECT_EQ(variants[0], "hoge");
  EXPECT_EQ(variants[1], "Hoge");

  EXPECT_FALSE(rewriter.ExpandEnglishVariants("Foo Bar", &variants));

  EXPECT_TRUE(rewriter.ExpandEnglishVariants("iPhone", &variants));
  EXPECT_EQ(variants.size(), 1);
  EXPECT_EQ(variants[0], "iphone");

  EXPECT_TRUE(rewriter.ExpandEnglishVariants("MeCab", &variants));
  EXPECT_EQ(variants.size(), 1);
  EXPECT_EQ(variants[0], "mecab");

  EXPECT_FALSE(rewriter.ExpandEnglishVariants("グーグル", &variants));
}

TEST_F(EnglishVariantsRewriterTest, RewriteTest) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  // T13N
  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_EQ(seg->candidate(1).value, "google");
    EXPECT_EQ(seg->candidate(1).content_value, "google");
    EXPECT_EQ(seg->candidate(2).value, "GOOGLE");
    EXPECT_EQ(seg->candidate(2).content_value, "GOOGLE");
  }

  {
    seg->Clear();

    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->value = std::to_string(i);
      candidate1->content_value = std::to_string(i);
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->content_key = "ぐーぐる";
      candidate2->key = "ぐーぐる";
      candidate2->value = "Google";
      candidate2->content_value = "Google";
      candidate2->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;
    }

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 40);

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(seg->candidate(4 * i).value, std::to_string(i));
      EXPECT_EQ(seg->candidate(4 * i).content_value, std::to_string(i));
      EXPECT_EQ(seg->candidate(4 * i + 1).value, "Google");
      EXPECT_EQ(seg->candidate(4 * i + 1).content_value, "Google");
      EXPECT_EQ(seg->candidate(4 * i + 2).value, "google");
      EXPECT_EQ(seg->candidate(4 * i + 2).content_value, "google");
      EXPECT_EQ(seg->candidate(4 * i + 3).value, "GOOGLE");
      EXPECT_EQ(seg->candidate(4 * i + 3).content_value, "GOOGLE");
    }
  }
}

TEST_F(EnglishVariantsRewriterTest, Regression3242753) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  // Multi-word English candidate should not be expanded.
  // NO_VARIANTS_EXPANSION is passed to the candidate only.
  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "まいけるじゃくそん";
    candidate->key = "まいけるじゃくそん";
    candidate->value = "Michael Jackson";
    candidate->content_value = "Michael Jackson";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Michael Jackson");
    EXPECT_EQ(seg->candidate(0).content_value, "Michael Jackson");
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Michael Jackson");
    EXPECT_EQ(seg->candidate(0).content_value, "Michael Jackson");
    EXPECT_NE((seg->candidate(0).attributes &
               Segment::Candidate::NO_VARIANTS_EXPANSION),
              0);
  }
}

TEST_F(EnglishVariantsRewriterTest, Regression5137299) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
  }

  {
    seg->clear_candidates();
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    candidate->attributes |= Segment::Candidate::USER_DICTIONARY;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_EQ(seg->candidate(1).value, "google");
    EXPECT_EQ(seg->candidate(1).content_value, "google");
    EXPECT_EQ(seg->candidate(2).value, "GOOGLE");
    EXPECT_EQ(seg->candidate(2).content_value, "GOOGLE");
  }
}

TEST_F(EnglishVariantsRewriterTest, DoNotAddDuplicatedCandidates) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "GOOGLE";
    candidate->content_value = "GOOGLE";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "google";
    candidate->content_value = "google";

    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 4);  // Kana, lower, upper, capitalized
  }
}

TEST_F(EnglishVariantsRewriterTest, KeepRank) {
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "GOOGLE";
    candidate->content_value = "GOOGLE";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";

    candidate = seg->add_candidate();
    candidate->content_key = "ぐーぐる";
    candidate->key = "ぐーぐる";
    candidate->value = "google";
    candidate->content_value = "google";

    EXPECT_EQ(seg->candidates_size(), 3);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));

    int upper_rank, lower_rank, capitalized_rank, kana_rank;
    EXPECT_TRUE(GetRankFromValue(*seg, "GOOGLE", &upper_rank));
    EXPECT_TRUE(GetRankFromValue(*seg, "google", &lower_rank));
    EXPECT_TRUE(GetRankFromValue(*seg, "Google", &capitalized_rank));
    EXPECT_TRUE(GetRankFromValue(*seg, "グーグル", &kana_rank));
    EXPECT_LT(upper_rank, lower_rank);
    EXPECT_LT(kana_rank, lower_rank);
    EXPECT_LT(lower_rank, capitalized_rank);
  }
}

TEST_F(EnglishVariantsRewriterTest, ExpandEnglishEntry) {
  // Fix variants
  EnglishVariantsRewriter rewriter;
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->content_key = "google";
    candidate->key = "google";
    candidate->value = "Google";
    candidate->content_value = "Google";
    candidate->attributes &= ~Segment::Candidate::NO_VARIANTS_EXPANSION;

    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(seg->candidates_size(), 1);
    EXPECT_EQ(seg->candidate(0).value, "Google");
    EXPECT_EQ(seg->candidate(0).content_value, "Google");
    EXPECT_NE((seg->candidate(0).attributes &
               Segment::Candidate::NO_VARIANTS_EXPANSION),
              0);
  }
}

TEST_F(EnglishVariantsRewriterTest, MobileEnvironmentTest) {
  ConversionRequest convreq;
  commands::Request request;
  convreq.set_request(&request);
  EnglishVariantsRewriter rewriter;

  {
    request.set_mixed_conversion(true);
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    EXPECT_EQ(rewriter.capability(convreq), RewriterInterface::CONVERSION);
  }
}

}  // namespace mozc
