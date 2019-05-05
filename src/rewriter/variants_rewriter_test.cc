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

#include "rewriter/variants_rewriter.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "config/character_form_manager.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/pos_matcher.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

namespace mozc {
namespace {

using config::CharacterFormManager;
using config::Config;
using dictionary::POSMatcher;

string AppendString(const string &lhs, const string &rhs) {
  if (!rhs.empty()) {
    return lhs + ' ' + rhs;
  }
  return lhs;
}

}  // namespace

class VariantsRewriterTest : public ::testing::Test {
 protected:
  // Explicitly define constructor to prevent Visual C++ from
  // considering this class as POD.
  VariantsRewriterTest() {}

  void SetUp() override {
    Reset();
    pos_matcher_.Set(mock_data_manager_.GetPOSMatcherData());
  }

  void TearDown() override {
    Reset();
  }

  void Reset() {
    CharacterFormManager::GetCharacterFormManager()->SetDefaultRule();
    CharacterFormManager::GetCharacterFormManager()->ClearHistory();
  }

  void InitSegmentsForAlphabetRewrite(const string &value,
                                      Segments *segments) const {
    Segment *segment = segments->push_back_segment();
    CHECK(segment);
    segment->set_key(value);
    Segment::Candidate *candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->Init();
    candidate->key = value;
    candidate->content_key = value;
    candidate->value = value;
    candidate->content_value = value;
  }

  VariantsRewriter *CreateVariantsRewriter() const {
    return new VariantsRewriter(pos_matcher_);
  }

  POSMatcher pos_matcher_;

 private:
  const testing::ScopedTmpUserProfileDirectory tmp_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
};

TEST_F(VariantsRewriterTest, RewriteTest) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Segments segments;
  const ConversionRequest request;

  Segment *seg = segments.push_back_segment();

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "あいう";
    candidate->content_value = "あいう";
    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "012";
    candidate->content_value = "012";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "012", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("０１２", seg->candidate(0).value);
    EXPECT_EQ("０１２", seg->candidate(0).content_value);
    EXPECT_EQ("012", seg->candidate(1).value);
    EXPECT_EQ("012", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "012";
    candidate->content_value = "012";
    candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "012", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, seg->candidates_size());
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "Google";
    candidate->content_value = "Google";
    CharacterFormManager::GetCharacterFormManager()->
      SetCharacterForm("abc", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("Ｇｏｏｇｌｅ", seg->candidate(0).value);
    EXPECT_EQ("Ｇｏｏｇｌｅ", seg->candidate(0).content_value);
    EXPECT_EQ("Google", seg->candidate(1).value);
    EXPECT_EQ("Google", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "@";
    candidate->content_value = "@";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "@", Config::FULL_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("＠", seg->candidate(0).value);
    EXPECT_EQ("＠", seg->candidate(0).content_value);
    EXPECT_EQ("@", seg->candidate(1).value);
    EXPECT_EQ("@", seg->candidate(1).content_value);
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";
    CharacterFormManager::GetCharacterFormManager()->SetCharacterForm(
        "アイウ", Config::FULL_WIDTH);

    EXPECT_FALSE(rewriter->Rewrite(request, &segments));
    seg->clear_candidates();
  }

  {
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->Init();
    candidate->value = "グーグル";
    candidate->content_value = "グーグル";
    CharacterFormManager::GetCharacterFormManager()->AddConversionRule(
        "アイウ", Config::HALF_WIDTH);

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(2, seg->candidates_size());
    EXPECT_EQ("ｸﾞｰｸﾞﾙ", seg->candidate(0).value);
    EXPECT_EQ("ｸﾞｰｸﾞﾙ", seg->candidate(0).content_value);
    EXPECT_EQ("グーグル", seg->candidate(1).value);
    EXPECT_EQ("グーグル", seg->candidate(1).content_value);
    seg->clear_candidates();
  }
}

TEST_F(VariantsRewriterTest, RewriteTestManyCandidates) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  Segments segments;
  const ConversionRequest request;
  Segment *seg = segments.push_back_segment();

  {
    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->Init();
      candidate1->value = std::to_string(i);
      candidate1->content_value = std::to_string(i);
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->Init();
      candidate2->content_key = "ぐーぐる";
      candidate2->key = "ぐーぐる";
      candidate2->value = "ぐーぐる";
      candidate2->content_value = "ぐーぐる";
    }

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(30, seg->candidates_size());

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 1).value);
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 1).content_value);
      string full_width;
      Util::HalfWidthToFullWidth(seg->candidate(3 * i + 1).value, &full_width);
      EXPECT_EQ(full_width, seg->candidate(3 * i).value);
      EXPECT_EQ(full_width, seg->candidate(3 * i).content_value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i + 2).value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i + 2).content_value);
    }
  }

  {
    seg->Clear();

    for (int i = 0; i < 10; ++i) {
      Segment::Candidate *candidate1 = seg->add_candidate();
      candidate1->Init();
      candidate1->content_key = "ぐーぐる";
      candidate1->key = "ぐーぐる";
      candidate1->value = "ぐーぐる";
      candidate1->content_value = "ぐーぐる";
      Segment::Candidate *candidate2 = seg->add_candidate();
      candidate2->Init();
      candidate2->value = std::to_string(i);
      candidate2->content_value = std::to_string(i);
    }

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(30, seg->candidates_size());

    for (int i = 0; i < 10; ++i) {
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 2).value);
      EXPECT_EQ(std::to_string(i), seg->candidate(3 * i + 2).content_value);
      string full_width;
      Util::HalfWidthToFullWidth(seg->candidate(3 * i + 2).value, &full_width);
      EXPECT_EQ(full_width, seg->candidate(3 * i + 1).value);
      EXPECT_EQ(full_width, seg->candidate(3 * i + 1).content_value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i).value);
      EXPECT_EQ("ぐーぐる", seg->candidate(3 * i).content_value);
    }
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForCandidate) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[半] アルファベット"
    EXPECT_EQ(AppendString(VariantsRewriter::kHalfWidth,
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  // containing symbols
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[半] アルファベット"
    EXPECT_EQ(AppendString(VariantsRewriter::kHalfWidth,
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[半] アルファベット"
    EXPECT_EQ(AppendString(VariantsRewriter::kHalfWidth,
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[半] アルファベット"
    EXPECT_EQ(AppendString(VariantsRewriter::kHalfWidth,
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "コギト・エルゴ・スム";
    candidate.content_value = candidate.value;
    candidate.content_key = "こぎとえるごすむ";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] カタカナ"
    EXPECT_EQ(AppendString(VariantsRewriter::kFullWidth,
                           VariantsRewriter::kKatakana),
        candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    EXPECT_EQ(VariantsRewriter::kHalfWidth, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(AppendString(VariantsRewriter::kFullWidth,
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "草彅剛";
    candidate.content_value = candidate.value;
    candidate.content_key = "くさなぎつよし";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    EXPECT_EQ(VariantsRewriter::kPlatformDependent, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "\\";
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    const char *expected = "[半] バックスラッシュ";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "＼";  // Full-width backslash
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    const char *expected = "[全] バックスラッシュ";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "¥";  // Half-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "[半] 円記号 <機種依存文字>" for Desktop,
    // "[半] 円記号 <機種依存>" for Android
    string expected = "[半] 円記号 ";
    expected.append(VariantsRewriter::kPlatformDependent);
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "￥";   // Full-width yen-symbol
    candidate.content_value = candidate.value;
    candidate.content_key = "えん";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    const char *expected = "[全] 円記号";
    EXPECT_EQ(expected, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    // An emoji character of mouse face.
    candidate.value = "🐭";
    candidate.content_value = candidate.value;
    candidate.content_key = "ねずみ";
    candidate.description = "絵文字";
    VariantsRewriter::SetDescriptionForCandidate(pos_matcher_, &candidate);
    // "絵文字 <機種依存文字>" for Desktop, "絵文字 <機種依存>" for Andorid
    string expected("絵文字" " ");
    expected.append(VariantsRewriter::kPlatformDependent);
    EXPECT_EQ(expected, candidate.description);
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForTransliteration) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[半] アルファベット"
    EXPECT_EQ(AppendString(VariantsRewriter::kHalfWidth,
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[半]"
    EXPECT_EQ(VariantsRewriter::kHalfWidth, candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    // "[全] アルファベット"
    EXPECT_EQ(AppendString(VariantsRewriter::kFullWidth,
                           VariantsRewriter::kAlphabet),
              candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "草彅剛";
    candidate.content_value = candidate.value;
    candidate.content_key = "くさなぎつよし";
    VariantsRewriter::SetDescriptionForTransliteration(pos_matcher_,
                                                       &candidate);
    EXPECT_EQ(VariantsRewriter::kPlatformDependent, candidate.description);
  }
}

TEST_F(VariantsRewriterTest, SetDescriptionForPrediction) {
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "HalfASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "halfascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  // containing symbols
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half ASCII";
    candidate.content_value = candidate.value;
    candidate.content_key = "half ascii";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "Half!ASCII!";
    candidate.content_value = candidate.value;
    candidate.content_key = "half!ascii!";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "CD-ROM";
    candidate.content_value = candidate.value;
    candidate.content_key = "しーでぃーろむ";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "!@#";
    candidate.content_value = candidate.value;
    candidate.content_key = "!@#";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "「ＡＢＣ」";
    candidate.content_value = candidate.value;
    candidate.content_key = "[ABC]";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ("", candidate.description);
  }
  {
    Segment::Candidate candidate;
    candidate.Init();
    candidate.value = "草彅剛";
    candidate.content_value = candidate.value;
    candidate.content_key = "くさなぎつよし";
    VariantsRewriter::SetDescriptionForPrediction(pos_matcher_, &candidate);
    EXPECT_EQ(VariantsRewriter::kPlatformDependent, candidate.description);
  }
}

TEST_F(VariantsRewriterTest, RewriteForConversion) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request;
  {
    Segments segments;
    segments.set_request_type(Segments::CONVERSION);
    {
      Segment *segment = segments.push_back_segment();
      segment->set_key("abc");
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->Init();
      candidate->key = "abc";
      candidate->content_key = "abc";
      candidate->value = "abc";
      candidate->content_value = "abc";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(0).value);
    EXPECT_EQ("abc", segments.segment(0).candidate(1).value);
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    segments.set_request_type(Segments::CONVERSION);
    {
      Segment *segment = segments.push_back_segment();
      segment->set_key("abc");
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->Init();
      candidate->key = "abc";
      candidate->content_key = "abc";
      candidate->value = "abc";
      candidate->content_value = "abc";
    }
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("abc", segments.segment(0).candidate(0).value);
    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(1).value);
  }
}

TEST_F(VariantsRewriterTest, RewriteForPrediction) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request;
  {
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(0).value);
    EXPECT_EQ("abc", segments.segment(0).candidate(1).value);
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(2, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("abc", segments.segment(0).candidate(0).value);
    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(1).value);
  }
}

TEST_F(VariantsRewriterTest, RewriteForSuggestion) {
  CharacterFormManager *character_form_manager =
      CharacterFormManager::GetCharacterFormManager();
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request;
  {
    Segments segments;
    segments.set_request_type(Segments::SUGGESTION);
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(1, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::FULL_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("ａｂｃ", segments.segment(0).candidate(0).value);
  }
  {
    character_form_manager->SetCharacterForm("abc", Config::HALF_WIDTH);
    Segments segments;
    segments.set_request_type(Segments::SUGGESTION);
    InitSegmentsForAlphabetRewrite("abc", &segments);
    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_EQ(1, segments.segment(0).candidates_size());

    EXPECT_EQ(Config::HALF_WIDTH,
              character_form_manager->GetConversionCharacterForm("abc"));

    EXPECT_EQ("abc", segments.segment(0).candidate(0).value);
  }
  {
    // Test for candidate with inner segment boundary.
    Segments segments;
    segments.set_request_type(Segments::SUGGESTION);

    Segment *segment = segments.push_back_segment();
    segment->set_key("まじ!");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->Init();
    candidate->key = "まじ!";
    candidate->content_key = candidate->key;
    candidate->value = "マジ!";
    candidate->content_value = candidate->value;
    candidate->inner_segment_boundary.push_back(
        Segment::Candidate::EncodeLengths(6, 6, 6, 6));  // 6 bytes for "まじ"
    candidate->inner_segment_boundary.push_back(
        Segment::Candidate::EncodeLengths(1, 1, 1, 1));  // 1 byte for "!"

    EXPECT_TRUE(rewriter->Rewrite(request, &segments));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_EQ(1, segments.segment(0).candidates_size());

    const Segment::Candidate &rewritten_candidate =
        segments.segment(0).candidate(0);
    EXPECT_EQ("マジ！",  // "マジ！" (full-width)
              rewritten_candidate.value);
    EXPECT_EQ("マジ！",  // "マジ！" (full-width)
              rewritten_candidate.content_value);
    ASSERT_EQ(2, rewritten_candidate.inner_segment_boundary.size());

    // Boundary information for
    // key="まじ", value="マジ", ckey="まじ", cvalue="マジ"
    EXPECT_EQ(Segment::Candidate::EncodeLengths(6, 6, 6, 6),
              rewritten_candidate.inner_segment_boundary[0]);
    // Boundary information for
    // key="!", value="！", ckey="!", cvalue="！".
    // Values are converted to full-width.
    EXPECT_EQ(Segment::Candidate::EncodeLengths(1, 3, 1, 3),
              rewritten_candidate.inner_segment_boundary[1]);
  }
}

TEST_F(VariantsRewriterTest, Capability) {
  std::unique_ptr<VariantsRewriter> rewriter(CreateVariantsRewriter());
  const ConversionRequest request;
  EXPECT_EQ(RewriterInterface::ALL, rewriter->capability(request));
}

}  // namespace mozc
