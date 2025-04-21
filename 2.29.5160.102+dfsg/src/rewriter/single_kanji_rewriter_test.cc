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

#include "rewriter/single_kanji_rewriter.h"

#include <cstddef>
#include <memory>
#include <string>

#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace mozc {

using dictionary::PosMatcher;

class SingleKanjiRewriterTest : public testing::TestWithTempUserProfile {
 protected:
  SingleKanjiRewriterTest() {
    data_manager_ = std::make_unique<testing::MockDataManager>();
    pos_matcher_.Set(data_manager_->GetPosMatcherData());
  }

  SingleKanjiRewriter *CreateSingleKanjiRewriter() const {
    return new SingleKanjiRewriter(*data_manager_);
  }

  const PosMatcher &pos_matcher() { return pos_matcher_; }

  static void InitSegments(absl::string_view key, absl::string_view value,
                           Segments *segments) {
    Segment *segment = segments->add_segment();
    segment->set_key(key);

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key.assign(key.data(), key.size());
    candidate->content_key.assign(key.data(), key.size());
    candidate->value.assign(value.data(), value.size());
    candidate->content_value.assign(value.data(), value.size());
  }

  static bool Contains(const Segments &segments, absl::string_view word) {
    const Segment &segment = segments.segment(0);
    for (int i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == word) {
        return true;
      }
    }
    return false;
  }

  const ConversionRequest default_request_;
  std::unique_ptr<testing::MockDataManager> data_manager_;
  PosMatcher pos_matcher_;
};

TEST_F(SingleKanjiRewriterTest, CapabilityTest) {
  std::unique_ptr<SingleKanjiRewriter> rewriter(CreateSingleKanjiRewriter());

  ConversionRequest convreq;
  commands::Request request;
  convreq.set_request(&request);

  request.set_mixed_conversion(false);
  EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::CONVERSION);
}

TEST_F(SingleKanjiRewriterTest, SetKeyTest) {
  std::unique_ptr<SingleKanjiRewriter> rewriter(CreateSingleKanjiRewriter());

  Segments segments;
  Segment *segment = segments.add_segment();
  const std::string kKey = "あ";
  segment->set_key(kKey);
  Segment::Candidate *candidate = segment->add_candidate();
  // First candidate may be inserted by other rewriters.
  candidate->key = "strange key";
  candidate->content_key = "starnge key";
  candidate->value = "starnge value";
  candidate->content_value = "strange value";

  EXPECT_EQ(segment->candidates_size(), 1);
  rewriter->Rewrite(default_request_, &segments);
  EXPECT_GT(segment->candidates_size(), 1);
  for (size_t i = 1; i < segment->candidates_size(); ++i) {
    EXPECT_EQ(segment->candidate(i).key, kKey);
  }
}

TEST_F(SingleKanjiRewriterTest, MobileEnvironmentTest) {
  ConversionRequest convreq;
  commands::Request request;
  convreq.set_request(&request);
  std::unique_ptr<SingleKanjiRewriter> rewriter(CreateSingleKanjiRewriter());

  {
    request.set_mixed_conversion(true);
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::ALL);
  }

  {
    request.set_mixed_conversion(false);
    EXPECT_EQ(rewriter->capability(convreq), RewriterInterface::CONVERSION);
  }
}

TEST_F(SingleKanjiRewriterTest, NounPrefixTest) {
  SingleKanjiRewriter rewriter(*data_manager_);
  Segments segments;
  Segment *segment1 = segments.add_segment();

  segment1->set_key("み");
  Segment::Candidate *candidate1 = segment1->add_candidate();

  candidate1->key = "み";
  candidate1->content_key = "見";
  candidate1->value = "見";
  candidate1->content_value = "見";

  EXPECT_EQ(segment1->candidates_size(), 1);
  rewriter.Rewrite(default_request_, &segments);

  EXPECT_EQ(segment1->candidate(0).value, "未");

  Segment *segment2 = segments.add_segment();

  segment2->set_key("こうたい");
  Segment::Candidate *candidate2 = segment2->add_candidate();

  candidate2->key = "こうたい";
  candidate2->content_key = "後退";
  candidate2->value = "後退";

  candidate2->lid = pos_matcher().GetContentWordWithConjugationId();
  candidate2->rid = pos_matcher().GetContentWordWithConjugationId();

  candidate1 = segment1->mutable_candidate(0);
  *candidate1 = Segment::Candidate();
  candidate1->key = "み";
  candidate1->content_key = "見";
  candidate1->value = "見";
  candidate1->content_value = "見";

  rewriter.Rewrite(default_request_, &segments);
  EXPECT_EQ(segment1->candidate(0).value, "見");

  // Only applied when right word's POS is noun.
  candidate2->lid = pos_matcher().GetContentNounId();
  candidate2->rid = pos_matcher().GetContentNounId();

  rewriter.Rewrite(default_request_, &segments);
  EXPECT_EQ(segment1->candidate(0).value, "未");

  EXPECT_EQ(segment1->candidate(0).lid, pos_matcher().GetNounPrefixId());
  EXPECT_EQ(segment1->candidate(0).rid, pos_matcher().GetNounPrefixId());
}

TEST_F(SingleKanjiRewriterTest, InsertionPositionTest) {
  SingleKanjiRewriter rewriter(*data_manager_);
  Segments segments;
  Segment *segment = segments.add_segment();

  segment->set_key("あ");
  for (int i = 0; i < 10; ++i) {
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = segment->key();
    candidate->content_key = segment->key();
    candidate->value = absl::StrFormat("cand%d", i);
    candidate->content_value = candidate->value;
  }

  EXPECT_EQ(segment->candidates_size(), 10);
  EXPECT_TRUE(rewriter.Rewrite(default_request_, &segments));
  EXPECT_LT(10, segment->candidates_size());  // Some candidates were inserted.

  for (int i = 0; i < 10; ++i) {
    // First 10 candidates have not changed.
    const Segment::Candidate &candidate = segment->candidate(i);
    EXPECT_EQ(candidate.value, absl::StrFormat("cand%d", i));
  }
}

TEST_F(SingleKanjiRewriterTest, AddDescriptionTest) {
  SingleKanjiRewriter rewriter(*data_manager_);
  Segments segments;
  Segment *segment = segments.add_segment();

  segment->set_key("あ");
  {
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = segment->key();
    candidate->content_key = segment->key();
    candidate->value = "亞";  // variant of "亜".
    candidate->content_value = candidate->value;
  }

  EXPECT_EQ(segment->candidates_size(), 1);
  EXPECT_TRUE(segment->candidate(0).description.empty());
  EXPECT_TRUE(rewriter.Rewrite(default_request_, &segments));
  EXPECT_LT(1, segment->candidates_size());  // Some candidates were inserted.
  EXPECT_EQ(segment->candidate(0).description, "亜の旧字体");
}

TEST_F(SingleKanjiRewriterTest, TriggerConditionForPrediction) {
  SingleKanjiRewriter rewriter(*data_manager_);

  {
    Segments segments;
    InitSegments("あ", "あ", &segments);

    commands::Request request;
    commands::RequestForUnitTest::FillMobileRequest(&request);
    ConversionRequest convreq;
    convreq.set_request_type(ConversionRequest::PREDICTION);
    convreq.set_request(&request);
    ASSERT_TRUE(rewriter.capability(convreq) & RewriterInterface::PREDICTION);
    EXPECT_TRUE(rewriter.Rewrite(convreq, &segments));
  }

  {
    Segments segments;
    InitSegments("あ", "あ", &segments);

    commands::Request request;
    commands::RequestForUnitTest::FillMobileRequest(&request);
    request.mutable_decoder_experiment_params()
        ->set_enable_single_kanji_prediction(true);
    ConversionRequest convreq;
    convreq.set_request_type(ConversionRequest::PREDICTION);
    convreq.set_request(&request);
    ASSERT_TRUE(rewriter.capability(convreq) & RewriterInterface::PREDICTION);
    EXPECT_FALSE(rewriter.Rewrite(convreq, &segments));
  }

  {
    Segments segments;
    InitSegments("あ", "あ", &segments);

    commands::Request request;
    commands::RequestForUnitTest::FillMobileRequestWithHardwareKeyboard(
        &request);
    request.mutable_decoder_experiment_params()
        ->set_enable_single_kanji_prediction(true);
    ConversionRequest convreq;
    convreq.set_request_type(ConversionRequest::PREDICTION);
    convreq.set_request(&request);
    ASSERT_FALSE(rewriter.capability(convreq) & RewriterInterface::PREDICTION);
  }

  {
    Segments segments;
    InitSegments("あ", "あ", &segments);

    commands::Request request;
    commands::RequestForUnitTest::FillMobileRequestWithHardwareKeyboard(
        &request);
    request.mutable_decoder_experiment_params()
        ->set_enable_single_kanji_prediction(true);
    ConversionRequest convreq;
    convreq.set_request_type(ConversionRequest::CONVERSION);
    convreq.set_request(&request);
    ASSERT_TRUE(rewriter.capability(convreq) & RewriterInterface::CONVERSION);
    EXPECT_TRUE(rewriter.Rewrite(convreq, &segments));
  }
}

TEST_F(SingleKanjiRewriterTest, NoVariationTest) {
  SingleKanjiRewriter rewriter(*data_manager_);

  Segments segments;
  InitSegments("かみ", "神", &segments);  // U+795E

  ConversionRequest svs_convreq;
  commands::Request request;
  request.mutable_decoder_experiment_params()->set_variation_character_types(
      commands::DecoderExperimentParams::NO_VARIATION);
  svs_convreq.set_request(&request);

  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_TRUE(rewriter.Rewrite(svs_convreq, &segments));
  EXPECT_FALSE(Contains(segments, "\u795E\uFE00"));  // 神︀ SVS character.
  EXPECT_TRUE(Contains(segments, "\uFA19"));  // 神 CJK compat ideograph.
}

TEST_F(SingleKanjiRewriterTest, SvsVariationTest) {
  SingleKanjiRewriter rewriter(*data_manager_);

  Segments segments;
  InitSegments("かみ", "神", &segments);  // U+795E

  ConversionRequest svs_convreq;
  commands::Request request;
  request.mutable_decoder_experiment_params()->set_variation_character_types(
      commands::DecoderExperimentParams::SVS_JAPANESE);
  svs_convreq.set_request(&request);

  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_TRUE(rewriter.Rewrite(svs_convreq, &segments));
  EXPECT_TRUE(Contains(segments, "\u795E\uFE00"));  // 神︀ SVS character.
  EXPECT_FALSE(Contains(segments, "\uFA19"));       // 神 CJK compat ideograph.
}

TEST_F(SingleKanjiRewriterTest, EmptySegments) {
  SingleKanjiRewriter rewriter(*data_manager_);

  Segments segments;

  EXPECT_EQ(segments.conversion_segments_size(), 0);
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));
}

TEST_F(SingleKanjiRewriterTest, EmptyCandidates) {
  SingleKanjiRewriter rewriter(*data_manager_);

  Segments segments;
  Segment *segment = segments.add_segment();
  segment->set_key("み");

  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 0);
  EXPECT_FALSE(rewriter.Rewrite(default_request_, &segments));
}
}  // namespace mozc
