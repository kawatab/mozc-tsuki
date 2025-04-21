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

#include "converter/converter.h"

#include <cstdint>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_impl.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "dictionary/system/value_dictionary.h"
#include "dictionary/user_dictionary.h"
#include "dictionary/user_dictionary_stub.h"
#include "dictionary/user_pos.h"
#include "engine/engine.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "prediction/dictionary_predictor.h"
#include "prediction/predictor.h"
#include "prediction/predictor_interface.h"
#include "prediction/suggestion_filter.h"
#include "prediction/user_history_predictor.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "session/request_test_util.h"
#include "testing/gmock.h"
#include "testing/googletest.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

using ::mozc::dictionary::DictionaryImpl;
using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::PosGroup;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::SuffixDictionary;
using ::mozc::dictionary::SuppressionDictionary;
using ::mozc::dictionary::SystemDictionary;
using ::mozc::dictionary::Token;
using ::mozc::dictionary::UserDictionaryStub;
using ::mozc::dictionary::ValueDictionary;
using ::mozc::prediction::DefaultPredictor;
using ::mozc::prediction::DictionaryPredictor;
using ::mozc::prediction::MobilePredictor;
using ::mozc::prediction::PredictorInterface;
using ::mozc::prediction::UserHistoryPredictor;
using ::mozc::usage_stats::UsageStats;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::StrEq;

void PushBackCandidate(Segment *segment, absl::string_view text) {
  Segment::Candidate *cand = segment->push_back_candidate();
  cand->key = std::string(text);
  cand->content_key = cand->key;
  cand->value = cand->key;
  cand->content_value = cand->key;
}

class StubPredictor : public PredictorInterface {
 public:
  StubPredictor() : predictor_name_("StubPredictor") {}

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    if (segments->conversion_segments_size() == 0) {
      return false;
    }
    Segment *seg = segments->mutable_conversion_segment(0);
    if (seg->key().empty()) {
      return false;
    }
    PushBackCandidate(seg, seg->key());
    return true;
  }

  const std::string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const std::string predictor_name_;
};

class StubRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override {
    return true;
  }
};

SuffixDictionary *CreateSuffixDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  absl::string_view suffix_key_array_data, suffix_value_array_data;
  const uint32_t *token_array;
  data_manager.GetSuffixDictionaryData(&suffix_key_array_data,
                                       &suffix_value_array_data, &token_array);
  return new SuffixDictionary(suffix_key_array_data, suffix_value_array_data,
                              token_array);
}

class InsertDummyWordsRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &, Segments *segments) const override {
    for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
      Segment *seg = segments->mutable_conversion_segment(i);
      {
        Segment::Candidate *cand = seg->add_candidate();
        cand->key = "tobefiltered";
        cand->value = "ToBeFiltered";
      }
      {
        Segment::Candidate *cand = seg->add_candidate();
        cand->key = "nottobefiltered";
        cand->value = "NotToBeFiltered";
      }
    }
    return true;
  }
};

}  // namespace

class ConverterTest : public ::testing::Test {
 protected:
  enum PredictorType {
    STUB_PREDICTOR,
    DEFAULT_PREDICTOR,
    MOBILE_PREDICTOR,
  };

  struct UserDefinedEntry {
    const std::string key;
    const std::string value;
    const user_dictionary::UserDictionary::PosType pos;
    UserDefinedEntry(absl::string_view k, absl::string_view v,
                     user_dictionary::UserDictionary::PosType p)
        : key(k), value(v), pos(p) {}
  };

  void SetUp() override { UsageStats::ClearAllStatsForTest(); }

  void TearDown() override { UsageStats::ClearAllStatsForTest(); }

  // This struct holds resources used by converter.
  struct ConverterAndData {
    std::unique_ptr<testing::MockDataManager> data_manager;
    std::unique_ptr<DictionaryInterface> user_dictionary;
    std::unique_ptr<SuppressionDictionary> suppression_dictionary;
    std::unique_ptr<DictionaryInterface> suffix_dictionary;
    Connector connector;
    std::unique_ptr<const Segmenter> segmenter;
    std::unique_ptr<DictionaryInterface> dictionary;
    std::unique_ptr<const PosGroup> pos_group;
    SuggestionFilter suggestion_filter;
    std::unique_ptr<ImmutableConverterInterface> immutable_converter;
    std::unique_ptr<ConverterImpl> converter;
    dictionary::PosMatcher pos_matcher;
  };

  // Returns initialized predictor for the given type.
  // Note that all fields of |converter_and_data| should be filled including
  // |converter_and_data.converter|. |converter| will be initialized using
  // predictor pointer, but predictor need the pointer for |converter| for
  // initializing. Prease see mozc/engine/engine.cc for details.
  // Caller should manage the ownership.
  std::unique_ptr<PredictorInterface> CreatePredictor(
      PredictorType predictor_type, const PosMatcher *pos_matcher,
      const ConverterAndData &converter_and_data) {
    if (predictor_type == STUB_PREDICTOR) {
      return std::make_unique<StubPredictor>();
    }

    std::unique_ptr<PredictorInterface> (*predictor_factory)(
        std::unique_ptr<PredictorInterface>,
        std::unique_ptr<PredictorInterface>, const ConverterInterface *) =
        nullptr;
    bool enable_content_word_learning = false;

    switch (predictor_type) {
      case DEFAULT_PREDICTOR:
        predictor_factory = DefaultPredictor::CreateDefaultPredictor;
        enable_content_word_learning = false;
        break;
      case MOBILE_PREDICTOR:
        predictor_factory = MobilePredictor::CreateMobilePredictor;
        enable_content_word_learning = true;
        break;
      default:
        LOG(ERROR) << "Should not come here: Invalid predictor type.";
        predictor_factory = DefaultPredictor::CreateDefaultPredictor;
        enable_content_word_learning = false;
        break;
    }

    CHECK(converter_and_data.converter.get()) << "converter should be filled.";

    // Create a predictor with three sub-predictors, dictionary predictor, user
    // history predictor, and extra predictor.
    auto dictionary_predictor = std::make_unique<DictionaryPredictor>(
        *converter_and_data.data_manager, converter_and_data.converter.get(),
        converter_and_data.immutable_converter.get(),
        converter_and_data.dictionary.get(),
        converter_and_data.suffix_dictionary.get(),
        converter_and_data.connector, converter_and_data.segmenter.get(),
        *pos_matcher, converter_and_data.suggestion_filter);
    CHECK(dictionary_predictor);

    auto user_history_predictor = std::make_unique<UserHistoryPredictor>(
        converter_and_data.dictionary.get(), pos_matcher,
        converter_and_data.suppression_dictionary.get(),
        enable_content_word_learning);
    CHECK(user_history_predictor);

    auto ret_predictor = (*predictor_factory)(
        std::move(dictionary_predictor), std::move(user_history_predictor),
        converter_and_data.converter.get());
    CHECK(ret_predictor);
    return ret_predictor;
  }

  // Initializes ConverterAndData with mock data set using given
  // |user_dictionary| and |suppression_dictionary|.
  void InitConverterAndData(
      std::unique_ptr<DictionaryInterface> user_dictionary,
      std::unique_ptr<SuppressionDictionary> suppression_dictionary,
      std::unique_ptr<RewriterInterface> rewriter, PredictorType predictor_type,
      ConverterAndData *converter_and_data) {
    converter_and_data->data_manager =
        std::make_unique<testing::MockDataManager>();
    const auto &data_manager = *converter_and_data->data_manager;

    const char *dictionary_data = nullptr;
    int dictionary_size = 0;
    data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);

    converter_and_data->pos_matcher.Set(data_manager.GetPosMatcherData());

    std::unique_ptr<SystemDictionary> sysdic =
        SystemDictionary::Builder(dictionary_data, dictionary_size)
            .Build()
            .value();
    auto value_dic = std::make_unique<ValueDictionary>(
        converter_and_data->pos_matcher, &sysdic->value_trie());
    converter_and_data->user_dictionary = std::move(user_dictionary);
    converter_and_data->suppression_dictionary =
        std::move(suppression_dictionary);
    converter_and_data->dictionary = std::make_unique<DictionaryImpl>(
        std::move(sysdic), std::move(value_dic),
        converter_and_data->user_dictionary.get(),
        converter_and_data->suppression_dictionary.get(),
        &converter_and_data->pos_matcher);
    converter_and_data->pos_group =
        std::make_unique<PosGroup>(data_manager.GetPosGroupData());
    converter_and_data->suggestion_filter =
        SuggestionFilter::CreateOrDie(data_manager.GetSuggestionFilterData());
    converter_and_data->suffix_dictionary.reset(
        CreateSuffixDictionaryFromDataManager(data_manager));
    converter_and_data->connector =
        Connector::CreateFromDataManager(data_manager).value();
    converter_and_data->segmenter =
        Segmenter::CreateFromDataManager(data_manager);
    converter_and_data->immutable_converter =
        std::make_unique<ImmutableConverterImpl>(
            converter_and_data->dictionary.get(),
            converter_and_data->suffix_dictionary.get(),
            converter_and_data->suppression_dictionary.get(),
            converter_and_data->connector, converter_and_data->segmenter.get(),
            &converter_and_data->pos_matcher,
            converter_and_data->pos_group.get(),
            converter_and_data->suggestion_filter);
    converter_and_data->converter = std::make_unique<ConverterImpl>();

    auto predictor = CreatePredictor(
        predictor_type, &converter_and_data->pos_matcher, *converter_and_data);
    converter_and_data->converter->Init(
        &converter_and_data->pos_matcher,
        converter_and_data->suppression_dictionary.get(), std::move(predictor),
        std::move(rewriter), converter_and_data->immutable_converter.get());
  }

  std::unique_ptr<ConverterAndData> CreateConverterAndData(
      std::unique_ptr<RewriterInterface> rewriter,
      PredictorType predictor_type) {
    auto ret = std::make_unique<ConverterAndData>();
    InitConverterAndData(std::make_unique<UserDictionaryStub>(),
                         std::make_unique<SuppressionDictionary>(),
                         std::move(rewriter), predictor_type, ret.get());
    return ret;
  }

  std::unique_ptr<ConverterAndData> CreateStubbedConverterAndData() {
    return CreateConverterAndData(std::make_unique<StubRewriter>(),
                                  STUB_PREDICTOR);
  }

  std::unique_ptr<ConverterAndData>
  CreateConverterAndDataWithInsertDummyWordsRewriter() {
    return CreateConverterAndData(std::make_unique<InsertDummyWordsRewriter>(),
                                  STUB_PREDICTOR);
  }

  std::unique_ptr<ConverterAndData>
  CreateConverterAndDataWithUserDefinedEntries(
      const std::vector<UserDefinedEntry> &user_defined_entries,
      std::unique_ptr<RewriterInterface> rewriter,
      PredictorType predictor_type) {
    auto ret = std::make_unique<ConverterAndData>();

    ret->pos_matcher.Set(mock_data_manager_.GetPosMatcherData());

    auto suppression_dictionary = std::make_unique<SuppressionDictionary>();
    auto user_dictionary = std::make_unique<dictionary::UserDictionary>(
        dictionary::UserPos::CreateFromDataManager(mock_data_manager_),
        ret->pos_matcher, suppression_dictionary.get());
    {
      user_dictionary::UserDictionaryStorage storage;
      using UserEntry = user_dictionary::UserDictionary::Entry;
      user_dictionary::UserDictionary *dictionary = storage.add_dictionaries();
      for (const auto &user_entry : user_defined_entries) {
        UserEntry *entry = dictionary->add_entries();
        entry->set_key(user_entry.key);
        entry->set_value(user_entry.value);
        entry->set_pos(user_entry.pos);
      }
      user_dictionary->Load(storage);
    }
    InitConverterAndData(std::move(user_dictionary),
                         std::move(suppression_dictionary), std::move(rewriter),
                         predictor_type, ret.get());
    return ret;
  }

  std::unique_ptr<EngineInterface> CreateEngineWithMobilePredictor() {
    return Engine::CreateMobileEngineHelper<testing::MockDataManager>().value();
  }

  bool FindCandidateByValue(absl::string_view value,
                            const Segment &segment) const {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        return true;
      }
    }
    return false;
  }

  int GetCandidateIndexByValue(absl::string_view value,
                               const Segment &segment) const {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        return i;
      }
    }
    return -1;  // not found
  }

  const commands::Request &default_request() const { return default_request_; }

 private:
  const testing::ScopedTempUserProfileDirectory scoped_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
  const commands::Request default_request_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

// test for issue:2209644
// just checking whether this causes segmentation fault or not.
// TODO(toshiyuki): make dictionary mock and test strictly.
TEST_F(ConverterTest, CanConvertTest) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "-"));
  }
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "おきておきて"));
  }
}

namespace {
std::string ContextAwareConvert(const std::string &first_key,
                                const std::string &first_value,
                                const std::string &second_key) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  Segments segments;
  EXPECT_TRUE(converter->StartConversion(&segments, first_key));

  std::string converted;
  int segment_num = 0;
  while (true) {
    int position = -1;
    for (size_t i = 0; i < segments.segment(segment_num).candidates_size();
         ++i) {
      const std::string &value =
          segments.segment(segment_num).candidate(i).value;
      if (first_value.substr(converted.size(), value.size()) == value) {
        position = static_cast<int>(i);
        converted += value;
        break;
      }
    }

    if (position < 0) {
      break;
    }

    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, position))
        << first_value;

    ++segment_num;

    if (first_value == converted) {
      break;
    }
  }
  EXPECT_EQ(first_value, converted) << first_value;
  // TODO(team): Use StartConversionForRequest instead of StartConversion.
  const ConversionRequest default_request;
  converter->FinishConversion(default_request, &segments);
  EXPECT_TRUE(converter->StartConversion(&segments, second_key));
  EXPECT_EQ(segments.segments_size(), segment_num + 1);

  return segments.segment(segment_num).candidate(0).value;
}
}  // namespace

TEST_F(ConverterTest, ContextAwareConversionTest) {
  // Desirable context aware conversions
  EXPECT_EQ(ContextAwareConvert("きき", "危機", "いっぱつ"), "一髪");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 2000, 1, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 2000, 1, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 2);

  EXPECT_EQ(ContextAwareConvert("きょうと", "京都", "だい"), "大");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 4000, 2, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 4000, 2, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 2000, 2, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 4);

  EXPECT_EQ(ContextAwareConvert("もんだい", "問題", "てん"), "点");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 6000, 3, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 6000, 3, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 3000, 3, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 6);

  EXPECT_EQ(ContextAwareConvert("いのうえ", "井上", "ようすい"), "陽水");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 8000, 4, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 8000, 4, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 4000, 4, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 8);

  // Undesirable context aware conversions
  EXPECT_NE(ContextAwareConvert("19じ", "19時", "しゅうごう"), "宗号");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 11000, 6, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 11000, 5, 2000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 6000, 5, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 11);

  EXPECT_NE(ContextAwareConvert("の", "の", "なまえ"), "な前");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 12000, 7, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 12000, 6, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 7000, 6, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 12);

  EXPECT_NE(ContextAwareConvert("の", "の", "しりょう"), "し料");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 13000, 8, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 13000, 7, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 8000, 7, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 13);

  EXPECT_NE(ContextAwareConvert("ぼくと", "僕と", "しらいさん"), "し礼賛");
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 15000, 9, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 15000, 8, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 9000, 8, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 15);
}

TEST_F(ConverterTest, CommitSegmentValue) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  {
    // Prepare a segment, with candidates "1" and "2";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "1";
    segment->add_candidate()->value = "2";
  }
  {
    // Prepare a segment, with candidates "3" and "4";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "3";
    segment->add_candidate()->value = "4";
  }
  {
    // Commit the candidate whose value is "2".
    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 1));
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.history_segments_size(), 0);
    EXPECT_EQ(segments.conversion_segments_size(), 2);
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(segment.segment_type(), Segment::FIXED_VALUE);
    EXPECT_EQ(segment.candidate(0).value, "2");
    EXPECT_NE(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
              0);
  }
  {
    // Make the segment SUBMITTED
    segments.mutable_conversion_segment(0)->set_segment_type(
        Segment::SUBMITTED);
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.history_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segments_size(), 1);
  }
  {
    // Commit the candidate whose value is "3".
    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 0));
    EXPECT_EQ(segments.segments_size(), 2);
    EXPECT_EQ(segments.history_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(segment.segment_type(), Segment::FIXED_VALUE);
    EXPECT_EQ(segment.candidate(0).value, "3");
    EXPECT_EQ(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
              0);
  }
}

TEST_F(ConverterTest, CommitSegments) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  // History segment.
  {
    Segment *segment = segments.add_segment();
    segment->set_key("あした");
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "あした";
    candidate->value = "今日";
  }

  {
    Segment *segment = segments.add_segment();
    segment->set_key("かつこうに");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "学校に";
    candidate->key = "がっこうに";
  }

  {
    Segment *segment = segments.add_segment();
    segment->set_key("いく");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "行く";
    candidate->key = "いく";
  }

  // Test "CommitFirstSegment" feature.
  {
    // Commit 1st segment.
    std::vector<size_t> index_list;
    index_list.push_back(0);
    ASSERT_TRUE(converter->CommitSegments(&segments, index_list));

    EXPECT_EQ(segments.history_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.history_segment(0).segment_type(), Segment::HISTORY);
    EXPECT_EQ(segments.history_segment(1).segment_type(), Segment::SUBMITTED);

    EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 3000, 1, 3000, 3000);
    EXPECT_TIMING_STATS("SubmittedLengthx1000", 3000, 1, 3000, 3000);
    EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
    EXPECT_COUNT_STATS("SubmittedTotalLength", 3);
  }

  // Reset the test data.
  segments.mutable_history_segment(1)->set_segment_type(Segment::FREE);

  // Test "CommitHeadToFocusedSegment" feature.
  {
    // Commit 1st and 2nd segments.
    std::vector<size_t> index_list;
    index_list.push_back(0);
    index_list.push_back(0);
    ASSERT_TRUE(converter->CommitSegments(&segments, index_list));

    EXPECT_EQ(segments.history_segments_size(), 3);
    EXPECT_EQ(segments.conversion_segments_size(), 0);
    EXPECT_EQ(segments.history_segment(0).segment_type(), Segment::HISTORY);
    EXPECT_EQ(segments.history_segment(1).segment_type(), Segment::SUBMITTED);
    EXPECT_EQ(segments.history_segment(2).segment_type(), Segment::SUBMITTED);

    EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 8000, 3, 2000, 3000);
    EXPECT_TIMING_STATS("SubmittedLengthx1000", 8000, 2, 3000, 5000);
    EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 3000, 2, 1000, 2000);
    EXPECT_COUNT_STATS("SubmittedTotalLength", 8);
  }
}

TEST_F(ConverterTest, CommitPartialSuggestionSegmentValue) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  {
    // Prepare a segment, with candidates "1" and "2";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "1";
    segment->add_candidate()->value = "2";
  }
  {
    // Prepare a segment, with candidates "3" and "4";
    Segment *segment = segments.add_segment();
    segment->add_candidate()->value = "3";
    segment->add_candidate()->value = "4";
  }
  {
    // Commit the candidate whose value is "2".
    EXPECT_TRUE(converter->CommitPartialSuggestionSegmentValue(
        &segments, 0, 1, "left2", "right2"));
    EXPECT_EQ(segments.segments_size(), 3);
    EXPECT_EQ(segments.history_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segments_size(), 2);
    {
      // The tail segment of the history segments uses
      // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
      // and contains original value.
      const Segment &segment =
          segments.history_segment(segments.history_segments_size() - 1);
      EXPECT_EQ(segment.segment_type(), Segment::SUBMITTED);
      EXPECT_EQ(segment.candidate(0).value, "2");
      EXPECT_EQ(segment.key(), "left2");
      EXPECT_NE(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
                0);
    }
    {
      // The head segment of the conversion segments uses |new_segment_key|.
      const Segment &segment = segments.conversion_segment(0);
      EXPECT_EQ(segment.segment_type(), Segment::FREE);
      EXPECT_EQ(segment.key(), "right2");
    }
  }
}

TEST_F(ConverterTest, CommitPartialSuggestionUsageStats) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  // History segment.
  {
    Segment *segment = segments.add_segment();
    segment->set_key("あした");
    segment->set_segment_type(Segment::HISTORY);

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "あした";
    candidate->value = "今日";
  }

  {
    Segment *segment = segments.add_segment();
    segment->set_key("かつこうに");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "学校に";
    candidate->key = "がっこうに";

    candidate = segment->add_candidate();
    candidate->value = "格好に";
    candidate->key = "かっこうに";

    candidate = segment->add_candidate();
    candidate->value = "かつこうに";
    candidate->key = "かつこうに";
  }

  EXPECT_STATS_NOT_EXIST("CommitPartialSuggestion");
  EXPECT_TRUE(converter->CommitPartialSuggestionSegmentValue(
      &segments, 0, 1, "かつこうに", "いく"));
  EXPECT_EQ(segments.history_segments_size(), 2);
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.history_segment(0).segment_type(), Segment::HISTORY);
  EXPECT_EQ(segments.history_segment(1).segment_type(), Segment::SUBMITTED);
  {
    // The tail segment of the history segments uses
    // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
    // and contains original value.
    const Segment &segment =
        segments.history_segment(segments.history_segments_size() - 1);
    EXPECT_EQ(segment.segment_type(), Segment::SUBMITTED);
    EXPECT_EQ(segment.candidate(0).value, "格好に");
    EXPECT_EQ(segment.candidate(0).key, "かっこうに");
    EXPECT_EQ(segment.key(), "かつこうに");
    EXPECT_NE(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
              0);
  }
  {
    // The head segment of the conversion segments uses |new_segment_key|.
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(segment.segment_type(), Segment::FREE);
    EXPECT_EQ(segment.key(), "いく");
  }

  EXPECT_COUNT_STATS("CommitPartialSuggestion", 1);
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 3);
}

TEST_F(ConverterTest, CommitAutoPartialSuggestionUsageStats) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;

  {
    Segment *segment = segments.add_segment();
    segment->set_key("かつこうにいく");

    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "学校にいく";
    candidate->key = "がっこうにいく";

    candidate = segment->add_candidate();
    candidate->value = "学校に行く";
    candidate->key = "がっこうにいく";

    candidate = segment->add_candidate();
    candidate->value = "学校に";
    candidate->key = "がっこうに";
  }

  EXPECT_STATS_NOT_EXIST("CommitPartialSuggestion");
  EXPECT_TRUE(converter->CommitPartialSuggestionSegmentValue(
      &segments, 0, 2, "かつこうに", "いく"));
  EXPECT_EQ(segments.segments_size(), 2);
  EXPECT_EQ(segments.history_segments_size(), 1);
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  {
    // The tail segment of the history segments uses
    // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
    // and contains original value.
    const Segment &segment =
        segments.history_segment(segments.history_segments_size() - 1);
    EXPECT_EQ(segment.segment_type(), Segment::SUBMITTED);
    EXPECT_EQ(segment.candidate(0).value, "学校に");
    EXPECT_EQ(segment.candidate(0).key, "がっこうに");
    EXPECT_EQ(segment.key(), "かつこうに");
    EXPECT_NE(segment.candidate(0).attributes & Segment::Candidate::RERANKED,
              0);
  }
  {
    // The head segment of the conversion segments uses |new_segment_key|.
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(segment.segment_type(), Segment::FREE);
    EXPECT_EQ(segment.key(), "いく");
  }

  EXPECT_COUNT_STATS("CommitAutoPartialSuggestion", 1);
}

TEST_F(ConverterTest, CandidateKeyTest) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartConversion(&segments, "わたしは"));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたし");
}

TEST_F(ConverterTest, Regression3437022) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  const std::string kKey1 = "けいたい";
  const std::string kKey2 = "でんわ";

  const std::string kValue1 = "携帯";
  const std::string kValue2 = "電話";

  {
    // Make sure convert result is one segment
    EXPECT_TRUE(converter->StartConversion(&segments, kKey1 + kKey2));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kValue1 + kValue2);
  }
  {
    // Make sure we can convert first part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(&segments, kKey1));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kValue1);
  }
  {
    // Make sure we can convert last part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(&segments, kKey2));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kValue2);
  }

  // Add compound entry to suppression dictionary
  segments.Clear();

  SuppressionDictionary *dic = engine->GetSuppressionDictionary();
  dic->Lock();
  dic->AddEntry(kKey1 + kKey2, kValue1 + kValue2);
  dic->UnLock();

  EXPECT_TRUE(converter->StartConversion(&segments, kKey1 + kKey2));

  int rest_size = 0;
  for (size_t i = 1; i < segments.conversion_segments_size(); ++i) {
    rest_size +=
        Util::CharsLen(segments.conversion_segment(i).candidate(0).key);
  }

  // Expand segment so that the entire part will become one segment
  if (rest_size > 0) {
    const ConversionRequest default_request;
    EXPECT_TRUE(
        converter->ResizeSegment(&segments, default_request, 0, rest_size));
  }

  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_NE(segments.conversion_segment(0).candidate(0).value,
            kValue1 + kValue2);

  dic->Lock();
  dic->Clear();
  dic->UnLock();
}

TEST_F(ConverterTest, CompletePosIds) {
  const char *kTestKeys[] = {
      "きょうと", "いきます",         "うつくしい",
      "おおきな", "いっちゃわないね", "わたしのなまえはなかのです",
  };

  std::unique_ptr<ConverterAndData> converter_and_data =
      CreateStubbedConverterAndData();
  ConverterImpl *converter = converter_and_data->converter.get();
  for (size_t i = 0; i < std::size(kTestKeys); ++i) {
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kTestKeys[i]);
    seg->set_segment_type(Segment::FREE);
    ConversionRequest request;
    request.set_request_type(ConversionRequest::PREDICTION);
    request.set_max_conversion_candidates_size(20);
    CHECK(converter_and_data->immutable_converter->ConvertForRequest(
        request, &segments));
    const int lid = segments.segment(0).candidate(0).lid;
    const int rid = segments.segment(0).candidate(0).rid;
    Segment::Candidate candidate;
    candidate.value = segments.segment(0).candidate(0).value;
    candidate.key = segments.segment(0).candidate(0).key;
    candidate.lid = 0;
    candidate.rid = 0;
    converter->CompletePosIds(&candidate);
    EXPECT_EQ(candidate.lid, lid);
    EXPECT_EQ(candidate.rid, rid);
    EXPECT_NE(candidate.lid, 0);
    EXPECT_NE(candidate.rid, 0);
  }

  {
    Segment::Candidate candidate;
    candidate.key = "test";
    candidate.value = "test";
    candidate.lid = 10;
    candidate.rid = 11;
    converter->CompletePosIds(&candidate);
    EXPECT_EQ(candidate.lid, 10);
    EXPECT_EQ(candidate.rid, 11);
  }
}

TEST_F(ConverterTest, Regression3046266) {
  // Shouldn't correct nodes at the beginning of a sentence.
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  // Can be any string that has "ん" at the end
  constexpr char kKey1[] = "かん";

  // Can be any string that has a vowel at the beginning
  constexpr char kKey2[] = "あか";

  constexpr char kValueNotExpected[] = "中";

  EXPECT_TRUE(converter->StartConversion(&segments, kKey1));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 0));

  // TODO(team): Use StartConversionForRequest instead of StartConversion.
  const ConversionRequest default_request;
  converter->FinishConversion(default_request, &segments);

  EXPECT_TRUE(converter->StartConversion(&segments, kKey2));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_NE(segment.candidate(i).value, kValueNotExpected);
  }
}

TEST_F(ConverterTest, Regression5502496) {
  // Make sure key correction works for the first word of a sentence.
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  constexpr char kKey[] = "みんあ";
  constexpr char kValueExpected[] = "みんな";

  EXPECT_TRUE(converter->StartConversion(&segments, kKey));
  EXPECT_EQ(segments.conversion_segments_size(), 1);
  const Segment &segment = segments.conversion_segment(0);
  bool found = false;
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (segment.candidate(i).value == kValueExpected) {
      found = true;
    }
  }
  EXPECT_TRUE(found);
}

TEST_F(ConverterTest, StartSuggestionForRequest) {
  commands::Request client_request;
  client_request.set_mixed_conversion(true);

  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  const std::string kShi = "し";

  composer::Table table;
  table.AddRule("si", kShi, "");
  table.AddRule("shi", kShi, "");
  config::Config config;

  {
    composer::Composer composer(&table, &client_request, &config);

    composer.InsertCharacter("shi");

    Segments segments;
    ConversionRequest request(&composer, &client_request, &config);
    request.set_request_type(ConversionRequest::SUGGESTION);
    EXPECT_TRUE(converter->StartSuggestionForRequest(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ(
        segments.segment(0).meta_candidate(transliteration::HALF_ASCII).value,
        "shi");
  }

  {
    composer::Composer composer(&table, &client_request, &config);

    composer.InsertCharacter("si");

    Segments segments;
    ConversionRequest request(&composer, &client_request, &config);
    request.set_request_type(ConversionRequest::SUGGESTION);
    EXPECT_TRUE(converter->StartSuggestionForRequest(request, &segments));
    EXPECT_EQ(segments.segments_size(), 1);
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ(
        segments.segment(0).meta_candidate(transliteration::HALF_ASCII).value,
        "si");
  }
}

TEST_F(ConverterTest, StartPartialPrediction) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialPrediction(&segments, "わたしは"));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
}

TEST_F(ConverterTest, StartPartialSuggestion) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialSuggestion(&segments, "わたしは"));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
}

TEST_F(ConverterTest, StartPartialPredictionMobile) {
  std::unique_ptr<EngineInterface> engine = CreateEngineWithMobilePredictor();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialPrediction(&segments, "わたしは"));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
}

TEST_F(ConverterTest, StartPartialSuggestionMobile) {
  std::unique_ptr<EngineInterface> engine = CreateEngineWithMobilePredictor();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialSuggestion(&segments, "わたしは"));
  EXPECT_EQ(segments.segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidate(0).key, "わたしは");
  EXPECT_EQ(segments.segment(0).candidate(0).content_key, "わたしは");
}

TEST_F(ConverterTest, MaybeSetConsumedKeySizeToSegment) {
  const size_t consumed_key_size = 5;
  const size_t original_consumed_key_size = 10;

  Segment segment;
  // 1st candidate without PARTIALLY_KEY_CONSUMED
  segment.push_back_candidate();
  // 2nd candidate with PARTIALLY_KEY_CONSUMED
  Segment::Candidate *candidate2 = segment.push_back_candidate();
  candidate2->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  candidate2->consumed_key_size = original_consumed_key_size;
  // 1st meta candidate without PARTIALLY_KEY_CONSUMED
  segment.add_meta_candidate();
  // 2nd meta candidate with PARTIALLY_KEY_CONSUMED
  Segment::Candidate *meta_candidate2 = segment.add_meta_candidate();
  meta_candidate2->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
  meta_candidate2->consumed_key_size = original_consumed_key_size;

  ConverterImpl::MaybeSetConsumedKeySizeToSegment(consumed_key_size, &segment);
  EXPECT_NE((segment.candidate(0).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.candidate(0).consumed_key_size, consumed_key_size);
  EXPECT_NE((segment.candidate(1).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.candidate(1).consumed_key_size, original_consumed_key_size);
  EXPECT_NE((segment.meta_candidate(0).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.meta_candidate(0).consumed_key_size, consumed_key_size);
  EXPECT_NE((segment.meta_candidate(1).attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED),
            0);
  EXPECT_EQ(segment.meta_candidate(1).consumed_key_size,
            original_consumed_key_size);
}

TEST_F(ConverterTest, PredictSetKey) {
  constexpr char kPredictionKey[] = "prediction key";
  constexpr char kPredictionKey2[] = "prediction key2";
  // Tests whether SetKey method is called or not.
  struct TestData {
    // Input conditions.
    const bool should_call_set_key_in_prediction;  // Member of Request.
    const std::optional<absl::string_view> key;    // Input key presence.

    const bool expect_set_key_is_called;
  };
  const TestData test_data_list[] = {
      {true, std::nullopt, true},
      {true, kPredictionKey, true},
      {true, kPredictionKey2, true},
      {false, std::nullopt, true},
      {false, kPredictionKey2, true},
      // This is the only case where SetKey() is not called; because SetKey is
      // not requested in Request and Segments' key is already present.
      {false, kPredictionKey, false},
  };

  std::unique_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();
  ASSERT_NE(converter, nullptr);
  // Note that TearDown method will reset above stubs.

  for (const TestData &test_data : test_data_list) {
    Segments segments;
    int orig_candidates_size = 0;
    if (test_data.key) {
      Segment *seg = segments.add_segment();
      seg->set_key(*test_data.key);
      PushBackCandidate(seg, *test_data.key);
      orig_candidates_size = seg->candidates_size();
    }

    ConversionRequest request;
    request.set_request_type(ConversionRequest::PREDICTION);
    request.set_should_call_set_key_in_prediction(
        test_data.should_call_set_key_in_prediction);

    ASSERT_TRUE(converter->Predict(request, kPredictionKey, &segments));

    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).key(), kPredictionKey);
    if (test_data.expect_set_key_is_called) {
      // If SetKey is called, the segment has only one candidate populated by
      // StubPredictor.
      EXPECT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    } else {
      // If SetKey is not called, the segment has been added one candidate by
      // StubPredictor.
      const int expected_candidates_size = orig_candidates_size + 1;
      EXPECT_EQ(segments.conversion_segment(0).candidates_size(),
                expected_candidates_size);
    }
  }
}

// An action that invokes a DictionaryInterface::Callback with the token whose
// key and value is set to the given ones.
ACTION_P2(InvokeCallbackWithUserDictionaryToken, key, value) {
  // const absl::string_view key = arg0;
  DictionaryInterface::Callback *const callback = arg2;
  const Token token(std::string(key), value, MockDictionary::kDefaultCost,
                    MockDictionary::kDefaultPosId,
                    MockDictionary::kDefaultPosId, Token::USER_DICTIONARY);
  callback->OnToken(key, key, token);
}

TEST_F(ConverterTest, VariantExpansionForSuggestion) {
  // Create Converter with mock user dictionary
  testing::MockDataManager data_manager;
  auto mock_user_dictionary = std::make_unique<MockDictionary>();

  EXPECT_CALL(*mock_user_dictionary, LookupPredictive(_, _, _))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_user_dictionary, LookupPredictive(StrEq("てすと"), _, _))
      .WillRepeatedly(InvokeCallbackWithUserDictionaryToken("てすと", "<>!?"));

  EXPECT_CALL(*mock_user_dictionary, LookupPrefix(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(*mock_user_dictionary, LookupPrefix(StrEq("てすとの"), _, _))
      .WillRepeatedly(InvokeCallbackWithUserDictionaryToken("てすと", "<>!?"));
  auto suppression_dictionary = std::make_unique<SuppressionDictionary>();

  const char *dictionary_data = nullptr;
  int dictionary_size = 0;
  data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);

  const dictionary::PosMatcher pos_matcher(data_manager.GetPosMatcherData());

  std::unique_ptr<SystemDictionary> sysdic =
      SystemDictionary::Builder(dictionary_data, dictionary_size)
          .Build()
          .value();
  auto value_dic =
      std::make_unique<ValueDictionary>(pos_matcher, &sysdic->value_trie());
  auto dictionary = std::make_unique<DictionaryImpl>(
      std::move(sysdic), std::move(value_dic), mock_user_dictionary.get(),
      suppression_dictionary.get(), &pos_matcher);
  PosGroup pos_group(data_manager.GetPosGroupData());
  std::unique_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  Connector connector = Connector::CreateFromDataManager(data_manager).value();
  std::unique_ptr<const Segmenter> segmenter(
      Segmenter::CreateFromDataManager(data_manager));
  SuggestionFilter suggestion_filter(
      SuggestionFilter::CreateOrDie(data_manager.GetSuggestionFilterData()));
  auto immutable_converter = std::make_unique<ImmutableConverterImpl>(
      dictionary.get(), suffix_dictionary.get(), suppression_dictionary.get(),
      connector, segmenter.get(), &pos_matcher, &pos_group, suggestion_filter);
  ConverterImpl converter;
  const DictionaryInterface *kNullDictionary = nullptr;
  converter.Init(&pos_matcher, suppression_dictionary.get(),
                 DefaultPredictor::CreateDefaultPredictor(
                     std::make_unique<DictionaryPredictor>(
                         data_manager, &converter, immutable_converter.get(),
                         dictionary.get(), suffix_dictionary.get(), connector,
                         segmenter.get(), pos_matcher, suggestion_filter),
                     std::make_unique<UserHistoryPredictor>(
                         dictionary.get(), &pos_matcher,
                         suppression_dictionary.get(), false),
                     &converter),
                 std::make_unique<RewriterImpl>(&converter, &data_manager,
                                                &pos_group, kNullDictionary),
                 immutable_converter.get());

  Segments segments;
  {
    // Dictionary suggestion
    EXPECT_TRUE(converter.StartSuggestion(&segments, "てすと"));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_TRUE(FindCandidateByValue("<>!?", segments.conversion_segment(0)));
    EXPECT_FALSE(
        FindCandidateByValue("＜＞！？", segments.conversion_segment(0)));
  }
  {
    // Realtime conversion
    segments.Clear();
    EXPECT_TRUE(converter.StartSuggestion(&segments, "てすとの"));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_TRUE(FindCandidateByValue("<>!?の", segments.conversion_segment(0)));
    EXPECT_FALSE(
        FindCandidateByValue("＜＞！？の", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, ComposerKeySelection) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();
  composer::Table table;
  config::Config config;
  {
    Segments segments;
    composer::Composer composer(&table, &default_request(), &config);
    composer.InsertCharacterPreedit("わたしh");
    ConversionRequest request(&composer, &default_request(), &config);
    request.set_composer_key_selection(ConversionRequest::CONVERSION_KEY);
    ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 2);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "私");
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "h");
  }
  {
    Segments segments;
    composer::Composer composer(&table, &default_request(), &config);
    composer.InsertCharacterPreedit("わたしh");
    ConversionRequest request(&composer, &default_request(), &config);
    request.set_composer_key_selection(ConversionRequest::PREDICTION_KEY);
    ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
    EXPECT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "私");
  }
}

TEST_F(ConverterTest, SuppressionDictionaryForRewriter) {
  std::unique_ptr<ConverterAndData> ret(
      CreateConverterAndDataWithInsertDummyWordsRewriter());

  // Set up suppression dictionary
  ret->suppression_dictionary->Lock();
  ret->suppression_dictionary->AddEntry("tobefiltered", "ToBeFiltered");
  ret->suppression_dictionary->UnLock();
  EXPECT_FALSE(ret->suppression_dictionary->IsEmpty());

  // Convert
  composer::Table table;
  config::Config config;
  composer::Composer composer(&table, &default_request(), &config);
  composer.InsertCharacter("dummy");
  const ConversionRequest request(&composer, &default_request(), &config);
  Segments segments;
  EXPECT_TRUE(ret->converter->StartConversionForRequest(request, &segments));

  // Verify that words inserted by the rewriter is suppressed if its in the
  // suppression_dictionary.
  for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
    const Segment &seg = segments.conversion_segment(i);
    EXPECT_FALSE(FindCandidateByValue("ToBeFiltered", seg));
    EXPECT_TRUE(FindCandidateByValue("NotToBeFiltered", seg));
  }
}

TEST_F(ConverterTest, EmptyConvertReverseIssue8661091) {
  // This is a test case against b/8661091.
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();

  Segments segments;
  EXPECT_FALSE(converter->StartReverseConversion(&segments, ""));
}

TEST_F(ConverterTest, StartReverseConversion) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  const ConverterInterface *converter = engine->GetConverter();

  const std::string kHonKanji = "本";
  const std::string kHonHiragana = "ほん";
  const std::string kMuryouKanji = "無料";
  const std::string kMuryouHiragana = "むりょう";
  const std::string kFullWidthSpace = "　";  // full-width space
  {
    // Test for single Kanji character.
    const std::string &kInput = kHonKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
  }
  {
    // Test for multi-Kanji character.
    const std::string &kInput = kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by a space.
    const std::string &kInput = kHonKanji + " " + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, " ");
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by multiple spaces.
    const std::string &kInput = kHonKanji + "   " + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "   ");
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for leading white spaces.
    const std::string &kInput = "  " + kHonKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 2);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "  ");
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, kHonHiragana);
  }
  {
    // Test for trailing white spaces.
    const std::string &kInput = kMuryouKanji + "  ";
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 2);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value,
              kMuryouHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value, "  ");
  }
  {
    // Test for multi terms separated by a full-width space.
    const std::string &kInput = kHonKanji + kFullWidthSpace + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by two full-width spaces.
    const std::string &kFullWidthSpace2 = kFullWidthSpace + kFullWidthSpace;
    const std::string &kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace2);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for multi terms separated by the mix of full- and half-width spaces.
    const std::string &kFullWidthSpace2 = kFullWidthSpace + " ";
    const std::string &kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(segments.segments_size(), 3);
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kHonHiragana);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(segments.conversion_segment(1).candidate(0).value,
              kFullWidthSpace2);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(segments.conversion_segment(2).candidate(0).value,
              kMuryouHiragana);
  }
  {
    // Test for math expressions; see b/9398304.
    const std::string &kInputHalf = "365*24*60*60*1000=";
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputHalf));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kInputHalf);

    // Test for full-width characters.
    segments.Clear();
    const std::string &kInputFull = "３６５＊２４＊６０＊６０＊１０００＝";
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputFull));
    ASSERT_EQ(segments.segments_size(), 1);
    ASSERT_EQ(segments.conversion_segment(0).candidates_size(), 1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, kInputHalf);
  }
}

TEST_F(ConverterTest, GetLastConnectivePart) {
  std::unique_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_FALSE(converter->GetLastConnectivePart("", &key, &value, &id));
    EXPECT_FALSE(converter->GetLastConnectivePart(" ", &key, &value, &id));
    EXPECT_FALSE(converter->GetLastConnectivePart("  ", &key, &value, &id));
  }

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_TRUE(converter->GetLastConnectivePart("a", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");
    EXPECT_EQ(id,
              converter_and_data->converter->pos_matcher_->GetUniqueNounId());

    EXPECT_TRUE(converter->GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");

    EXPECT_FALSE(converter->GetLastConnectivePart("a  ", &key, &value, &id));

    EXPECT_TRUE(converter->GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");

    EXPECT_TRUE(converter->GetLastConnectivePart("a10a", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "a");

    EXPECT_TRUE(converter->GetLastConnectivePart("ａ", &key, &value, &id));
    EXPECT_EQ(key, "a");
    EXPECT_EQ(value, "ａ");
  }

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_TRUE(converter->GetLastConnectivePart("10", &key, &value, &id));
    EXPECT_EQ(key, "10");
    EXPECT_EQ(value, "10");
    EXPECT_EQ(id, converter_and_data->converter->pos_matcher_->GetNumberId());

    EXPECT_TRUE(converter->GetLastConnectivePart("10a10", &key, &value, &id));
    EXPECT_EQ(key, "10");
    EXPECT_EQ(value, "10");

    EXPECT_TRUE(converter->GetLastConnectivePart("１０", &key, &value, &id));
    EXPECT_EQ(key, "10");
    EXPECT_EQ(value, "１０");
  }

  {
    std::string key;
    std::string value;
    uint16_t id = 0;
    EXPECT_FALSE(converter->GetLastConnectivePart("あ", &key, &value, &id));
  }
}

TEST_F(ConverterTest, ReconstructHistory) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();

  constexpr char kTen[] = "１０";

  Segments segments;
  EXPECT_TRUE(converter->ReconstructHistory(&segments, kTen));
  EXPECT_EQ(segments.segments_size(), 1);
  const Segment &segment = segments.segment(0);
  EXPECT_EQ(segment.segment_type(), Segment::HISTORY);
  EXPECT_EQ(segment.key(), "10");
  EXPECT_EQ(segment.candidates_size(), 1);
  const Segment::Candidate &candidate = segment.candidate(0);
  EXPECT_EQ(candidate.attributes, Segment::Candidate::NO_LEARNING);
  EXPECT_EQ(candidate.content_key, "10");
  EXPECT_EQ(candidate.key, "10");
  EXPECT_EQ(candidate.content_value, kTen);
  EXPECT_EQ(candidate.value, kTen);
  EXPECT_NE(candidate.lid, 0);
  EXPECT_NE(candidate.rid, 0);
}

TEST_F(ConverterTest, LimitCandidatesSize) {
  std::unique_ptr<EngineInterface> engine =
      MockDataEngineFactory::Create().value();
  ConverterInterface *converter = engine->GetConverter();

  composer::Table table;
  const config::Config &config = config::ConfigHandler::DefaultConfig();
  mozc::commands::Request request_proto;
  mozc::composer::Composer composer(&table, &request_proto, &config);
  composer.InsertCharacterPreedit("あ");
  ConversionRequest request(&composer, &request_proto, &config);

  Segments segments;
  ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  const int original_candidates_size = segments.segment(0).candidates_size();
  const int original_meta_candidates_size =
      segments.segment(0).meta_candidates_size();
  EXPECT_LT(0, original_candidates_size - 1 - original_meta_candidates_size)
      << "original candidates size: " << original_candidates_size
      << ", original meta candidates size: " << original_meta_candidates_size;

  segments.Clear();
  request_proto.set_candidates_size_limit(original_candidates_size - 1);
  ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_GE(original_candidates_size - 1,
            segments.segment(0).candidates_size());
  EXPECT_LE(original_candidates_size - 1 - original_meta_candidates_size,
            segments.segment(0).candidates_size());
  EXPECT_EQ(segments.segment(0).meta_candidates_size(),
            original_meta_candidates_size);

  segments.Clear();
  request_proto.set_candidates_size_limit(0);
  ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
  ASSERT_EQ(segments.conversion_segments_size(), 1);
  EXPECT_EQ(segments.segment(0).candidates_size(), 1);
  EXPECT_EQ(segments.segment(0).meta_candidates_size(),
            original_meta_candidates_size);
}

TEST_F(ConverterTest, UserEntryShouldBePromoted) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));

  std::unique_ptr<ConverterAndData> ret =
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, std::make_unique<StubRewriter>(),
          STUB_PREDICTOR);

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "あい"));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(segments.conversion_segment(0).candidate(0).value, "哀");
  }
}

TEST_F(ConverterTest, UserEntryShouldBePromotedMobilePrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));

  std::unique_ptr<ConverterAndData> ret =
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, std::make_unique<StubRewriter>(),
          MOBILE_PREDICTOR);

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartPrediction(&segments, "あい"));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());

    // "哀" should be the top result for the key "あい".
    int first_ai_index = -1;
    for (int i = 0; i < segments.conversion_segment(0).candidates_size(); ++i) {
      if (segments.conversion_segment(0).candidate(i).key == "あい") {
        first_ai_index = i;
        break;
      }
    }
    ASSERT_NE(first_ai_index, -1);
    EXPECT_EQ(segments.conversion_segment(0).candidate(first_ai_index).value,
              "哀");
  }
}

TEST_F(ConverterTest, SuppressionEntryShouldBePrioritized) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::SUPPRESSION_WORD));

  std::unique_ptr<ConverterAndData> ret =
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, std::make_unique<StubRewriter>(),
          STUB_PREDICTOR);

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "あい"));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
    EXPECT_FALSE(FindCandidateByValue("哀", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, SuppressionEntryShouldBePrioritizedPrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::SUPPRESSION_WORD));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < std::size(types); ++i) {
    std::unique_ptr<ConverterAndData> ret =
        CreateConverterAndDataWithUserDefinedEntries(
            user_defined_entries, std::make_unique<StubRewriter>(), types[i]);
    ConverterInterface *converter = ret->converter.get();
    CHECK(converter);
    {
      Segments segments;
      EXPECT_TRUE(converter->StartPrediction(&segments, "あい"));
      ASSERT_EQ(segments.conversion_segments_size(), 1);
      ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
      EXPECT_FALSE(FindCandidateByValue("哀", segments.conversion_segment(0)));
    }
  }
}

TEST_F(ConverterTest, AbbreviationShouldBeIndependent) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::ABBREVIATION));

  std::unique_ptr<ConverterAndData> ret =
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, std::make_unique<StubRewriter>(),
          STUB_PREDICTOR);

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "じゅうじか"));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(
        FindCandidateByValue("Google+うじか", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, AbbreviationShouldBeIndependentPrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::ABBREVIATION));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < std::size(types); ++i) {
    std::unique_ptr<ConverterAndData> ret =
        CreateConverterAndDataWithUserDefinedEntries(
            user_defined_entries, std::make_unique<StubRewriter>(), types[i]);

    ConverterInterface *converter = ret->converter.get();
    CHECK(converter);

    {
      Segments segments;
      EXPECT_TRUE(converter->StartPrediction(&segments, "じゅうじか"));
      ASSERT_EQ(segments.conversion_segments_size(), 1);
      EXPECT_FALSE(FindCandidateByValue("Google+うじか",
                                        segments.conversion_segment(0)));
    }
  }
}

TEST_F(ConverterTest, SuggestionOnlyShouldBeIndependent) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::SUGGESTION_ONLY));

  std::unique_ptr<ConverterAndData> ret =
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, std::make_unique<StubRewriter>(),
          STUB_PREDICTOR);

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "じゅうじか"));
    ASSERT_EQ(segments.conversion_segments_size(), 1);
    EXPECT_FALSE(
        FindCandidateByValue("Google+うじか", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, SuggestionOnlyShouldBeIndependentPrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::SUGGESTION_ONLY));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < std::size(types); ++i) {
    std::unique_ptr<ConverterAndData> ret =
        CreateConverterAndDataWithUserDefinedEntries(
            user_defined_entries, std::make_unique<StubRewriter>(), types[i]);

    ConverterInterface *converter = ret->converter.get();
    CHECK(converter);
    {
      Segments segments;
      EXPECT_TRUE(converter->StartConversion(&segments, "じゅうじか"));
      ASSERT_EQ(segments.conversion_segments_size(), 1);
      EXPECT_FALSE(FindCandidateByValue("Google+うじか",
                                        segments.conversion_segment(0)));
    }
  }
}

TEST_F(ConverterTest, RewriterShouldRespectDefaultCandidates) {
  std::unique_ptr<EngineInterface> engine = CreateEngineWithMobilePredictor();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  commands::Request request;
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  composer::Table table;
  composer::Composer composer(&table, &request, &config);
  commands::RequestForUnitTest::FillMobileRequest(&request);
  ConversionRequest conversion_request(&composer, &request, &config);
  conversion_request.set_request_type(ConversionRequest::PREDICTION);

  Segments segments;
  composer.SetPreeditTextForTestOnly("あい");

  const std::string top_candidate = "合い";
  absl::flat_hash_set<std::string> seen;
  seen.insert(top_candidate);

  // Remember user history 3 times.
  for (int i = 0; i < 3; ++i) {
    segments.Clear();
    EXPECT_TRUE(
        converter->StartPredictionForRequest(conversion_request, &segments));
    const Segment &segment = segments.conversion_segment(0);
    for (int index = 0; index < segment.candidates_size(); ++index) {
      const bool inserted = seen.insert(segment.candidate(index).value).second;
      if (inserted) {
        ASSERT_TRUE(converter->CommitSegmentValue(&segments, 0, index));
        break;
      }
    }
    converter->FinishConversion(conversion_request, &segments);
  }

  segments.Clear();
  EXPECT_TRUE(
      converter->StartPredictionForRequest(conversion_request, &segments));

  int default_candidate_index = -1;
  for (int i = 0; i < segments.conversion_segment(0).candidates_size(); ++i) {
    if (segments.conversion_segment(0).candidate(i).value == top_candidate) {
      default_candidate_index = i;
      break;
    }
  }
  ASSERT_NE(default_candidate_index, -1);
  EXPECT_LE(default_candidate_index, 3);
}

TEST_F(ConverterTest,
       DoNotPromotePrefixOfSingleEntryForEnrichPartialCandidates) {
  std::unique_ptr<EngineInterface> engine = CreateEngineWithMobilePredictor();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  commands::Request request;
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  composer::Table table;
  composer::Composer composer(&table, &request, &config);
  commands::RequestForUnitTest::FillMobileRequest(&request);
  ConversionRequest conversion_request(&composer, &request, &config);
  conversion_request.set_request_type(ConversionRequest::PREDICTION);

  Segments segments;
  composer.SetPreeditTextForTestOnly("おつかれ");

  EXPECT_TRUE(
      converter->StartPredictionForRequest(conversion_request, &segments));

  int o_index = GetCandidateIndexByValue("お", segments.conversion_segment(0));
  int otsukare_index =
      GetCandidateIndexByValue("お疲れ", segments.conversion_segment(0));
  EXPECT_NE(o_index, -1);
  EXPECT_NE(otsukare_index, -1);
  EXPECT_LT(otsukare_index, o_index);
}

TEST_F(ConverterTest, DoNotAddOverlappingNodesForPrediction) {
  std::unique_ptr<EngineInterface> engine = CreateEngineWithMobilePredictor();
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  commands::Request request;
  config::Config config;
  config::ConfigHandler::GetDefaultConfig(&config);
  composer::Table table;
  composer::Composer composer(&table, &request, &config);
  commands::RequestForUnitTest::FillMobileRequest(&request);
  const dictionary::PosMatcher pos_matcher(
      engine->GetDataManager()->GetPosMatcherData());
  ConversionRequest conversion_request(&composer, &request, &config);
  conversion_request.set_request_type(ConversionRequest::PREDICTION);

  Segments segments;
  // History segment.
  {
    Segment *segment = segments.add_segment();
    segment->set_key("に");
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "に";
    candidate->value = "に";
    // Hack: Get POS for "助詞".
    // The POS group of the test dictionary entries, "に" and "にて" should be
    // the same to trigger overlapping lookup.
    candidate->lid = pos_matcher.GetAcceptableParticleAtBeginOfSegmentId();
  }
  composer.SetPreeditTextForTestOnly("てはい");

  EXPECT_TRUE(
      converter->StartPredictionForRequest(conversion_request, &segments));
  EXPECT_FALSE(FindCandidateByValue("て廃", segments.conversion_segment(0)));
}

}  // namespace mozc
