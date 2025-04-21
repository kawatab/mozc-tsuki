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

// Tests for session converter.
//
// Note that we have a lot of tests which assume that the converter fills
// T13Ns. If you want to add test case related to T13Ns, please make sure
// you set T13Ns to the result for a mock converter.

#include "session/session_converter.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/converter_mock.h"
#include "converter/segments.h"
#include "converter/segments_matchers.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/internal/candidate_list.h"
#include "session/request_test_util.h"
#include "session/session_converter_interface.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "testing/testing_util.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace session {
namespace {

using ::mozc::commands::Context;
using ::mozc::commands::Request;
using ::mozc::commands::RequestForUnitTest;
using ::mozc::config::Config;
using ::testing::_;
using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArgPointee;
using ::testing::SetArgPointee;

constexpr char kChars_Aiueo[] = "あいうえお";
constexpr char kChars_Mo[] = "も";
constexpr char kChars_Mozuku[] = "もずく";
constexpr char kChars_Mozukusu[] = "もずくす";
constexpr char kChars_Momonga[] = "ももんが";
}  // namespace

void AddSegmentWithSingleCandidate(Segments *segments, absl::string_view key,
                                   absl::string_view value) {
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  Segment::Candidate *cand = seg->add_candidate();
  cand->key.assign(key.data(), key.size());
  cand->content_key = cand->key;
  cand->value.assign(value.data(), value.size());
  cand->content_value = cand->value;
}

class SessionConverterTest : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();

    config_ = std::make_unique<Config>();
    config_->set_use_cascading_window(true);
    request_ = std::make_unique<Request>();

    table_ = std::make_unique<composer::Table>();
    table_->InitializeWithRequestAndConfig(*request_, *config_,
                                           mock_data_manager_);
    composer_ = std::make_unique<composer::Composer>(
        table_.get(), request_.get(), config_.get());
  }

  void TearDown() override {
    table_.reset();
    composer_.reset();

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  static void GetSegments(const SessionConverter &converter, Segments *dest) {
    CHECK(dest);
    *dest = *converter.segments_;
  }

  static const Segments &GetSegments(const SessionConverter &converter) {
    return *converter.segments_;
  }

  static void SetSegments(const Segments &src, SessionConverter *converter) {
    CHECK(converter);
    *converter->segments_ = src;
  }

  static const commands::Result &GetResult(const SessionConverter &converter) {
    return *converter.result_;
  }

  static const CandidateList &GetCandidateList(
      const SessionConverter &converter) {
    return *converter.candidate_list_;
  }

  static SessionConverterInterface::State GetState(
      const SessionConverter &converter) {
    return converter.state_;
  }

  static void SetState(SessionConverterInterface::State state,
                       SessionConverter *converter) {
    converter->state_ = state;
  }

  static size_t GetSegmentIndex(const SessionConverter &converter) {
    return converter.segment_index_;
  }

  static bool IsCandidateListVisible(const SessionConverter &converter) {
    return converter.candidate_list_visible_;
  }

  static const commands::Request &GetRequest(
      const SessionConverter &converter) {
    return *converter.request_;
  }

  static void GetPreedit(const SessionConverter &converter, size_t index,
                         size_t size, std::string *conversion) {
    converter.GetPreedit(index, size, conversion);
  }

  static void GetConversion(const SessionConverter &converter, size_t index,
                            size_t size, std::string *conversion) {
    converter.GetConversion(index, size, conversion);
  }

  static void AppendCandidateList(ConversionRequest::RequestType request_type,
                                  SessionConverter *converter) {
    ConversionRequest unused_request;
    converter->SetRequestType(request_type, &unused_request);
    converter->AppendCandidateList();
  }

  // set result for "あいうえお"
  static void SetAiueo(Segments *segments) {
    segments->Clear();
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments->add_segment();
    segment->set_key("あいうえお");
    candidate = segment->add_candidate();
    candidate->key = "あいうえお";
    candidate->value = candidate->key;
    candidate = segment->add_candidate();
    candidate->key = "あいうえお";
    candidate->value = "アイウエオ";
  }

  // set result for "かまぼこのいんぼう"
  static void SetKamaboko(Segments *segments) {
    Segment *segment;
    Segment::Candidate *candidate;

    segments->Clear();
    segment = segments->add_segment();

    segment->set_key("かまぼこの");
    candidate = segment->add_candidate();
    candidate->value = "かまぼこの";
    candidate = segment->add_candidate();
    candidate->value = "カマボコの";
    segment = segments->add_segment();
    segment->set_key("いんぼう");
    candidate = segment->add_candidate();
    candidate->value = "陰謀";
    candidate = segment->add_candidate();
    candidate->value = "印房";

    // Set dummy T13Ns
    std::vector<Segment::Candidate> *meta_candidates =
        segment->mutable_meta_candidates();
    meta_candidates->resize(transliteration::NUM_T13N_TYPES);
    for (size_t i = 0; i < transliteration::NUM_T13N_TYPES; ++i) {
      meta_candidates->at(i).value = segment->key();
      meta_candidates->at(i).content_value = segment->key();
      meta_candidates->at(i).content_key = segment->key();
    }
  }

  // set T13N candidates to segments using composer
  static void FillT13Ns(Segments *segments,
                        const composer::Composer *composer) {
    size_t composition_pos = 0;
    for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
      Segment *segment = segments->mutable_conversion_segment(i);
      CHECK(segment);
      const size_t composition_len = Util::CharsLen(segment->key());
      std::vector<std::string> t13ns;
      composer->GetSubTransliterations(composition_pos, composition_len,
                                       &t13ns);
      std::vector<Segment::Candidate> *meta_candidates =
          segment->mutable_meta_candidates();
      meta_candidates->resize(transliteration::NUM_T13N_TYPES);
      for (size_t j = 0; j < transliteration::NUM_T13N_TYPES; ++j) {
        meta_candidates->at(j).value = t13ns[j];
        meta_candidates->at(j).content_value = t13ns[j];
        meta_candidates->at(j).content_key = segment->key();
      }
      composition_pos += composition_len;
    }
  }

  // Sets the result for "like"
  void SetLike(Segments *segments) {
    composer_->InsertCharacterKeyAndPreedit("li", "ぃ");
    composer_->InsertCharacterKeyAndPreedit("ke", "け");

    Segment *segment;
    Segment::Candidate *candidate;

    segments->Clear();
    segment = segments->add_segment();

    segment->set_key("ぃ");
    candidate = segment->add_candidate();
    candidate->value = "ぃ";

    candidate = segment->add_candidate();
    candidate->value = "ィ";

    segment = segments->add_segment();
    segment->set_key("け");
    candidate = segment->add_candidate();
    candidate->value = "家";
    candidate = segment->add_candidate();
    candidate->value = "け";

    FillT13Ns(segments, composer_.get());
  }

  static Segments GetSegmentsTest() {
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key("てすと");
    candidate = segment->add_candidate();
    candidate->value = "テスト";
    candidate->key = "てすと";
    candidate->content_key = "てすと";
    return segments;
  }

  static void InsertASCIISequence(const std::string &text,
                                  composer::Composer *composer) {
    for (size_t i = 0; i < text.size(); ++i) {
      commands::KeyEvent key;
      key.set_key_code(text[i]);
      composer->InsertCharacterKeyEvent(key);
    }
  }

  static void ExpectSameSessionConverter(const SessionConverter &lhs,
                                         const SessionConverter &rhs) {
    EXPECT_EQ(lhs.IsActive(), rhs.IsActive());
    EXPECT_EQ(IsCandidateListVisible(lhs), IsCandidateListVisible(rhs));
    EXPECT_EQ(GetSegmentIndex(lhs), GetSegmentIndex(rhs));

    EXPECT_EQ(lhs.conversion_preferences().use_history,
              rhs.conversion_preferences().use_history);
    EXPECT_EQ(lhs.conversion_preferences().max_history_size,
              rhs.conversion_preferences().max_history_size);
    EXPECT_EQ(IsCandidateListVisible(lhs), IsCandidateListVisible(rhs));

    Segments segments_lhs, segments_rhs;
    GetSegments(lhs, &segments_lhs);
    GetSegments(rhs, &segments_rhs);
    EXPECT_EQ(segments_lhs.segments_size(), segments_rhs.segments_size());
    for (size_t i = 0; i < segments_lhs.segments_size(); ++i) {
      Segment segment_lhs, segment_rhs;
      segment_lhs = segments_lhs.segment(i);
      segment_rhs = segments_rhs.segment(i);
      EXPECT_EQ(segment_lhs.key(), segment_rhs.key()) << " i=" << i;
      EXPECT_EQ(segment_lhs.segment_type(), segment_rhs.segment_type())
          << " i=" << i;
      EXPECT_EQ(segment_lhs.candidates_size(), segment_rhs.candidates_size());
    }

    const CandidateList &candidate_list_lhs = GetCandidateList(lhs);
    const CandidateList &candidate_list_rhs = GetCandidateList(rhs);
    EXPECT_EQ(candidate_list_lhs.name(), candidate_list_rhs.name());
    EXPECT_EQ(candidate_list_lhs.page_size(), candidate_list_rhs.page_size());
    EXPECT_EQ(candidate_list_lhs.size(), candidate_list_rhs.size());
    EXPECT_EQ(candidate_list_lhs.last_index(), candidate_list_rhs.last_index());
    EXPECT_EQ(candidate_list_lhs.focused_id(), candidate_list_rhs.focused_id());
    EXPECT_EQ(candidate_list_lhs.focused_index(),
              candidate_list_rhs.focused_index());
    EXPECT_EQ(candidate_list_lhs.focused(), candidate_list_rhs.focused());

    for (int i = 0; i < candidate_list_lhs.size(); ++i) {
      const Candidate &candidate_lhs = candidate_list_lhs.candidate(i);
      const Candidate &candidate_rhs = candidate_list_rhs.candidate(i);
      EXPECT_EQ(candidate_lhs.id(), candidate_rhs.id());
      EXPECT_EQ(candidate_lhs.attributes(), candidate_rhs.attributes());
      EXPECT_EQ(candidate_lhs.HasSubcandidateList(),
                candidate_rhs.HasSubcandidateList());
      if (candidate_lhs.HasSubcandidateList()) {
        EXPECT_EQ(candidate_lhs.subcandidate_list().size(),
                  candidate_rhs.subcandidate_list().size());
      }
    }

    EXPECT_PROTO_EQ(GetResult(lhs), GetResult(rhs));
    EXPECT_PROTO_EQ(GetRequest(lhs), GetRequest(rhs));
  }

  static ::testing::AssertionResult ExpectSelectedCandidateIndices(
      const char *, const char *, const SessionConverter &converter,
      const std::vector<int> &expected) {
    const std::vector<int> &actual = converter.selected_candidate_indices_;

    if (expected.size() != actual.size()) {
      return ::testing::AssertionFailure()
             << "Indices size mismatch.\n"
             << "Expected: " << expected.size() << "\n"
             << "Actual:   " << actual.size();
    }

    for (size_t i = 0; i < expected.size(); ++i) {
      if (expected[i] != actual[i]) {
        return ::testing::AssertionFailure()
               << "Index mismatch.\n"
               << "Expected: " << expected[i] << "\n"
               << "Actual:   " << actual[i];
      }
    }

    return ::testing::AssertionSuccess();
  }

  static void SetCommandCandidate(Segments *segments, int segment_index,
                                  int candidate_index,
                                  Segment::Candidate::Command command) {
    segments->mutable_conversion_segment(segment_index)
        ->mutable_candidate(candidate_index)
        ->attributes |= Segment::Candidate::COMMAND_CANDIDATE;
    segments->mutable_conversion_segment(segment_index)
        ->mutable_candidate(candidate_index)
        ->command = command;
  }

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<composer::Table> table_;
  std::unique_ptr<Request> request_;
  std::unique_ptr<Config> config_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;

 private:
  const testing::MockDataManager mock_data_manager_;
};

#define EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, indices) \
  EXPECT_PRED_FORMAT2(ExpectSelectedCandidateIndices, converter, indices);

TEST_F(SessionConverterTest, Convert) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  {
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  std::vector<int> expected_indices;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_TRUE(converter.Convert(*composer_));
  ASSERT_TRUE(converter.IsActive());
  expected_indices.push_back(0);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());

  const commands::Preedit &conversion = output.preedit();
  EXPECT_EQ(conversion.segment_size(), 1);
  EXPECT_EQ(conversion.segment(0).annotation(),
            commands::Preedit::Segment::HIGHLIGHT);
  EXPECT_EQ(conversion.segment(0).value(), kChars_Aiueo);
  EXPECT_EQ(conversion.segment(0).key(), kChars_Aiueo);

  // Converter should be active before submittion
  EXPECT_TRUE(converter.IsActive());
  EXPECT_FALSE(IsCandidateListVisible(converter));

  converter.Commit(*composer_, Context::default_instance());
  composer_->Reset();
  output.Clear();
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());
  expected_indices.clear();
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  const commands::Result &result = output.result();
  EXPECT_EQ(result.value(), kChars_Aiueo);
  EXPECT_EQ(result.key(), kChars_Aiueo);

  // Converter should be inactive after submittion
  EXPECT_FALSE(converter.IsActive());
  EXPECT_FALSE(IsCandidateListVisible(converter));

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromConversion", 1);
  EXPECT_COUNT_STATS("ConversionCandidates0", 1);
}

TEST_F(SessionConverterTest, ConvertWithSpellingCorrection) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  {
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    segments.mutable_conversion_segment(0)->mutable_candidate(0)->attributes |=
        Segment::Candidate::SPELLING_CORRECTION;
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_TRUE(converter.Convert(*composer_));
  ASSERT_TRUE(converter.IsActive());
  EXPECT_TRUE(IsCandidateListVisible(converter));
}

TEST_F(SessionConverterTest, ConvertToTransliteration) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  {
    Segments segments;
    SetAiueo(&segments);
    composer_->InsertCharacterKeyAndPreedit("aiueo", kChars_Aiueo);
    FillT13Ns(&segments, composer_.get());
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  std::vector<int> expected_indices = {0};
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "aiueo");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "AIUEO");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ＡＩＵＥＯ");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  converter.Commit(*composer_, Context::default_instance());

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromConversion", 1);
  EXPECT_COUNT_STATS("ConversionCandidates0", 1);
}

TEST_F(SessionConverterTest, ConvertToTransliterationWithMultipleSegments) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  {
    Segments segments;
    SetLike(&segments);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  // Convert
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices = {0, 0};
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 2);
    EXPECT_EQ(conversion.segment(0).value(), "ぃ");
    EXPECT_EQ(conversion.segment(1).value(), "家");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  // Convert to half-width alphanumeric.
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 2);
    EXPECT_EQ(conversion.segment(0).value(), "li");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }
}

TEST_F(SessionConverterTest, ConvertToTransliterationWithoutCascadigWindow) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  {
    Segments segments;
    {
      Segment *segment;
      Segment::Candidate *candidate;
      segment = segments.add_segment();
      segment->set_key("dvd");
      candidate = segment->add_candidate();
      candidate->value = "dvd";
      candidate = segment->add_candidate();
      candidate->value = "DVD";
    }
    {  // Set OperationPreferences
      converter.set_use_cascading_window(false);
      converter.set_selection_shortcut(config::Config::NO_SHORTCUT);
    }
    composer_->InsertCharacterKeyAndPreedit("dvd", "ｄｖｄ");
    FillT13Ns(&segments, composer_.get());

    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  std::vector<int> expected_indices = {0};
  {  // Check the conversion #1
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ｄｖｄ");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #2
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ＤＶＤ");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  {  // Check the conversion #3
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "Ｄｖｄ");
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }
}

TEST_F(SessionConverterTest, MultiSegmentsConversion) {
  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";

  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  {
    Segments segments;
    SetKamaboko(&segments);
    composer_->InsertCharacterPreedit(kKamabokono + kInbou);
    FillT13Ns(&segments, composer_.get());
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices = {0, 0};
  {
    EXPECT_EQ(GetSegmentIndex(converter), 0);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    ASSERT_EQ(conversion.segment_size(), 2);
    EXPECT_EQ(conversion.segment(0).annotation(),
              commands::Preedit::Segment::HIGHLIGHT);
    EXPECT_EQ(conversion.segment(0).key(), kKamabokono);
    EXPECT_EQ(conversion.segment(0).value(), kKamabokono);

    EXPECT_EQ(conversion.segment(1).annotation(),
              commands::Preedit::Segment::UNDERLINE);
    EXPECT_EQ(conversion.segment(1).key(), kInbou);
    EXPECT_EQ(conversion.segment(1).value(), "陰謀");
  }

  // Test for candidates [CandidateNext]
  EXPECT_FALSE(IsCandidateListVisible(converter));
  converter.CandidateNext(*composer_);
  expected_indices[0] += 1;
  {
    EXPECT_TRUE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  // Test for candidates [CandidatePrev]
  converter.CandidatePrev();
  expected_indices[0] -= 1;
  {
    EXPECT_TRUE(IsCandidateListVisible(converter));
    EXPECT_EQ(GetSegmentIndex(converter), 0);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    ASSERT_EQ(candidates.size(), 3);  // two candidates + one t13n sub list.
    EXPECT_EQ(candidates.position(), 0);
    EXPECT_EQ(candidates.candidate(0).value(), kKamabokono);
    EXPECT_EQ(candidates.candidate(1).value(), "カマボコの");
    EXPECT_EQ(candidates.candidate(2).value(), "そのほかの文字種");
  }

  // Test for segment motion. [SegmentFocusRight]
  converter.SegmentFocusRight();
  {
    EXPECT_EQ(GetSegmentIndex(converter), 1);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.focused_index(), 0);
    ASSERT_EQ(candidates.size(), 3);  // two candidates + one t13n sub list.
    EXPECT_EQ(candidates.position(), 5);
    EXPECT_EQ(candidates.candidate(0).value(), "陰謀");
    EXPECT_EQ(candidates.candidate(1).value(), "印房");
    EXPECT_EQ(candidates.candidate(2).value(), "そのほかの文字種");
  }

  // Test for segment motion. [SegmentFocusLeft]
  converter.SegmentFocusLeft();
  {
    EXPECT_EQ(GetSegmentIndex(converter), 0);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.focused_index(), 0);
    ASSERT_EQ(candidates.size(), 3);  // two candidates + one t13n sub list.
    EXPECT_EQ(candidates.position(), 0);
    EXPECT_EQ(candidates.candidate(0).value(), kKamabokono);
    EXPECT_EQ(candidates.candidate(1).value(), "カマボコの");
    EXPECT_EQ(candidates.candidate(2).value(), "そのほかの文字種");
  }

  // Test for segment motion. [SegmentFocusLeft] at the head of segments.
  // http://b/2990134
  // Focus changing at the tail of segments to right,
  // and at the head of segments to left, should work.
  converter.SegmentFocusLeft();
  {
    EXPECT_EQ(GetSegmentIndex(converter), 1);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.focused_index(), 0);
    ASSERT_EQ(candidates.size(), 3);  // two candidates + one t13n sub list.
    EXPECT_EQ(candidates.position(), 5);
    EXPECT_EQ(candidates.candidate(0).value(), "陰謀");
    EXPECT_EQ(candidates.candidate(1).value(), "印房");
    EXPECT_EQ(candidates.candidate(2).value(), "そのほかの文字種");
  }

  // Test for segment motion. [SegmentFocusRight] at the tail of segments.
  // http://b/2990134
  // Focus changing at the tail of segments to right,
  // and at the head of segments to left, should work.
  converter.SegmentFocusRight();
  {
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    EXPECT_EQ(GetSegmentIndex(converter), 0);
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.focused_index(), 0);
    ASSERT_EQ(candidates.size(), 3);  // two candidates + one t13n sub list.
    EXPECT_EQ(candidates.position(), 0);
    EXPECT_EQ(candidates.candidate(0).value(), kKamabokono);
    EXPECT_EQ(candidates.candidate(1).value(), "カマボコの");
    EXPECT_EQ(candidates.candidate(2).value(), "そのほかの文字種");
  }

  // Test for candidate motion. [CandidateNext]
  converter.SegmentFocusRight();  // Focus to the last segment.
  EXPECT_EQ(GetSegmentIndex(converter), 1);
  converter.CandidateNext(*composer_);
  expected_indices[1] += 1;
  {
    EXPECT_TRUE(IsCandidateListVisible(converter));
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.focused_index(), 1);
    ASSERT_EQ(candidates.size(), 3);  // two candidates + one t13n sub list.
    EXPECT_EQ(candidates.position(), 5);
    EXPECT_EQ(candidates.candidate(0).value(), "陰謀");
    EXPECT_EQ(candidates.candidate(1).value(), "印房");
    EXPECT_EQ(candidates.candidate(2).value(), "そのほかの文字種");

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment(0).value(), kKamabokono);
    EXPECT_EQ(conversion.segment(1).value(), "印房");
  }

  // Test for segment motion again [SegmentFocusLeftEdge] [SegmentFocusLast]
  // The positions of "陰謀" and "印房" should be swapped.
  {
    Segments fixed_segments;
    SetKamaboko(&fixed_segments);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    ASSERT_EQ(fixed_segments.segment(1).candidate(0).value, "陰謀");
    ASSERT_EQ(fixed_segments.segment(1).candidate(1).value, "印房");
    // swap the values.
    fixed_segments.mutable_segment(1)->mutable_candidate(0)->value.swap(
        fixed_segments.mutable_segment(1)->mutable_candidate(1)->value);
    ASSERT_EQ(fixed_segments.segment(1).candidate(0).value, "印房");
    ASSERT_EQ(fixed_segments.segment(1).candidate(1).value, "陰謀");
    EXPECT_CALL(mock_converter, CommitSegmentValue(_, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<0>(fixed_segments), Return(true)));
  }
  converter.SegmentFocusLeftEdge();
  {
    EXPECT_EQ(GetSegmentIndex(converter), 0);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SegmentFocusLast();
    EXPECT_EQ(GetSegmentIndex(converter), 1);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    converter.SetCandidateListVisible(true);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.focused_index(), 0);
    ASSERT_EQ(candidates.size(), 3);  // two candidates + one t13n sub list.
    EXPECT_EQ(candidates.position(), 5);
    EXPECT_EQ(candidates.candidate(0).value(), "印房");

    EXPECT_EQ(candidates.candidate(1).value(), "陰謀");

    EXPECT_EQ(candidates.candidate(2).value(), "そのほかの文字種");

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment(0).value(), kKamabokono);
    EXPECT_EQ(conversion.segment(1).value(), "印房");
  }

  converter.Commit(*composer_, Context::default_instance());
  expected_indices.clear();
  {
    composer_->Reset();
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), "かまぼこの印房");
    EXPECT_EQ(result.key(), "かまぼこのいんぼう");
    EXPECT_FALSE(converter.IsActive());
  }
}

TEST_F(SessionConverterTest, Transliterations) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  composer_->InsertCharacterKeyAndPreedit("h", "く");
  composer_->InsertCharacterKeyAndPreedit("J", "ま");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("くま");
    segment->add_candidate()->value = "クマー";
  }
  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // Move to the t13n list.
  converter.CandidateNext(*composer_);
  expected_indices[0] = -1;
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_TRUE(output.has_candidates());

  const commands::Candidates &candidates = output.candidates();
  EXPECT_EQ(candidates.size(), 2);  // one candidate + one t13n sub list.
  EXPECT_EQ(candidates.focused_index(), 1);
  EXPECT_EQ(candidates.candidate(1).value(), "そのほかの文字種");

  std::vector<std::string> t13ns;
  composer_->GetTransliterations(&t13ns);

  EXPECT_TRUE(candidates.has_subcandidates());
  EXPECT_EQ(candidates.subcandidates().size(), t13ns.size());
  EXPECT_EQ(candidates.subcandidates().candidate_size(), 9);

  for (size_t i = 0; i < candidates.subcandidates().candidate_size(); ++i) {
    EXPECT_EQ(candidates.subcandidates().candidate(i).value(), t13ns[i]);
  }
}

TEST_F(SessionConverterTest, T13NWithResegmentation) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    CHECK(segment);
    segment->set_key("かまぼこの");
    candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->value = "かまぼこの";

    segment = segments.add_segment();
    CHECK(segment);
    segment->set_key("いんぼう");
    candidate = segment->add_candidate();
    CHECK(candidate);
    candidate->value = "いんぼう";

    InsertASCIISequence("kamabokonoinbou", composer_.get());
    FillT13Ns(&segments, composer_.get());
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  EXPECT_TRUE(converter.Convert(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  std::vector<int> expected_indices = {0, 0};
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // Test for segment motion. [SegmentFocusRight]
  converter.SegmentFocusRight();
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  // Shrink segment
  {
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segments.Clear();
    segment = segments.add_segment();

    segment->set_key("かまぼこの");
    candidate = segment->add_candidate();
    candidate->value = "かまぼこの";
    candidate = segment->add_candidate();
    candidate->value = "カマボコの";

    segment = segments.add_segment();
    segment->set_key("いんぼ");
    candidate = segment->add_candidate();
    candidate->value = "インボ";

    segment = segments.add_segment();
    segment->set_key("う");
    candidate = segment->add_candidate();
    candidate->value = "ウ";

    FillT13Ns(&segments, composer_.get());
    EXPECT_CALL(mock_converter, ResizeSegment(_, _, _, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  }
  converter.SegmentWidthShrink(*composer_);
  Mock::VerifyAndClearExpectations(&mock_converter);
  expected_indices.push_back(0);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // Convert to half katakana. Expected index should be 0.
  converter.ConvertToTransliteration(*composer_,
                                     transliteration::HALF_KATAKANA);
  expected_indices[0] = 0;
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    const commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(preedit.segment_size(), 3);
    EXPECT_EQ(preedit.segment(1).value(), "ｲﾝﾎﾞ");
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }
}

TEST_F(SessionConverterTest, ConvertToHalfWidth) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  std::vector<int> expected_indices;
  composer_->InsertCharacterKeyAndPreedit("a", "あ");
  composer_->InsertCharacterKeyAndPreedit("b", "ｂ");
  composer_->InsertCharacterKeyAndPreedit("c", "ｃ");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("あｂｃ");
    segment->add_candidate()->value = "あべし";
  }
  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  expected_indices.push_back(0);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ｱbc");
  }

  // Composition will be transliterated to "ａｂｃ".
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::FULL_ASCII));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ａｂｃ");
  }

  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "abc");
  }

  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ABC");
  }
}

TEST_F(SessionConverterTest, ConvertToHalfWidth2) {
  // http://b/2517514
  // ConvertToHalfWidth converts punctuations differently w/ or w/o kana.
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  composer_->InsertCharacterKeyAndPreedit("q", "ｑ");
  composer_->InsertCharacterKeyAndPreedit(",", "、");
  composer_->InsertCharacterKeyAndPreedit(".", "。");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("ｑ、。");
    segment->add_candidate()->value = "q,.";
    segment->add_candidate()->value = "q､｡";
  }
  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.ConvertToHalfWidth(*composer_));
  std::vector<int> expected_indices;
  expected_indices.push_back(0);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "q､｡");
  }
}

TEST_F(SessionConverterTest, SwitchKanaTypeFromCompositionMode) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  composer_->InsertCharacterKeyAndPreedit("a", "あ");
  composer_->InsertCharacterKeyAndPreedit("b", "ｂ");
  composer_->InsertCharacterKeyAndPreedit("c", "ｃ");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("あｂｃ");
    segment->add_candidate()->value = "あべし";
  }
  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
  std::vector<int> expected_indices = {0};
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "アｂｃ");
  }

  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ｱbc");
  }

  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "あｂｃ");
  }
}

TEST_F(SessionConverterTest, SwitchKanaTypeFromConversionMode) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  composer_->EditErase();
  composer_->InsertCharacterKeyAndPreedit("ka", "か");
  composer_->InsertCharacterKeyAndPreedit("n", "ん");
  composer_->InsertCharacterKeyAndPreedit("ji", "じ");

  Segments segments;
  {  // Initialize segments.
    Segment *segment = segments.add_segment();
    segment->set_key("かんじ");
    segment->add_candidate()->value = "漢字";
  }
  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Convert(*composer_));
  std::vector<int> expected_indices = {0};
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Make sure the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "漢字");
  }

  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "かんじ");
  }

  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "カンジ");
  }

  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "ｶﾝｼﾞ");
  }

  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "かんじ");
  }
}

TEST_F(SessionConverterTest, ResizeSegmentFailedInSwitchKanaType) {
  const MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  // ResizeSegment() is called when the conversion result has multiple segments.
  // Let the underlying converter return the result with two segments.
  Segments segments;
  AddSegmentWithSingleCandidate(&segments, "かな", "カナ");
  AddSegmentWithSingleCandidate(&segments, "たいぷ", "タイプ");
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  // Suppose that ResizeSegment() fails for "かな|たいぷ" (UTF8-length is 5).
  EXPECT_CALL(mock_converter, ResizeSegment(_, _, 0, 5))
      .WillOnce(Return(false));

  // FocusSegmentValue() is called in the last step.
  EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 0))
      .WillOnce(Return(true));

  // Calling SwitchKanaType() with the above set up doesn't crash.
  EXPECT_TRUE(converter.SwitchKanaType(*composer_));
}

TEST_F(SessionConverterTest, CommitFirstSegment) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  SetKamaboko(&segments);
  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";

  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  EXPECT_TRUE(converter.Convert(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  std::vector<int> expected_indices = {0, 0};
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment(0).value(), kKamabokono);
    EXPECT_EQ(conversion.segment(1).value(), "陰謀");
  }

  EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 1))
      .WillOnce(Return(true));
  converter.CandidateNext(*composer_);
  Mock::VerifyAndClearExpectations(&mock_converter);
  expected_indices[0] += 1;
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment(0).value(), "カマボコの");
    EXPECT_EQ(conversion.segment(1).value(), "陰謀");
  }

  {  // Initialization of CommitSegments.
    Segments segments_after_submit;
    Segment *segment = segments_after_submit.add_segment();
    segment->set_key("いんぼう");
    segment->add_candidate()->value = "陰謀";
    segment->add_candidate()->value = "印房";
    EXPECT_CALL(mock_converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  }
  size_t size;
  converter.CommitFirstSegment(*composer_, Context::default_instance(), &size);
  expected_indices.erase(expected_indices.begin(),
                         expected_indices.begin() + 1);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_EQ(size, Util::CharsLen(kKamabokono));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromConversion", 1);
  EXPECT_STATS_NOT_EXIST("ConversionCandidates0");
  EXPECT_COUNT_STATS("ConversionCandidates1", 1);
}

TEST_F(SessionConverterTest, CommitHeadToFocusedSegments) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  const std::string kIberiko = "いべりこ";
  const std::string kNekowo = "ねこを";
  const std::string kItadaita = "いただいた";
  {  // Three segments as the result of conversion.
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key(kIberiko);
    candidate = segment->add_candidate();
    candidate->value = "イベリコ";

    segment = segments.add_segment();
    segment->set_key(kNekowo);
    candidate = segment->add_candidate();
    candidate->value = "猫を";

    segment = segments.add_segment();
    segment->set_key(kItadaita);
    candidate = segment->add_candidate();
    candidate->value = "頂いた";

    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  composer_->InsertCharacterPreedit(kIberiko + kNekowo + kItadaita);
  EXPECT_TRUE(converter.Convert(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  // Here [イベリコ]|猫を|頂いた

  EXPECT_CALL(mock_converter, CommitSegmentValue(_, 0, 0))
      .WillOnce(Return(true));
  converter.SegmentFocusRight();
  // Here イベリコ|[猫を]|頂いた

  {  // Initialization of CommitSegments.
    Segments segments;
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key(kItadaita);
    candidate = segment->add_candidate();
    candidate->value = "頂いた";
    EXPECT_CALL(mock_converter, CommitSegments(_, _))
        .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  }
  size_t size;
  converter.CommitHeadToFocusedSegments(*composer_, Context::default_instance(),
                                        &size);
  // Here 頂いた
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_EQ(size, Util::CharsLen(kIberiko + kNekowo));
  EXPECT_TRUE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitHeadToFocusedSegmentsAtLastSegment) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  Segments segments;
  SetKamaboko(&segments);
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";

  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  EXPECT_TRUE(converter.Convert(*composer_));
  // Here [かまぼこの]|陰謀

  converter.SegmentFocusRight();
  // Here かまぼこの|[陰謀]

  size_t size;
  // All the segments should be committed.
  converter.CommitHeadToFocusedSegments(*composer_, Context::default_instance(),
                                        &size);
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_EQ(size, 0);
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitConvertedBracketPairText) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  const std::string kKakko = "かっこ";

  {  // Initialize segments.
    segments.Clear();
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kKakko);
    candidate = segment->add_candidate();
    candidate->value = "（）";
    candidate->key = kKakko;
    candidate->content_key = kKakko;
    candidate = segment->add_candidate();
    candidate->value = "「」";
    candidate->key = kKakko;
    candidate->content_key = kKakko;
  }

  composer_->InsertCharacterPreedit(kKakko);

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  ASSERT_TRUE(converter.Suggest(*composer_));
  std::vector<int> expected_indices = {0};
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(preedit.segment_size(), 1);
    EXPECT_EQ(preedit.segment(0).value(), kKakko);

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_EQ(candidates.candidate(0).value(), "（）");
    EXPECT_FALSE(candidates.has_focused_index());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_CALL(mock_converter, CommitSegmentValue(_, 0, 1))
      .WillOnce(Return(true));
  // FinishConversion is expected to return empty Segments.
  EXPECT_CALL(mock_converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(Segments()));

  size_t committed_key_size = 0;
  converter.CommitSuggestionByIndex(1, *composer_, Context::default_instance(),
                                    &committed_key_size);
  expected_indices.clear();
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
  EXPECT_EQ(committed_key_size, SessionConverter::kConsumedAllCharacters);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), "「」");
    EXPECT_EQ(result.key(), kKakko);
    EXPECT_EQ(result.cursor_offset(), -1);
    EXPECT_EQ(GetState(converter), SessionConverterInterface::COMPOSITION);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, CommitPreedit) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  std::vector<int> expected_indices;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  converter.CommitPreedit(*composer_, Context::default_instance());
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), kChars_Aiueo);
    EXPECT_EQ(result.key(), kChars_Aiueo);
  }
  EXPECT_FALSE(converter.IsActive());

  EXPECT_COUNT_STATS("Commit", 1);
  EXPECT_COUNT_STATS("CommitFromComposition", 1);
}

TEST_F(SessionConverterTest, CommitPreeditBracketPairText) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  std::vector<int> expected_indices;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  composer_->InsertCharacterPreedit("（）");
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  converter.CommitPreedit(*composer_, Context::default_instance());
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), "（）");
    EXPECT_EQ(result.key(), "（）");
    EXPECT_EQ(result.cursor_offset(), -1);
  }

  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, ClearSegmentsBeforeSuggest) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  // Call Suggest() and sets the segments of converter to the following one.
  const Segments &segments = GetSegmentsTest();
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  composer_->InsertCharacterPreedit("てすと");
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);

  // Then, call Suggest() again. It should be called with the brandnew segments.
  Segments empty;
  empty.set_max_history_segments_size(
      converter.conversion_preferences().max_history_size);
  EXPECT_CALL(mock_converter,
              StartSuggestionForRequest(_, Pointee(EqualsSegments(empty))))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
}

TEST_F(SessionConverterTest, PredictIsNotCalledInPredictionState) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  // Call Predict() and sets the segments of converter to the following one. By
  // calling Predict(), converter enters PREDICTION state.
  const Segments &segments = GetSegmentsTest();
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  composer_->InsertCharacterPreedit("てすと");
  EXPECT_TRUE(converter.Predict(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);

  // Then, call Predict() again. PredictForRequest() is not called.
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _)).Times(0);
  EXPECT_TRUE(converter.Predict(*composer_));
}

TEST_F(SessionConverterTest, CommitSuggestionByIndex) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->key = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->key = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  ASSERT_TRUE(converter.Suggest(*composer_));
  std::vector<int> expected_indices = {0};
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &preedit = output.preedit();
    EXPECT_EQ(preedit.segment_size(), 1);
    EXPECT_EQ(preedit.segment(0).value(), kChars_Mo);

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_EQ(candidates.candidate(0).value(), kChars_Mozukusu);
    EXPECT_FALSE(candidates.has_focused_index());
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_CALL(mock_converter, CommitSegmentValue(_, 0, 1))
      .WillOnce(Return(true));
  // FinishConversion is expected to return empty Segments.
  EXPECT_CALL(mock_converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(Segments()));

  size_t committed_key_size = 0;
  converter.CommitSuggestionByIndex(1, *composer_, Context::default_instance(),
                                    &committed_key_size);
  expected_indices.clear();
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
  EXPECT_EQ(committed_key_size, SessionConverter::kConsumedAllCharacters);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), kChars_Momonga);
    EXPECT_EQ(result.key(), kChars_Momonga);
    EXPECT_EQ(GetState(converter), SessionConverterInterface::COMPOSITION);
    EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  }

  EXPECT_COUNT_STATS("Commit", 1);
  // Suggestion is counted as Prediction
  EXPECT_COUNT_STATS("CommitFromPrediction", 1);
  EXPECT_COUNT_STATS("PredictionCandidates1", 1);
}

TEST_F(SessionConverterTest, CommitSuggestionById) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->key = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->key = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);

  std::vector<int> expected_indices = {0};
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // FinishConversion is expected to return empty Segments.
  constexpr int kCandidateIndex = 1;
  EXPECT_CALL(mock_converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(Segments()));
  EXPECT_CALL(mock_converter, CommitSegmentValue(_, 0, kCandidateIndex))
      .WillOnce(DoAll(SetArgPointee<0>(segments), Return(true)));
  size_t committed_key_size = 0;
  converter.CommitSuggestionById(kCandidateIndex, *composer_,
                                 Context::default_instance(),
                                 &committed_key_size);
  Mock::VerifyAndClearExpectations(&mock_converter);
  expected_indices.clear();
  composer_->Reset();
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
  EXPECT_EQ(committed_key_size, SessionConverter::kConsumedAllCharacters);
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), kChars_Momonga);
    EXPECT_EQ(result.key(), kChars_Momonga);
    EXPECT_EQ(GetState(converter), SessionConverterInterface::COMPOSITION);
  }

  EXPECT_COUNT_STATS("Commit", 1);
  // Suggestion is counted as Prediction.
  EXPECT_COUNT_STATS("CommitFromPrediction", 1);
  EXPECT_COUNT_STATS("PredictionCandidates" + std::to_string(kCandidateIndex),
                     1);
}

TEST_F(SessionConverterTest, PartialPrediction) {
  RequestForUnitTest::FillMobileRequest(request_.get());
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments1, segments2;
  Segments suggestion_segments;
  const std::string kChars_Kokode = "ここで";
  const std::string kChars_Hakimonowo = "はきものを";

  {  // Initialize mock segments for partial prediction
    Segment *segment = segments1.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Kokode);
    candidate = segment->add_candidate();
    candidate->value = "此処では";
    candidate->key = kChars_Kokode;
    candidate->content_key = kChars_Kokode;
    candidate->attributes = Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    candidate->consumed_key_size = Util::CharsLen(kChars_Kokode);
  }

  // Suggestion that matches to the same key by its prefix.
  // Should not be used by partial prediction.
  {
    Segment *segment = suggestion_segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Kokode);
    candidate = segment->add_candidate();
    candidate->value = "ここでは着物を";
    candidate->key = "ここではきものを";
    candidate->content_key = candidate->key;
    candidate = segment->add_candidate();
  }

  {  // Initialize mock segments for prediction
    Segment *segment = segments2.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Hakimonowo);
    candidate = segment->add_candidate();
    candidate->value = "此処では";
    candidate->key = kChars_Hakimonowo;
    candidate->content_key = kChars_Hakimonowo;
  }

  // "ここではきものを|"    ("|" is cursor position)
  composer_->InsertCharacterPreedit(kChars_Kokode + kChars_Hakimonowo);
  composer_->MoveCursorToEnd();
  // Prediction for "ここではきものを".
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(suggestion_segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  std::vector<int> expected_indices = {0};
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // "|ここではきものを"    ("|" is cursor position)
  composer_->MoveCursorTo(0);

  // Prediction for "ここではきものを".
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(suggestion_segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // "ここで|はきものを"    ("|" is cursor position)
  composer_->MoveCursorTo(3);

  // Partial prediction for "ここで"
  EXPECT_CALL(mock_converter, StartPartialPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments1), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // commit partial suggestion
  size_t committed_key_size = 0;
  EXPECT_CALL(mock_converter,
              CommitPartialSuggestionSegmentValue(_, _, _, _, _))
      .WillOnce(DoAll(SetArgPointee<0>(segments2), Return(true)));
  converter.CommitSuggestionById(0, *composer_, Context::default_instance(),
                                 &committed_key_size);
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_EQ(committed_key_size, Util::CharsLen(kChars_Kokode));
  // Indices should be {0} since there is another segment.
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the result
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), "此処では");
    EXPECT_EQ(result.key(), kChars_Kokode);
    EXPECT_EQ(GetState(converter), SessionConverterInterface::SUGGESTION);
  }

  EXPECT_COUNT_STATS("Commit", 1);
  // Suggestion is counted as Prediction.
  EXPECT_COUNT_STATS("CommitFromPrediction", 1);
  EXPECT_COUNT_STATS("PredictionCandidates0", 1);
}

TEST_F(SessionConverterTest, SuggestAndPredict) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  {  // Initialize mock segments for suggestion
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  std::vector<int> expected_indices = {0};
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_TRUE(output.candidates().has_footer());
#if defined(CHANNEL_DEV) && defined(GOOGLE_JAPANESE_INPUT_BUILD)
    EXPECT_FALSE(output.candidates().footer().has_label());
    EXPECT_TRUE(output.candidates().footer().has_sub_label());
#else   // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD
    EXPECT_TRUE(output.candidates().footer().has_label());
    EXPECT_FALSE(output.candidates().footer().has_sub_label());
#endif  // CHANNEL_DEV && GOOGLE_JAPANESE_INPUT_BUILD
    EXPECT_FALSE(output.candidates().footer().index_visible());
    EXPECT_FALSE(output.candidates().footer().logo_visible());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_EQ(candidates.candidate(0).value(), kChars_Mozukusu);
    EXPECT_FALSE(candidates.has_focused_index());
  }

  // Since Suggest() was called, the converter stores its results internally. In
  // this case, the prediction is not triggered.
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _)).Times(0);
  EXPECT_TRUE(converter.Predict(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_FALSE(output.candidates().footer().has_label());
    EXPECT_TRUE(output.candidates().footer().index_visible());
    EXPECT_TRUE(output.candidates().footer().logo_visible());

    // Check the conversion
    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), kChars_Mozukusu);

    // Check the candidate list
    const commands::Candidates &candidates = output.candidates();
    // Candidates should be the same as suggestion
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_EQ(candidates.candidate(0).value(), kChars_Mozukusu);
    EXPECT_EQ(candidates.candidate(1).value(), kChars_Momonga);
    EXPECT_TRUE(candidates.has_focused_index());
    EXPECT_EQ(candidates.focused_index(), 0);
  }

  EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 1))
      .WillOnce(Return(true));
  converter.CandidateNext(*composer_);
  Mock::VerifyAndClearExpectations(&mock_converter);

  // Prediction is called
  EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 2))
      .WillOnce(Return(true));
  Segments mondrian_segments;
  {  // Initialize mock segments for prediction
    Segment *segment = mondrian_segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
    candidate = segment->add_candidate();
    candidate->value = "モンドリアン";
    candidate->content_key = "もんどりあん";
  }
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(mondrian_segments), Return(true)));
  converter.CandidateNext(*composer_);
  Mock::VerifyAndClearExpectations(&mock_converter);
  expected_indices[0] += 2;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    // Candidates should be merged with the previous suggestions.
    EXPECT_EQ(candidates.size(), 4);
    EXPECT_EQ(candidates.candidate(0).value(), kChars_Mozukusu);
    EXPECT_EQ(candidates.candidate(1).value(), kChars_Momonga);
    EXPECT_EQ(candidates.candidate(2).value(), kChars_Mozuku);
    EXPECT_EQ(candidates.candidate(3).value(), "モンドリアン");
    EXPECT_TRUE(candidates.has_focused_index());
  }

  // Select to "モンドリアン".
  EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 4))
      .WillOnce(Return(true));
  converter.CandidateNext(*composer_);
  Mock::VerifyAndClearExpectations(&mock_converter);
  expected_indices[0] += 1;
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);

  // Commit "モンドリアン".
  EXPECT_CALL(mock_converter, CommitSegmentValue(_, 0, 4))
      .WillOnce(Return(true));
  EXPECT_CALL(mock_converter, FinishConversion(_, _));
  converter.Commit(*composer_, Context::default_instance());
  Mock::VerifyAndClearExpectations(&mock_converter);
  composer_->Reset();
  expected_indices.clear();
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {  // Check the submitted value
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Result &result = output.result();
    EXPECT_EQ(result.value(), "モンドリアン");
    EXPECT_EQ(result.key(), "もんどりあん");
  }

  // After commit, the state should be reset. Thus, calling prediction before
  // suggestion should trigger StartPredictionForRequest().
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(mondrian_segments), Return(true)));
  expected_indices.push_back(0);
  EXPECT_TRUE(converter.Predict(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(converter.IsActive());
  EXPECT_SELECTED_CANDIDATE_INDICES_EQ(converter, expected_indices);
  {
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    // Check the conversion
    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), kChars_Mozuku);

    // Check the candidate list
    const commands::Candidates &candidates = output.candidates();
    // Candidates should NOT be merged with the previous suggestions.
    EXPECT_EQ(candidates.size(), 3);
    EXPECT_EQ(candidates.candidate(0).value(), kChars_Mozuku);
    EXPECT_EQ(candidates.candidate(1).value(), kChars_Momonga);
    EXPECT_EQ(candidates.candidate(2).value(), "モンドリアン");
    EXPECT_TRUE(candidates.has_focused_index());
  }
}

TEST_F(SessionConverterTest, SuggestFillIncognitoCandidateWords) {
  Segments segments;
  {  // Initialize mock segments for suggestion
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // A matcher to test if the given conversion request sets incognito_mode().
  constexpr auto IsIncognitoConversionRequest = [](bool is_incognito) {
    return Property(&ConversionRequest::config,
                    Property(&Config::incognito_mode, is_incognito));
  };
  {
    request_->set_fill_incognito_candidate_words(false);
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(
                                    IsIncognitoConversionRequest(false), _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    EXPECT_TRUE(converter.Suggest(*composer_));
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_FALSE(output.has_incognito_candidate_words());
  }
  {
    request_->set_fill_incognito_candidate_words(true);
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(
                                    IsIncognitoConversionRequest(false), _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(
                                    IsIncognitoConversionRequest(true), _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    EXPECT_TRUE(converter.Suggest(*composer_));
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_TRUE(output.has_incognito_candidate_words());
  }
}

TEST_F(SessionConverterTest, OnePhaseSuggestion) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  request_->set_mixed_conversion(true);
  Segments segments;
  {  // Initialize mock segments for suggestion (internally prediction)
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
    candidate = segment->add_candidate();
    candidate->value = "モンドリアン";
    candidate->content_key = "もんどりあん";
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion (internally prediction)
  // Use "prediction" mock as this suggestion uses prediction internally.
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.size(), 3);
    EXPECT_EQ(candidates.candidate(0).value(), kChars_Mozuku);
    EXPECT_EQ(candidates.candidate(1).value(), kChars_Momonga);
    EXPECT_EQ(candidates.candidate(2).value(), "モンドリアン");
    EXPECT_FALSE(candidates.has_focused_index());
  }
}

TEST_F(SessionConverterTest, SuppressSuggestionWhenNotRequested) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _)).Times(0);
  // No candidates should be visible because we are on password field.

  ConversionPreferences conversion_preferences =
      converter.conversion_preferences();
  conversion_preferences.request_suggestion = false;
  EXPECT_FALSE(
      converter.SuggestWithPreferences(*composer_, conversion_preferences));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, SuppressSuggestionOnPasswordField) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  composer_->SetInputFieldType(Context::PASSWORD);
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _)).Times(0);

  // No candidates should be visible because we are on password field.
  EXPECT_FALSE(converter.Suggest(*composer_));
  EXPECT_FALSE(IsCandidateListVisible(converter));
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, AppendCandidateList) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  SetState(SessionConverterInterface::CONVERSION, &converter);
  converter.set_use_cascading_window(true);
  Segments segments;

  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());

    SetSegments(segments, &converter);
    AppendCandidateList(ConversionRequest::CONVERSION, &converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    // 3 == hiragana cand, katakana cand and sub candidate list.
    EXPECT_EQ(candidate_list.size(), 3);
    EXPECT_TRUE(candidate_list.focused());
    size_t sub_cand_list_count = 0;
    for (size_t i = 0; i < candidate_list.size(); ++i) {
      if (candidate_list.candidate(i).HasSubcandidateList()) {
        ++sub_cand_list_count;
      }
    }
    // Sub candidate list for T13N.
    EXPECT_EQ(sub_cand_list_count, 1);
  }
  {
    Segment *segment = segments.mutable_conversion_segment(0);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "あいうえお_2";
    // New meta candidates.
    // They should be ignored.
    std::vector<Segment::Candidate> *meta_candidates =
        segment->mutable_meta_candidates();
    meta_candidates->clear();
    meta_candidates->resize(1);
    meta_candidates->at(0).value = "t13nValue";
    meta_candidates->at(0).content_value = "t13nValue";
    meta_candidates->at(0).content_key = segment->key();

    SetSegments(segments, &converter);
    AppendCandidateList(ConversionRequest::CONVERSION, &converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    // 4 == hiragana cand, katakana cand, hiragana cand2
    // and sub candidate list.
    EXPECT_EQ(candidate_list.size(), 4);
    EXPECT_TRUE(candidate_list.focused());
    size_t sub_cand_list_count = 0;
    std::set<int> id_set;
    for (size_t i = 0; i < candidate_list.size(); ++i) {
      if (candidate_list.candidate(i).HasSubcandidateList()) {
        ++sub_cand_list_count;
      } else {
        // No duplicate ids are expected.
        int id = candidate_list.candidate(i).id();
        // EXPECT_EQ(iterator, iterator) might cause compile error in specific
        // environment.
        EXPECT_TRUE(id_set.end() == id_set.find(id));
        id_set.insert(id);
      }
    }
    // Sub candidate list shouldn't be duplicated.
    EXPECT_EQ(sub_cand_list_count, 1);
  }
}

TEST_F(SessionConverterTest, AppendCandidateListForRequestTypes) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  SetState(SessionConverterInterface::SUGGESTION, &converter);
  Segments segments;

  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    SetSegments(segments, &converter);
    AppendCandidateList(ConversionRequest::SUGGESTION, &converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    EXPECT_FALSE(candidate_list.focused());
  }

  segments.Clear();
  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    SetSegments(segments, &converter);
    AppendCandidateList(ConversionRequest::PARTIAL_SUGGESTION, &converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    EXPECT_FALSE(candidate_list.focused());
  }

  segments.Clear();
  {
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    SetSegments(segments, &converter);
    AppendCandidateList(ConversionRequest::PARTIAL_PREDICTION, &converter);
    const CandidateList &candidate_list = GetCandidateList(converter);
    EXPECT_FALSE(candidate_list.focused());
  }
}

TEST_F(SessionConverterTest, ReloadConfig) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  composer_->InsertCharacterPreedit("aiueo");
  EXPECT_TRUE(converter.Convert(*composer_));
  converter.SetCandidateListVisible(true);

  {  // Set OperationPreferences
    converter.set_use_cascading_window(false);
    converter.set_selection_shortcut(config::Config::SHORTCUT_123456789);
    EXPECT_TRUE(IsCandidateListVisible(converter));
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.candidate(0).annotation().shortcut(), "1");
    EXPECT_EQ(candidates.candidate(1).annotation().shortcut(), "2");
  }

  {  // Set OperationPreferences #2
    converter.set_use_cascading_window(false);
    converter.set_selection_shortcut(config::Config::NO_SHORTCUT);
  }
  {  // Check the config update
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_TRUE(candidates.candidate(0).annotation().shortcut().empty());
    EXPECT_TRUE(candidates.candidate(1).annotation().shortcut().empty());
  }
}

TEST_F(SessionConverterTest, OutputAllCandidateWords) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  SetKamaboko(&segments);
  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);
  FillT13Ns(&segments, composer_.get());

  commands::Output output;

  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Convert(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_FALSE(IsCandidateListVisible(converter));

    output.Clear();
    converter.PopOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(output.all_candidate_words().focused_index(), 0);
    EXPECT_EQ(output.all_candidate_words().category(), commands::CONVERSION);
    // [ "かまぼこの", "カマボコの", "カマボコノ" (t13n), "かまぼこの" (t13n),
    //   "ｶﾏﾎﾞｺﾉ" (t13n) ]
    EXPECT_EQ(output.all_candidate_words().candidates_size(), 5);
  }

  EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 1))
      .WillOnce(Return(true));
  converter.CandidateNext(*composer_);
  Mock::VerifyAndClearExpectations(&mock_converter);
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_TRUE(IsCandidateListVisible(converter));

    output.Clear();
    converter.PopOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(output.all_candidate_words().focused_index(), 1);
    EXPECT_EQ(output.all_candidate_words().category(), commands::CONVERSION);
    // [ "かまぼこの", "カマボコの", "カマボコノ" (t13n), "かまぼこの" (t13n),
    //   "ｶﾏﾎﾞｺﾉ" (t13n) ]
    EXPECT_EQ(output.all_candidate_words().candidates_size(), 5);
  }

  EXPECT_CALL(mock_converter, CommitSegmentValue(_, 0, 1))
      .WillOnce(Return(true));
  converter.SegmentFocusRight();
  Mock::VerifyAndClearExpectations(&mock_converter);
  {
    ASSERT_TRUE(converter.IsActive());
    EXPECT_FALSE(IsCandidateListVisible(converter));

    output.Clear();
    converter.PopOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_TRUE(output.has_all_candidate_words());

    EXPECT_EQ(output.all_candidate_words().focused_index(), 0);
    EXPECT_EQ(output.all_candidate_words().category(), commands::CONVERSION);
    // [ "陰謀", "印房", "インボウ" (t13n), "いんぼう" (t13n), "ｲﾝﾎﾞｳ" (t13n) ]
    EXPECT_EQ(output.all_candidate_words().candidates_size(), 5);
  }
}

TEST_F(SessionConverterTest, GetPreeditAndGetConversion) {
  Segments segments;

  Segment *segment;
  Segment::Candidate *candidate;

  segment = segments.add_segment();
  segment->set_segment_type(Segment::HISTORY);
  segment->set_key("[key:history1]");
  candidate = segment->add_candidate();
  candidate->content_key = "[content_key:history1-1]";
  candidate = segment->add_candidate();
  candidate->content_key = "[content_key:history1-2]";

  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("[key:conversion1]");
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion1-1]";
  candidate->content_key = "[content_key:conversion1-1]";
  candidate->value = "[value:conversion1-1]";
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion1-2]";
  candidate->content_key = "[content_key:conversion1-2]";
  candidate->value = "[value:conversion1-2]";
  {
    // PREDICTION
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 1))
        .WillOnce(Return(true));
    converter.Predict(*composer_);
    converter.CandidateNext(*composer_);
    std::string preedit;
    GetPreedit(converter, 0, 1, &preedit);
    EXPECT_EQ(preedit, "[content_key:conversion1-2]");
    std::string conversion;
    GetConversion(converter, 0, 1, &conversion);
    EXPECT_EQ(conversion, "[value:conversion1-2]");
  }
  {
    // SUGGESTION
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    converter.Suggest(*composer_);
    std::string preedit;
    GetPreedit(converter, 0, 1, &preedit);
    EXPECT_EQ(preedit, "[content_key:conversion1-1]");
    std::string conversion;
    GetConversion(converter, 0, 1, &conversion);
    EXPECT_EQ(conversion, "[value:conversion1-1]");
  }
  segment = segments.add_segment();
  segment->set_segment_type(Segment::FREE);
  segment->set_key("[key:conversion2]");
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion2-1]";
  candidate->content_key = "[content_key:conversion2-1]";
  candidate->value = "[value:conversion2-1]";
  candidate = segment->add_candidate();
  candidate->key = "[key:conversion2-2]";
  candidate->content_key = "[content_key:conversion2-2]";
  candidate->value = "[value:conversion2-2]";
  {
    // CONVERSION
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 1))
        .WillOnce(Return(true));
    converter.Convert(*composer_);
    converter.CandidateNext(*composer_);
    std::string preedit;
    GetPreedit(converter, 0, 2, &preedit);
    EXPECT_EQ(preedit, "[key:conversion1][key:conversion2]");
    std::string conversion;
    GetConversion(converter, 0, 2, &conversion);
    EXPECT_EQ(conversion, "[value:conversion1-2][value:conversion2-1]");
  }
}

TEST_F(SessionConverterTest, GetAndSetSegments) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;

  // Set history segments.
  const std::string kHistoryInput[] = {"車で", "行く"};
  for (size_t i = 0; i < std::size(kHistoryInput); ++i) {
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kHistoryInput[i];
  }
  EXPECT_CALL(mock_converter, FinishConversion(_, _))
      .WillOnce(SetArgPointee<1>(segments));
  converter.CommitPreedit(*composer_, Context::default_instance());

  Segments src;
  GetSegments(converter, &src);
  ASSERT_EQ(src.history_segments_size(), 2);
  EXPECT_EQ(src.history_segment(0).candidate(0).value, "車で");
  EXPECT_EQ(src.history_segment(1).candidate(0).value, "行く");

  src.mutable_history_segment(0)->mutable_candidate(0)->value = "歩いて";
  Segment *segment = src.add_segment();
  segment->set_segment_type(Segment::FREE);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "?";

  SetSegments(src, &converter);

  Segments dest;
  GetSegments(converter, &dest);

  ASSERT_EQ(dest.history_segments_size(), 2);
  ASSERT_EQ(dest.conversion_segments_size(), 1);
  EXPECT_EQ(dest.history_segment(0).candidate(0).value,
            src.history_segment(0).candidate(0).value);
  EXPECT_EQ(dest.history_segment(1).candidate(0).value,
            src.history_segment(1).candidate(0).value);
  EXPECT_EQ(dest.history_segment(2).candidate(0).value,
            src.history_segment(2).candidate(0).value);
}

TEST_F(SessionConverterTest, Clone) {
  MockConverter mock_converter;
  SessionConverter src(&mock_converter, request_.get(), config_.get());

  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";
  const std::string kInbouKanji = "陰謀";

  {  // create source converter
    Segments segments;
    SetKamaboko(&segments);

    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));

    src.set_use_cascading_window(false);
    src.set_selection_shortcut(config::Config::SHORTCUT_123456789);
  }

  {  // validation
    // Copy and validate
    std::unique_ptr<SessionConverter> dest(src.Clone());
    ASSERT_TRUE(dest.get() != nullptr);
    ExpectSameSessionConverter(src, *dest);

    // Convert source
    EXPECT_TRUE(src.Convert(*composer_));
    EXPECT_TRUE(src.IsActive());

    // Convert destination and validate
    EXPECT_TRUE(dest->Convert(*composer_));
    ExpectSameSessionConverter(src, *dest);

    // Copy converted and validate
    dest.reset(src.Clone());
    ASSERT_TRUE(dest.get() != nullptr);
    ExpectSameSessionConverter(src, *dest);
  }
}

// Suggest() in the suggestion state was not accepted.  (http://b/1948334)
TEST_F(SessionConverterTest, Issue1948334) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  {  // Initialize mock segments for the first suggestion
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }
  composer_->InsertCharacterPreedit(kChars_Mo);

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(converter.IsActive());

  segments.Clear();
  {  // Initialize mock segments for the second suggestion
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key("もず");
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozukusu;
    candidate->content_key = kChars_Mozukusu;
  }
  composer_->InsertCharacterPreedit("もず");

  // Suggestion
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Suggest(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(converter.IsActive());

  {  // Check the candidate list
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    // Candidates should be merged with the previous suggestions.
    EXPECT_EQ(candidates.size(), 1);
    EXPECT_EQ(candidates.candidate(0).value(), kChars_Mozukusu);
    EXPECT_FALSE(candidates.has_focused_index());
  }
}

TEST_F(SessionConverterTest, Issue1960362) {
  // Testcase against http://b/1960362, a candidate list was not
  // updated when ConvertToTransliteration changed the size of segments.

  table_->AddRule("zyu", "ZYU", "");
  table_->AddRule("jyu", "ZYU", "");
  table_->AddRule("tt", "XTU", "t");
  table_->AddRule("ta", "TA", "");

  composer_->InsertCharacter("j");
  composer_->InsertCharacter("y");
  composer_->InsertCharacter("u");
  composer_->InsertCharacter("t");

  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  Segments segments;
  {
    Segment *segment;
    Segment::Candidate *candidate;

    segment = segments.add_segment();
    segment->set_key("ZYU");
    candidate = segment->add_candidate();
    candidate->value = "[ZYU]";
    candidate->content_key = "[ZYU]";

    segment = segments.add_segment();
    segment->set_key("t");
    candidate = segment->add_candidate();
    candidate->value = "[t]";
    candidate->content_key = "[t]";
  }

  Segments resized_segments;
  {
    Segment *segment = resized_segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key("ZYUt");
    candidate = segment->add_candidate();
    candidate->value = "[ZYUt]";
    candidate->content_key = "[ZYUt]";
  }
  FillT13Ns(&segments, composer_.get());
  FillT13Ns(&resized_segments, composer_.get());

  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_CALL(mock_converter, ResizeSegment(_, _, _, _))
      .WillRepeatedly(DoAll(SetArgPointee<0>(resized_segments), Return(true)));
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HALF_ASCII));
  EXPECT_FALSE(IsCandidateListVisible(converter));

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
  EXPECT_TRUE(output.has_preedit());
  EXPECT_FALSE(output.has_candidates());

  const commands::Preedit &conversion = output.preedit();
  EXPECT_EQ(conversion.segment(0).value(), "jyut");
}

TEST_F(SessionConverterTest, Issue1978201) {
  // This is a unittest against http://b/1978201
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  composer_->InsertCharacterPreedit(kChars_Mo);

  {  // Initialize mock segments for prediction
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate;
    segment->set_key(kChars_Mo);
    candidate = segment->add_candidate();
    candidate->value = kChars_Mozuku;
    candidate->content_key = kChars_Mozuku;
    candidate = segment->add_candidate();
    candidate->value = kChars_Momonga;
    candidate->content_key = kChars_Momonga;
  }

  // Prediction
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), kChars_Mozuku);
  }

  // Meaningless segment manipulations.
  converter.SegmentWidthShrink(*composer_);
  converter.SegmentFocusLeft();
  converter.SegmentFocusLast();

  {  // Check the conversion again
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), kChars_Mozuku);
  }
}

TEST_F(SessionConverterTest, Issue1981020) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  // "〜〜〜〜" U+301C * 4
  const std::string wave_dash_301c = "〜〜〜〜";
  composer_->InsertCharacterPreedit(wave_dash_301c);
  Segments segments;
  EXPECT_CALL(mock_converter, FinishConversion(_, _))
      .WillOnce(SaveArgPointee<1>(&segments));
  converter.CommitPreedit(*composer_, Context::default_instance());

#ifdef _WIN32
  // "～～～～" U+FF5E * 4
  const std::string fullwidth_tilde_ff5e = "～～～～";
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
            fullwidth_tilde_ff5e);
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).content_value,
            fullwidth_tilde_ff5e);
#else   // _WIN32
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, wave_dash_301c);
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).content_value,
            wave_dash_301c);
#endif  // _WIN32
}

TEST_F(SessionConverterTest, Issue2029557) {
  // Unittest against http://b/2029557
  // a<tab><F6> raised a DCHECK error.
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  // Composition (as "a")
  composer_->InsertCharacterPreedit("a");

  // Prediction (as <tab>)
  Segments segments;
  SetAiueo(&segments);
  EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.Predict(*composer_));
  EXPECT_TRUE(converter.IsActive());

  // Transliteration (as <F6>)
  segments.Clear();
  Segment *segment = segments.add_segment();
  segment->set_key("a");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->value = "a";

  FillT13Ns(&segments, composer_.get());
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  EXPECT_TRUE(converter.ConvertToTransliteration(*composer_,
                                                 transliteration::HIRAGANA));
  EXPECT_TRUE(converter.IsActive());
}

TEST_F(SessionConverterTest, Issue2031986) {
  // Unittest against http://b/2031986
  // aaaaa<Shift+Enter> raised a CRT error.
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  {  // Initialize a suggest result triggered by "aaaa".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("aaaa");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "AAAA";
    candidate = segment->add_candidate();
    candidate->value = "Aaaa";
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  // Get suggestion
  composer_->InsertCharacterPreedit("aaaa");
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(converter.IsActive());

  {  // Initialize no suggest result triggered by "aaaaa".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("aaaaa");
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(false)));
  }
  // Hide suggestion
  composer_->InsertCharacterPreedit("a");
  EXPECT_FALSE(converter.Suggest(*composer_));
  EXPECT_FALSE(converter.IsActive());
}

TEST_F(SessionConverterTest, Issue2040116) {
  // Unittest against http://b/2040116
  //
  // It happens when the first Predict returns results but the next
  // MaybeExpandPrediction does not return any results.  That's a
  // trick by GoogleSuggest.
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  composer_->InsertCharacterPreedit("G");

  {
    // Initialize no predict result.
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(false)));
  }
  // Get prediction
  EXPECT_FALSE(converter.Predict(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_FALSE(converter.IsActive());

  {
    // Initialize a suggest result triggered by "G".
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    Segment::Candidate *candidate;
    candidate = segment->add_candidate();
    candidate->value = "GoogleSuggest";
    EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  // Get prediction again
  EXPECT_TRUE(converter.Predict(*composer_));
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "GoogleSuggest");
  }

  {
    // Initialize no predict result triggered by "G".  It's possible
    // by Google Suggest.
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("G");
    EXPECT_CALL(mock_converter, StartPredictionForRequest(_, _)).Times(0);
  }
  // Hide prediction
  EXPECT_CALL(mock_converter, FocusSegmentValue(_, 0, 0));
  converter.CandidateNext(*composer_);
  Mock::VerifyAndClearExpectations(&mock_converter);
  EXPECT_TRUE(converter.IsActive());

  {  // Check the conversion.
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    EXPECT_EQ(conversion.segment_size(), 1);
    EXPECT_EQ(conversion.segment(0).value(), "GoogleSuggest");

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.candidate_size(), 1);
  }
}

TEST_F(SessionConverterTest, GetReadingText) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  const char *kKanjiAiueo = "阿伊宇江於";
  // Set up Segments for reverse conversion.
  Segments reverse_segments;
  Segment *segment;
  segment = reverse_segments.add_segment();
  segment->set_key(kKanjiAiueo);
  Segment::Candidate *candidate;
  candidate = segment->add_candidate();
  // For reverse conversion, key is the original kanji string.
  candidate->key = kKanjiAiueo;
  candidate->value = kChars_Aiueo;
  EXPECT_CALL(mock_converter,
              StartReverseConversion(_, absl::string_view(kKanjiAiueo)))
      .WillOnce(DoAll(SetArgPointee<0>(reverse_segments), Return(true)));
  std::string reading;
  EXPECT_TRUE(converter.GetReadingText(kKanjiAiueo, &reading));
  EXPECT_EQ(reading, kChars_Aiueo);
}

TEST_F(SessionConverterTest, ZeroQuerySuggestion) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  // Set up a mock suggestion result.
  Segments segments;
  Segment *segment;
  segment = segments.add_segment();
  segment->set_key("");
  segment->add_candidate()->value = "search";
  segment->add_candidate()->value = "input";
  EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  EXPECT_TRUE(composer_->Empty());
  EXPECT_TRUE(converter.Suggest(*composer_));
  EXPECT_TRUE(IsCandidateListVisible(converter));
  EXPECT_TRUE(converter.IsActive());

  {  // Check the output
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_FALSE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    const commands::Candidates &candidates = output.candidates();
    EXPECT_EQ(candidates.size(), 2);
    EXPECT_EQ(candidates.candidate(0).value(), "search");
    EXPECT_EQ(candidates.candidate(1).value(), "input");
  }
}

TEST(SessionConverterResetTest, Reset) {
  MockConverter mock_converter;
  const Request request;
  const Config config;
  SessionConverter converter(&mock_converter, &request, &config);
  EXPECT_CALL(mock_converter, ResetConversion(_));
  converter.Reset();
}

TEST(SessionConverterRevertTest, Revert) {
  MockConverter mock_converter;
  const Request request;
  const Config config;
  SessionConverter converter(&mock_converter, &request, &config);
  EXPECT_CALL(mock_converter, RevertConversion(_));
  converter.Revert();
}

TEST_F(SessionConverterTest, CommitHead) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  composer_->InsertCharacterPreedit(kChars_Aiueo);

  size_t committed_size;
  converter.CommitHead(1, *composer_, &committed_size);
  EXPECT_EQ(committed_size, 1);
  composer_->DeleteAt(0);

  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_candidates());

  const commands::Result &result = output.result();
  EXPECT_EQ(result.value(), "あ");
  EXPECT_EQ(result.key(), "あ");
  std::string preedit;
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "いうえお");

  converter.CommitHead(3, *composer_, &committed_size);
  EXPECT_EQ(committed_size, 3);
  composer_->DeleteAt(0);
  composer_->DeleteAt(0);
  composer_->DeleteAt(0);
  converter.FillOutput(*composer_, &output);
  EXPECT_TRUE(output.has_result());
  EXPECT_FALSE(output.has_candidates());

  const commands::Result &result2 = output.result();
  EXPECT_EQ(result2.value(), "いうえ");
  EXPECT_EQ(result2.key(), "いうえ");
  composer_->GetStringForPreedit(&preedit);
  EXPECT_EQ(preedit, "お");

  EXPECT_STATS_NOT_EXIST("Commit");
  EXPECT_STATS_NOT_EXIST("CommitFromComposition");
}

TEST_F(SessionConverterTest, CommandCandidate) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Segments segments;
  SetAiueo(&segments);
  FillT13Ns(&segments, composer_.get());
  // set COMMAND_CANDIDATE.
  SetCommandCandidate(&segments, 0, 0, Segment::Candidate::DEFAULT_COMMAND);
  EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
      .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

  composer_->InsertCharacterPreedit(kChars_Aiueo);
  EXPECT_TRUE(converter.Convert(*composer_));

  converter.Commit(*composer_, Context::default_instance());
  commands::Output output;
  converter.FillOutput(*composer_, &output);
  EXPECT_FALSE(output.has_result());
}

TEST_F(SessionConverterTest, CommandCandidateWithCommitCommands) {
  const std::string kKamabokono = "かまぼこの";
  const std::string kInbou = "いんぼう";
  composer_->InsertCharacterPreedit(kKamabokono + kInbou);

  {
    // The first candidate is a command candidate, so
    // CommitFirstSegment resets all conversion.
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    Segments segments;
    SetKamaboko(&segments);
    SetCommandCandidate(&segments, 0, 0, Segment::Candidate::DEFAULT_COMMAND);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    converter.Convert(*composer_);

    size_t committed_size = 0;
    converter.CommitFirstSegment(*composer_, Context::default_instance(),
                                 &committed_size);
    EXPECT_EQ(committed_size, 0);

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(converter.IsActive());
    EXPECT_FALSE(output.has_result());
  }

  {
    // The second candidate is a command candidate, so
    // CommitFirstSegment commits all conversion.
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    Segments segments;
    SetKamaboko(&segments);
    SetCommandCandidate(&segments, 1, 0, Segment::Candidate::DEFAULT_COMMAND);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    converter.Convert(*composer_);

    size_t committed_size = 0;
    converter.CommitFirstSegment(*composer_, Context::default_instance(),
                                 &committed_size);
    EXPECT_EQ(committed_size, Util::CharsLen(kKamabokono));

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_TRUE(converter.IsActive());
    EXPECT_TRUE(output.has_result());
  }

  {
    // The selected suggestion with Id is a command candidate.
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0, Segment::Candidate::DEFAULT_COMMAND);
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    converter.Suggest(*composer_);

    size_t committed_size = 0;
    EXPECT_FALSE(converter.CommitSuggestionById(
        0, *composer_, Context::default_instance(), &committed_size));
    EXPECT_EQ(committed_size, 0);
  }

  {
    // The selected suggestion with Index is a command candidate.
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 1, Segment::Candidate::DEFAULT_COMMAND);
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    converter.Suggest(*composer_);

    size_t committed_size = 0;
    EXPECT_FALSE(converter.CommitSuggestionByIndex(
        1, *composer_, Context::default_instance(), &committed_size));
    EXPECT_EQ(committed_size, 0);
  }
}

TEST_F(SessionConverterTest, ExecuteCommandCandidate) {
  // Enable Incognito mode
  {
    config_->set_incognito_mode(false);
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());

    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::ENABLE_INCOGNITO_MODE);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_TRUE(output.config().incognito_mode());
    EXPECT_FALSE(config_->incognito_mode());
  }

  // Disable Incognito mode
  {
    config_->set_incognito_mode(false);
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());

    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::DISABLE_INCOGNITO_MODE);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_FALSE(output.config().incognito_mode());
    EXPECT_FALSE(config_->incognito_mode());
  }

  // Enable Presentation mode
  {
    config_->set_presentation_mode(false);
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());

    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::ENABLE_PRESENTATION_MODE);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_TRUE(output.config().presentation_mode());
    EXPECT_FALSE(config_->presentation_mode());
  }

  // Disable Presentation mode
  {
    config_->set_incognito_mode(true);
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());

    Segments segments;
    SetAiueo(&segments);
    SetCommandCandidate(&segments, 0, 0,
                        Segment::Candidate::DISABLE_PRESENTATION_MODE);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    composer_->InsertCharacterPreedit(kChars_Aiueo);
    EXPECT_TRUE(converter.Convert(*composer_));

    converter.Commit(*composer_, Context::default_instance());
    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());

    // The config in the |output| has the updated value, but |config_| keeps
    // the previous value.
    EXPECT_TRUE(output.has_config());
    EXPECT_FALSE(output.config().presentation_mode());
    EXPECT_FALSE(config_->presentation_mode());
  }
}

TEST_F(SessionConverterTest, PropagateConfigToRenderer) {
  // Disable information_list_config()
  {
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));

    commands::Output output;
    composer_->InsertCharacterPreedit(kChars_Aiueo);
    converter.Convert(*composer_);

    EXPECT_FALSE(IsCandidateListVisible(converter));
    output.Clear();
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_config());

    converter.CandidateNext(*composer_);
    EXPECT_TRUE(IsCandidateListVisible(converter));
    output.Clear();
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_config());
  }
}

TEST_F(SessionConverterTest, ConversionFail) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  // Conversion fails.
  {
    // segments doesn't have any candidates.
    Segments segments;
    segments.add_segment()->set_key(kChars_Aiueo);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(false)));
    composer_->InsertCharacterPreedit(kChars_Aiueo);

    // Falls back to composition state.
    EXPECT_FALSE(converter.Convert(*composer_));
    Mock::VerifyAndClearExpectations(&mock_converter);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_TRUE(converter.CheckState(SessionConverterInterface::COMPOSITION));

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
    EXPECT_FALSE(IsCandidateListVisible(converter));
  }

  composer_->Reset();

  // Suggestion succeeds and conversion fails.
  {
    Segments segments;
    SetAiueo(&segments);
    EXPECT_CALL(mock_converter, StartSuggestionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
    composer_->InsertCharacterPreedit(kChars_Aiueo);

    EXPECT_TRUE(converter.Suggest(*composer_));
    Mock::VerifyAndClearExpectations(&mock_converter);
    EXPECT_TRUE(IsCandidateListVisible(converter));
    EXPECT_TRUE(converter.CheckState(SessionConverterInterface::SUGGESTION));

    commands::Output output;
    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_TRUE(output.has_candidates());

    segments.Clear();
    output.Clear();

    // segments doesn't have any candidates.
    segments.add_segment()->set_key(kChars_Aiueo);
    EXPECT_CALL(mock_converter, StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(false)));

    // Falls back to composition state.
    EXPECT_FALSE(converter.Convert(*composer_));
    Mock::VerifyAndClearExpectations(&mock_converter);
    EXPECT_FALSE(IsCandidateListVisible(converter));
    EXPECT_TRUE(converter.CheckState(SessionConverterInterface::COMPOSITION));

    converter.FillOutput(*composer_, &output);
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());
  }
}

TEST_F(SessionConverterTest, ResetByClientRevision) {
  const int32_t kRevision = 0x1234;

  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  Context context;

  // Initialize the session converter with given context age.
  EXPECT_CALL(mock_converter, ResetConversion(_));
  context.set_revision(kRevision);
  converter.OnStartComposition(context);
  EXPECT_CALL(mock_converter, RevertConversion(_));
  converter.Revert();

  // OnStartComposition with different context age causes Reset()
  EXPECT_CALL(mock_converter, ResetConversion(_));
  context.set_revision(kRevision + 1);
  converter.OnStartComposition(context);
}

TEST_F(SessionConverterTest, ResetByPrecedingText) {
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());

  // no preceding_text -> Reset should not be called.
  {
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    converter.OnStartComposition(Context::default_instance());
    EXPECT_CALL(mock_converter, RevertConversion(_));
    converter.Revert();
  }

  // preceding_text == history_segments -> Reset should not be called.
  {
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text(kChars_Aiueo);
    converter.OnStartComposition(context);
    EXPECT_CALL(mock_converter, RevertConversion(_));
    converter.Revert();
  }

  // preceding_text == "" && history_segments != "" -> Reset should be called.
  {
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text("");
    EXPECT_CALL(mock_converter, ResetConversion(_));
    converter.OnStartComposition(context);
    EXPECT_CALL(mock_converter, RevertConversion(_));
    converter.Revert();
  }

  // preceding_text != "" && preceding_text.EndsWith(history_segments).
  //    -> Reset should not be called.
  {
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text(kChars_Aiueo);
    converter.OnStartComposition(context);
  }

  // preceding_text != "" && history_segments.EndsWith(preceding_text).
  //    -> Reset should not be called.
  {
    Segments segments;
    SetAiueo(&segments);
    FillT13Ns(&segments, composer_.get());
    for (size_t i = 0; i < segments.segments_size(); ++i) {
      Segment *segment = segments.mutable_segment(i);
      segment->set_segment_type(Segment::HISTORY);
    }
    SetSegments(segments, &converter);
    Context context;
    context.set_preceding_text(kChars_Aiueo);
    converter.OnStartComposition(context);
    EXPECT_CALL(mock_converter, RevertConversion(_));
    converter.Revert();
  }
}

TEST_F(SessionConverterTest, ReconstructHistoryByPrecedingText) {
  constexpr uint16_t kId = 1234;
  constexpr char kKey[] = "1";
  constexpr char kValue[] = "1";

  // Set up the result which mock_converter.ReconstructHistory() returns.
  Segments mock_result;
  {
    Segment *segment = mock_result.add_segment();
    segment->set_key(kKey);
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->push_back_candidate();
    candidate->rid = kId;
    candidate->lid = kId;
    candidate->content_key = kKey;
    candidate->key = kKey;
    candidate->content_value = kValue;
    candidate->value = kValue;
    candidate->attributes = Segment::Candidate::NO_LEARNING;
  }

  // With revision
  {
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());

    EXPECT_CALL(mock_converter, ReconstructHistory(_, absl::string_view(kKey)))
        .WillOnce(DoAll(SetArgPointee<0>(mock_result), Return(true)));

    Context context;
    context.set_revision(0);
    context.set_preceding_text(kKey);
    // History segments should be reconstructed by this call.
    converter.OnStartComposition(context);
    EXPECT_THAT(GetSegments(converter), EqualsSegments(mock_result));

    // Increment the revesion. Since the history segments for kKey was already
    // constructed, ReconstructHistory should not be called.
    context.set_revision(1);
    context.set_preceding_text(kKey);
    converter.OnStartComposition(context);
  }

  // Without revision
  {
    MockConverter mock_converter;
    SessionConverter converter(&mock_converter, request_.get(), config_.get());

    EXPECT_CALL(mock_converter, ReconstructHistory(_, absl::string_view(kKey)))
        .WillOnce(DoAll(SetArgPointee<0>(mock_result), Return(true)));

    Context context;
    context.set_preceding_text(kKey);
    converter.OnStartComposition(context);
    // History segments should be reconstructed by this call.
    converter.OnStartComposition(context);
    EXPECT_THAT(GetSegments(converter), EqualsSegments(mock_result));

    // Revesion is not present but, since the history segments for kKey was
    // already constructed, ReconstructHistory should not be called.
    context.set_preceding_text(kKey);
    converter.OnStartComposition(context);
  }
}

// Test whether Request::candidate_page_size is correctly propagated to
// CandidateList.page_size in SessionConverter.  The tests for the behavior
// of CandidateList.page_size is in session/internal/candidate_list_test.cc
TEST_F(SessionConverterTest, CandidatePageSize) {
  constexpr size_t kPageSize = 3;
  request_->set_candidate_page_size(kPageSize);
  MockConverter mock_converter;
  SessionConverter converter(&mock_converter, request_.get(), config_.get());
  EXPECT_EQ(GetCandidateList(converter).page_size(), kPageSize);
}

}  // namespace session
}  // namespace mozc
