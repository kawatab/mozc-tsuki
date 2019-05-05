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

#include "converter/converter.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node.h"
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
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

namespace mozc {
namespace {

using dictionary::DictionaryImpl;
using dictionary::DictionaryInterface;
using dictionary::DictionaryMock;
using dictionary::PosGroup;
using dictionary::POSMatcher;
using dictionary::SuffixDictionary;
using dictionary::SuppressionDictionary;
using dictionary::SystemDictionary;
using dictionary::Token;
using dictionary::UserDictionaryStub;
using dictionary::ValueDictionary;
using usage_stats::UsageStats;

class StubPredictor : public PredictorInterface {
 public:
  StubPredictor() : predictor_name_("StubPredictor") {}

  bool PredictForRequest(const ConversionRequest &request, Segments *segments)
      const override {
    return true;
  }

  const string &GetPredictorName() const override {
    return predictor_name_;
  }

 private:
  const string predictor_name_;
};

class StubRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override {
    return true;
  }
};

SuffixDictionary *CreateSuffixDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  StringPiece suffix_key_array_data, suffix_value_array_data;
  const uint32 *token_array;
  data_manager.GetSuffixDictionaryData(&suffix_key_array_data,
                                       &suffix_value_array_data,
                                       &token_array);
  return new SuffixDictionary(suffix_key_array_data,
                              suffix_value_array_data,
                              token_array);
}

class InsertDummyWordsRewriter : public RewriterInterface {
  bool Rewrite(const ConversionRequest &, Segments *segments) const override {
    for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
      Segment *seg = segments->mutable_conversion_segment(i);
      {
        Segment::Candidate *cand = seg->add_candidate();
        cand->Init();
        cand->key = "tobefiltered";
        cand->value = "ToBeFiltered";
      }
      {
        Segment::Candidate *cand = seg->add_candidate();
        cand->Init();
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
    const string key;
    const string value;
    const user_dictionary::UserDictionary::PosType pos;
    UserDefinedEntry(const string &k, const string &v,
                     user_dictionary::UserDictionary::PosType p)
        : key(k), value(v), pos(p) {}
  };

  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  ConverterTest() {}
  ~ConverterTest() override {}

  void SetUp() override {
    UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    UsageStats::ClearAllStatsForTest();
  }

  // This struct holds resources used by converter.
  struct ConverterAndData {
    std::unique_ptr<testing::MockDataManager> data_manager;
    std::unique_ptr<DictionaryInterface> user_dictionary;
    std::unique_ptr<SuppressionDictionary> suppression_dictionary;
    std::unique_ptr<DictionaryInterface> suffix_dictionary;
    std::unique_ptr<const Connector> connector;
    std::unique_ptr<const Segmenter> segmenter;
    std::unique_ptr<DictionaryInterface> dictionary;
    std::unique_ptr<const PosGroup> pos_group;
    std::unique_ptr<const SuggestionFilter> suggestion_filter;
    std::unique_ptr<ImmutableConverterInterface> immutable_converter;
    std::unique_ptr<ConverterImpl> converter;
    dictionary::POSMatcher pos_matcher;
  };

  // Returns initialized predictor for the given type.
  // Note that all fields of |converter_and_data| should be filled including
  // |converter_and_data.converter|. |converter| will be initialized using
  // predictor pointer, but predictor need the pointer for |converter| for
  // initializing. Prease see mozc/engine/engine.cc for details.
  // Caller should manage the ownership.
  PredictorInterface *CreatePredictor(
      PredictorType predictor_type,
      const POSMatcher *pos_matcher,
      const ConverterAndData &converter_and_data) {
    if (predictor_type == STUB_PREDICTOR) {
      return new StubPredictor;
    }

    PredictorInterface *(*predictor_factory)(
        PredictorInterface *, PredictorInterface *) = nullptr;
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
    PredictorInterface *dictionary_predictor =
        new DictionaryPredictor(*converter_and_data.data_manager,
                                converter_and_data.converter.get(),
                                converter_and_data.immutable_converter.get(),
                                converter_and_data.dictionary.get(),
                                converter_and_data.suffix_dictionary.get(),
                                converter_and_data.connector.get(),
                                converter_and_data.segmenter.get(),
                                pos_matcher,
                                converter_and_data.suggestion_filter.get());
    CHECK(dictionary_predictor);

    PredictorInterface *user_history_predictor =
        new UserHistoryPredictor(
            converter_and_data.dictionary.get(),
            pos_matcher,
            converter_and_data.suppression_dictionary.get(),
            enable_content_word_learning);
    CHECK(user_history_predictor);

    PredictorInterface *ret_predictor =
        (*predictor_factory)(dictionary_predictor, user_history_predictor);
    CHECK(ret_predictor);
    return ret_predictor;
  }

  // Initializes ConverterAndData with mock data set using given
  // |user_dictionary| and |suppression_dictionary|.
  // ConverterAndData takes ownership of |user_dicationary| and
  // |suppression_dictionary|
  void InitConverterAndData(DictionaryInterface *user_dictionary,
                            SuppressionDictionary *suppression_dictionary,
                            RewriterInterface *rewriter,
                            PredictorType predictor_type,
                            ConverterAndData *converter_and_data) {
    converter_and_data->data_manager.reset(new testing::MockDataManager());
    const auto &data_manager = *converter_and_data->data_manager;

    const char *dictionary_data = nullptr;
    int dictionary_size = 0;
    data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);

    converter_and_data->pos_matcher.Set(data_manager.GetPOSMatcherData());

    SystemDictionary *sysdic =
        SystemDictionary::Builder(dictionary_data, dictionary_size).Build();
    converter_and_data->user_dictionary.reset(user_dictionary);
    converter_and_data->suppression_dictionary.reset(suppression_dictionary);
    converter_and_data->dictionary.reset(new DictionaryImpl(
        sysdic,  // DictionaryImpl takes the ownership
        new ValueDictionary(converter_and_data->pos_matcher,
                            &sysdic->value_trie()),
        converter_and_data->user_dictionary.get(),
        converter_and_data->suppression_dictionary.get(),
        &converter_and_data->pos_matcher));
    converter_and_data->pos_group.reset(
        new PosGroup(data_manager.GetPosGroupData()));
    converter_and_data->suggestion_filter.reset(
        CreateSuggestionFilter(data_manager));
    converter_and_data->suffix_dictionary.reset(
        CreateSuffixDictionaryFromDataManager(data_manager));
    converter_and_data->connector.reset(
        Connector::CreateFromDataManager(data_manager));
    converter_and_data->segmenter.reset(
        Segmenter::CreateFromDataManager(data_manager));
    converter_and_data->immutable_converter.reset(
        new ImmutableConverterImpl(
            converter_and_data->dictionary.get(),
            converter_and_data->suffix_dictionary.get(),
            converter_and_data->suppression_dictionary.get(),
            converter_and_data->connector.get(),
            converter_and_data->segmenter.get(),
            &converter_and_data->pos_matcher,
            converter_and_data->pos_group.get(),
            converter_and_data->suggestion_filter.get()));
    converter_and_data->converter.reset(new ConverterImpl);

    PredictorInterface *predictor = CreatePredictor(
        predictor_type, &converter_and_data->pos_matcher, *converter_and_data);
    converter_and_data->converter->Init(
        &converter_and_data->pos_matcher,
        converter_and_data->suppression_dictionary.get(),
        predictor,
        rewriter,
        converter_and_data->immutable_converter.get());
  }

  ConverterAndData *CreateConverterAndData(RewriterInterface *rewriter,
                                           PredictorType predictor_type) {
    ConverterAndData *ret = new ConverterAndData;
    InitConverterAndData(new UserDictionaryStub, new SuppressionDictionary,
                         rewriter, predictor_type, ret);
    return ret;
  }

  ConverterAndData *CreateStubbedConverterAndData() {
    return CreateConverterAndData(new StubRewriter, STUB_PREDICTOR);
  }

  ConverterAndData *CreateConverterAndDataWithInsertDummyWordsRewriter() {
    return CreateConverterAndData(new InsertDummyWordsRewriter, STUB_PREDICTOR);
  }

  ConverterAndData *CreateConverterAndDataWithUserDefinedEntries(
      const std::vector<UserDefinedEntry> &user_defined_entries,
      RewriterInterface *rewriter, PredictorType predictor_type) {
    ConverterAndData *ret = new ConverterAndData;

    ret->pos_matcher.Set(mock_data_manager_.GetPOSMatcherData());

    SuppressionDictionary *suppression_dictionary = new SuppressionDictionary;
    dictionary::UserDictionary *user_dictionary =
        new dictionary::UserDictionary(
            dictionary::UserPOS::CreateFromDataManager(mock_data_manager_),
            ret->pos_matcher,
            suppression_dictionary);
    InitConverterAndData(
        user_dictionary, suppression_dictionary, rewriter, predictor_type, ret);

    {
      user_dictionary::UserDictionaryStorage storage;
      typedef user_dictionary::UserDictionary::Entry UserEntry;
      user_dictionary::UserDictionary *dictionary = storage.add_dictionaries();
      for (const auto &user_entry : user_defined_entries) {
        UserEntry *entry = dictionary->add_entries();
        entry->set_key(user_entry.key);
        entry->set_value(user_entry.value);
        entry->set_pos(user_entry.pos);
      }

      user_dictionary->Load(storage);
    }
    return ret;
  }

  EngineInterface *CreateEngineWithMobilePredictor() {
    return Engine::CreateMobileEngineHelper<testing::MockDataManager>()
        .release();
  }

  bool FindCandidateByValue(const string &value, const Segment &segment) const {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      if (segment.candidate(i).value == value) {
        return true;
      }
    }
    return false;
  }

  const commands::Request &default_request() const {
    return default_request_;
  }

  static SuggestionFilter *CreateSuggestionFilter(
      const DataManagerInterface &data_manager) {
    const char *data = nullptr;
    size_t size = 0;
    data_manager.GetSuggestionFilterData(&data, &size);
    return new SuggestionFilter(data, size);
  }

 private:
  const testing::ScopedTmpUserProfileDirectory scoped_profile_dir_;
  const testing::MockDataManager mock_data_manager_;
  const commands::Request default_request_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

// test for issue:2209644
// just checking whether this causes segmentation fault or not.
// TODO(toshiyuki): make dictionary mock and test strictly.
TEST_F(ConverterTest, CanConvertTest) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
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
string ContextAwareConvert(const string &first_key,
                           const string &first_value,
                           const string &second_key) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  Segments segments;
  EXPECT_TRUE(converter->StartConversion(&segments, first_key));

  string converted;
  int segment_num = 0;
  while (1) {
    int position = -1;
    for (size_t i = 0; i < segments.segment(segment_num).candidates_size();
         ++i) {
      const string &value = segments.segment(segment_num).candidate(i).value;
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
  EXPECT_TRUE(converter->FinishConversion(default_request, &segments));
  EXPECT_TRUE(converter->StartConversion(&segments, second_key));
  EXPECT_EQ(segment_num + 1, segments.segments_size());

  return segments.segment(segment_num).candidate(0).value;
}
}  // namespace

TEST_F(ConverterTest, ContextAwareConversionTest) {
  // Desirable context aware conversions
  EXPECT_EQ("一髪", ContextAwareConvert("きき", "危機", "いっぱつ"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 2000, 1, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 2000, 1, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 2);

  EXPECT_EQ("大", ContextAwareConvert("きょうと", "京都", "だい"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 4000, 2, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 4000, 2, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 2000, 2, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 4);

  EXPECT_EQ("点", ContextAwareConvert("もんだい", "問題", "てん"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 6000, 3, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 6000, 3, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 3000, 3, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 6);

  EXPECT_EQ("陽水", ContextAwareConvert("いのうえ", "井上", "ようすい"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 8000, 4, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 8000, 4, 2000, 2000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 4000, 4, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 8);

  // Undesirable context aware conversions
  EXPECT_NE("宗号", ContextAwareConvert("19じ", "19時", "しゅうごう"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 11000, 6, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 11000, 5, 2000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 6000, 5, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 11);

  EXPECT_NE("な前", ContextAwareConvert("の", "の", "なまえ"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 12000, 7, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 12000, 6, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 7000, 6, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 12);

  EXPECT_NE("し料", ContextAwareConvert("の", "の", "しりょう"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 13000, 8, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 13000, 7, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 8000, 7, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 13);

  EXPECT_NE("し礼賛", ContextAwareConvert("ぼくと", "僕と", "しらいさん"));
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 15000, 9, 1000, 2000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 15000, 8, 1000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 9000, 8, 1000, 2000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 15);
}

TEST_F(ConverterTest, CommitSegmentValue) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
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
    EXPECT_EQ(2, segments.segments_size());
    EXPECT_EQ(0, segments.history_segments_size());
    EXPECT_EQ(2, segments.conversion_segments_size());
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FIXED_VALUE, segment.segment_type());
    EXPECT_EQ("2", segment.candidate(0).value);
    EXPECT_NE(0,
              segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
  {
    // Make the segment SUBMITTED
    segments.mutable_conversion_segment(0)->set_segment_type(
        Segment::SUBMITTED);
    EXPECT_EQ(2, segments.segments_size());
    EXPECT_EQ(1, segments.history_segments_size());
    EXPECT_EQ(1, segments.conversion_segments_size());
  }
  {
    // Commit the candidate whose value is "3".
    EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 0));
    EXPECT_EQ(2, segments.segments_size());
    EXPECT_EQ(1, segments.history_segments_size());
    EXPECT_EQ(1, segments.conversion_segments_size());
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FIXED_VALUE, segment.segment_type());
    EXPECT_EQ("3", segment.candidate(0).value);
    EXPECT_EQ(0,
              segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
}

TEST_F(ConverterTest, CommitSegments) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
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
    converter->CommitSegments(&segments, index_list);

    EXPECT_EQ(2, segments.history_segments_size());
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(Segment::HISTORY, segments.history_segment(0).segment_type());
    EXPECT_EQ(Segment::SUBMITTED, segments.history_segment(1).segment_type());

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
    converter->CommitSegments(&segments, index_list);

    EXPECT_EQ(3, segments.history_segments_size());
    EXPECT_EQ(0, segments.conversion_segments_size());
    EXPECT_EQ(Segment::HISTORY, segments.history_segment(0).segment_type());
    EXPECT_EQ(Segment::SUBMITTED, segments.history_segment(1).segment_type());
    EXPECT_EQ(Segment::SUBMITTED, segments.history_segment(2).segment_type());

    EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 8000, 3, 2000, 3000);
    EXPECT_TIMING_STATS("SubmittedLengthx1000", 8000, 2, 3000, 5000);
    EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 3000, 2, 1000, 2000);
    EXPECT_COUNT_STATS("SubmittedTotalLength", 8);
  }
}

TEST_F(ConverterTest, CommitPartialSuggestionSegmentValue) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
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
    EXPECT_EQ(3, segments.segments_size());
    EXPECT_EQ(1, segments.history_segments_size());
    EXPECT_EQ(2, segments.conversion_segments_size());
    {
      // The tail segment of the history segments uses
      // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
      // and contains original value.
      const Segment &segment =
          segments.history_segment(segments.history_segments_size() - 1);
      EXPECT_EQ(Segment::SUBMITTED, segment.segment_type());
      EXPECT_EQ("2", segment.candidate(0).value);
      EXPECT_EQ("left2", segment.key());
      EXPECT_NE(0,
                segment.candidate(0).attributes & Segment::Candidate::RERANKED);
    }
    {
      // The head segment of the conversion segments uses |new_segment_key|.
      const Segment &segment = segments.conversion_segment(0);
      EXPECT_EQ(Segment::FREE, segment.segment_type());
      EXPECT_EQ("right2", segment.key());
    }
  }
}

TEST_F(ConverterTest, CommitPartialSuggestionUsageStats) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
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
  EXPECT_EQ(2, segments.history_segments_size());
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(Segment::HISTORY, segments.history_segment(0).segment_type());
  EXPECT_EQ(Segment::SUBMITTED, segments.history_segment(1).segment_type());
  {
    // The tail segment of the history segments uses
    // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
    // and contains original value.
    const Segment &segment =
        segments.history_segment(segments.history_segments_size() - 1);
    EXPECT_EQ(Segment::SUBMITTED, segment.segment_type());
    EXPECT_EQ("格好に", segment.candidate(0).value);
    EXPECT_EQ("かっこうに", segment.candidate(0).key);
    EXPECT_EQ("かつこうに", segment.key());
    EXPECT_NE(0,
              segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
  {
    // The head segment of the conversion segments uses |new_segment_key|.
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FREE, segment.segment_type());
    EXPECT_EQ("いく", segment.key());
  }

  EXPECT_COUNT_STATS("CommitPartialSuggestion", 1);
  EXPECT_TIMING_STATS("SubmittedSegmentLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedLengthx1000", 3000, 1, 3000, 3000);
  EXPECT_TIMING_STATS("SubmittedSegmentNumberx1000", 1000, 1, 1000, 1000);
  EXPECT_COUNT_STATS("SubmittedTotalLength", 3);
}

TEST_F(ConverterTest, CommitAutoPartialSuggestionUsageStats) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
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
  EXPECT_EQ(2, segments.segments_size());
  EXPECT_EQ(1, segments.history_segments_size());
  EXPECT_EQ(1, segments.conversion_segments_size());
  {
    // The tail segment of the history segments uses
    // CommitPartialSuggestionSegmentValue's |current_segment_key| parameter
    // and contains original value.
    const Segment &segment =
        segments.history_segment(segments.history_segments_size() - 1);
    EXPECT_EQ(Segment::SUBMITTED, segment.segment_type());
    EXPECT_EQ("学校に", segment.candidate(0).value);
    EXPECT_EQ("がっこうに", segment.candidate(0).key);
    EXPECT_EQ("かつこうに", segment.key());
    EXPECT_NE(0,
              segment.candidate(0).attributes & Segment::Candidate::RERANKED);
  }
  {
    // The head segment of the conversion segments uses |new_segment_key|.
    const Segment &segment = segments.conversion_segment(0);
    EXPECT_EQ(Segment::FREE, segment.segment_type());
    EXPECT_EQ("いく", segment.key());
  }

  EXPECT_COUNT_STATS("CommitAutoPartialSuggestion", 1);
}

TEST_F(ConverterTest, CandidateKeyTest) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartConversion(&segments, "わたしは"));
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).key);
  EXPECT_EQ("わたし", segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, Regression3437022) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  const string kKey1 = "けいたい";
  const string kKey2 = "でんわ";

  const string kValue1 = "携帯";
  const string kValue2 = "電話";

  {
    // Make sure converte result is one segment
    EXPECT_TRUE(converter->StartConversion(&segments, kKey1 + kKey2));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kValue1 + kValue2,
              segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Make sure we can convert first part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(&segments, kKey1));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kValue1, segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Make sure we can convert last part
    segments.Clear();
    EXPECT_TRUE(converter->StartConversion(&segments, kKey2));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kValue2, segments.conversion_segment(0).candidate(0).value);
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
    rest_size += Util::CharsLen(
        segments.conversion_segment(i).candidate(0).key);
  }

  // Expand segment so that the entire part will become one segment
  if (rest_size > 0) {
    const ConversionRequest default_request;
    EXPECT_TRUE(converter->ResizeSegment(
        &segments, default_request, 0, rest_size));
  }

  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_NE(kValue1 + kValue2,
            segments.conversion_segment(0).candidate(0).value);

  dic->Lock();
  dic->Clear();
  dic->UnLock();
}

TEST_F(ConverterTest, CompletePOSIds) {
  const char *kTestKeys[] = {
      "きょうと",
      "いきます",
      "うつくしい",
      "おおきな",
      "いっちゃわないね",
      "わたしのなまえはなかのです",
  };

  std::unique_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();
  for (size_t i = 0; i < arraysize(kTestKeys); ++i) {
    Segments segments;
    segments.set_request_type(Segments::PREDICTION);
    Segment *seg = segments.add_segment();
    seg->set_key(kTestKeys[i]);
    seg->set_segment_type(Segment::FREE);
    segments.set_max_prediction_candidates_size(20);
    CHECK(converter_and_data->immutable_converter->Convert(&segments));
    const int lid = segments.segment(0).candidate(0).lid;
    const int rid = segments.segment(0).candidate(0).rid;
    Segment::Candidate candidate;
    candidate.value = segments.segment(0).candidate(0).value;
    candidate.key = segments.segment(0).candidate(0).key;
    candidate.lid = 0;
    candidate.rid = 0;
    converter->CompletePOSIds(&candidate);
    EXPECT_EQ(lid, candidate.lid);
    EXPECT_EQ(rid, candidate.rid);
    EXPECT_NE(candidate.lid, 0);
    EXPECT_NE(candidate.rid, 0);
  }

  {
    Segment::Candidate candidate;
    candidate.key = "test";
    candidate.value = "test";
    candidate.lid = 10;
    candidate.rid = 11;
    converter->CompletePOSIds(&candidate);
    EXPECT_EQ(10, candidate.lid);
    EXPECT_EQ(11, candidate.rid);
  }
}

TEST_F(ConverterTest, Regression3046266) {
  // Shouldn't correct nodes at the beginning of a sentence.
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  // Can be any string that has "ん" at the end
  const char kKey1[] = "かん";

  // Can be any string that has a vowel at the beginning
  const char kKey2[] = "あか";

  const char kValueNotExpected[] = "中";

  EXPECT_TRUE(converter->StartConversion(&segments, kKey1));
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_TRUE(converter->CommitSegmentValue(&segments, 0, 0));

  // TODO(team): Use StartConversionForRequest instead of StartConversion.
  const ConversionRequest default_request;
  EXPECT_TRUE(converter->FinishConversion(default_request, &segments));

  EXPECT_TRUE(converter->StartConversion(&segments, kKey2));
  EXPECT_EQ(1, segments.conversion_segments_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_NE(segment.candidate(i).value, kValueNotExpected);
  }
}

TEST_F(ConverterTest, Regression5502496) {
  // Make sure key correction works for the first word of a sentence.
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  Segments segments;

  const char kKey[] = "みんあ";
  const char kValueExpected[] = "みんな";

  EXPECT_TRUE(converter->StartConversion(&segments, kKey));
  EXPECT_EQ(1, segments.conversion_segments_size());
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

  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);

  const string kShi = "し";

  composer::Table table;
  table.AddRule("si", kShi, "");
  table.AddRule("shi", kShi, "");
  config::Config config;

  {
    composer::Composer composer(&table, &client_request, &config);

    composer.InsertCharacter("shi");

    Segments segments;
    const ConversionRequest request(&composer, &client_request, &config);
    EXPECT_TRUE(converter->StartSuggestionForRequest(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ(
        "shi",
        segments.segment(0).meta_candidate(transliteration::HALF_ASCII).value);
  }

  {
    composer::Composer composer(&table, &client_request, &config);

    composer.InsertCharacter("si");

    Segments segments;
    const ConversionRequest request(&composer, &client_request, &config);
    EXPECT_TRUE(converter->StartSuggestionForRequest(request, &segments));
    EXPECT_EQ(1, segments.segments_size());
    ASSERT_TRUE(segments.segment(0).meta_candidates_size() >=
                transliteration::HALF_ASCII);
    EXPECT_EQ(
        "si",
        segments.segment(0).meta_candidate(transliteration::HALF_ASCII).value);
  }
}

TEST_F(ConverterTest, StartPartialPrediction) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialPrediction(&segments, "わたしは"));
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).key);
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, StartPartialSuggestion) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialSuggestion(&segments, "わたしは"));
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).key);
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, StartPartialPrediction_mobile) {
  std::unique_ptr<EngineInterface> engine(CreateEngineWithMobilePredictor());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialPrediction(&segments, "わたしは"));
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).key);
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).content_key);
}

TEST_F(ConverterTest, StartPartialSuggestion_mobile) {
  std::unique_ptr<EngineInterface> engine(CreateEngineWithMobilePredictor());
  ConverterInterface *converter = engine->GetConverter();
  CHECK(converter);
  Segments segments;
  EXPECT_TRUE(converter->StartPartialSuggestion(&segments, "わたしは"));
  EXPECT_EQ(1, segments.segments_size());
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).key);
  EXPECT_EQ("わたしは", segments.segment(0).candidate(0).content_key);
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
  EXPECT_NE(0, (segment.candidate(0).attributes &
                Segment::Candidate::PARTIALLY_KEY_CONSUMED));
  EXPECT_EQ(consumed_key_size, segment.candidate(0).consumed_key_size);
  EXPECT_NE(0, (segment.candidate(1).attributes &
                Segment::Candidate::PARTIALLY_KEY_CONSUMED));
  EXPECT_EQ(original_consumed_key_size,
            segment.candidate(1).consumed_key_size);
  EXPECT_NE(0, (segment.meta_candidate(0).attributes &
                Segment::Candidate::PARTIALLY_KEY_CONSUMED));
  EXPECT_EQ(consumed_key_size, segment.meta_candidate(0).consumed_key_size);
  EXPECT_NE(0, (segment.meta_candidate(1).attributes &
                Segment::Candidate::PARTIALLY_KEY_CONSUMED));
  EXPECT_EQ(original_consumed_key_size,
            segment.meta_candidate(1).consumed_key_size);
}

TEST_F(ConverterTest, Predict_SetKey) {
  const char kPredictionKey[] = "prediction key";
  const char kPredictionKey2[] = "prediction key2";
  // Tests whether SetKey method is called or not.
  struct TestData {
    const Segments::RequestType request_type;
    const char *key;
    const bool expect_reset;
  };
  const TestData test_data_list[] = {
      {Segments::CONVERSION, nullptr, true},
      {Segments::CONVERSION, kPredictionKey, true},
      {Segments::CONVERSION, kPredictionKey2, true},
      {Segments::REVERSE_CONVERSION, nullptr, true},
      {Segments::REVERSE_CONVERSION, kPredictionKey, true},
      {Segments::REVERSE_CONVERSION, kPredictionKey2, true},
      {Segments::PREDICTION, nullptr, true},
      {Segments::PREDICTION, kPredictionKey2, true},
      {Segments::SUGGESTION, nullptr, true},
      {Segments::SUGGESTION, kPredictionKey2, true},
      {Segments::PARTIAL_PREDICTION, nullptr, true},
      {Segments::PARTIAL_PREDICTION, kPredictionKey2, true},
      {Segments::PARTIAL_SUGGESTION, nullptr, true},
      {Segments::PARTIAL_SUGGESTION, kPredictionKey2, true},
      // If we are predicting, and one or more segment exists,
      // and the segments's key equals to the input key, then do not reset.
      {Segments::PREDICTION, kPredictionKey, false},
      {Segments::SUGGESTION, kPredictionKey, true},
      {Segments::PARTIAL_PREDICTION, kPredictionKey, false},
      {Segments::PARTIAL_SUGGESTION, kPredictionKey, true},
  };

  std::unique_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();
  ASSERT_NE(nullptr, converter);

  // Note that TearDown method will reset above stubs.

  for (size_t i = 0; i < arraysize(test_data_list); ++i) {
    const TestData &test_data = test_data_list[i];
    Segments segments;
    segments.set_request_type(test_data.request_type);

    if (test_data.key) {
      Segment *seg = segments.add_segment();
      seg->Clear();
      seg->set_key(test_data.key);
      // The segment has a candidate.
      seg->add_candidate();
    }
    const ConversionRequest request;
    converter->Predict(request, kPredictionKey,
                       Segments::PREDICTION, &segments);

    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ(kPredictionKey, segments.conversion_segment(0).key());
    EXPECT_EQ(test_data.expect_reset ? 0 : 1,
              segments.conversion_segment(0).candidates_size());
  }
}

TEST_F(ConverterTest, VariantExpansionForSuggestion) {
  // Create Converter with mock user dictionary
  testing::MockDataManager data_manager;
  std::unique_ptr<DictionaryMock> mock_user_dictionary(new DictionaryMock);

  mock_user_dictionary->AddLookupPredictive(
      "てすと",
      "てすと",
      "<>!?",
      Token::USER_DICTIONARY);
  mock_user_dictionary->AddLookupPrefix(
      "てすと",
      "てすと",
      "<>!?",
      Token::USER_DICTIONARY);
  std::unique_ptr<SuppressionDictionary> suppression_dictionary(
      new SuppressionDictionary);

  const char *dictionary_data = nullptr;
  int dictionary_size = 0;
  data_manager.GetSystemDictionaryData(&dictionary_data, &dictionary_size);

  const dictionary::POSMatcher pos_matcher(
      data_manager.GetPOSMatcherData());

  SystemDictionary *sysdic =
      SystemDictionary::Builder(dictionary_data, dictionary_size).Build();
  std::unique_ptr<DictionaryInterface> dictionary(new DictionaryImpl(
      sysdic,  // DictionaryImpl takes the ownership
      new ValueDictionary(pos_matcher, &sysdic->value_trie()),
      mock_user_dictionary.get(),
      suppression_dictionary.get(),
      &pos_matcher));
  std::unique_ptr<const PosGroup> pos_group(
      new PosGroup(data_manager.GetPosGroupData()));
  std::unique_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  std::unique_ptr<const Connector> connector(
      Connector::CreateFromDataManager(data_manager));
  std::unique_ptr<const Segmenter> segmenter(
      Segmenter::CreateFromDataManager(data_manager));
  std::unique_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  std::unique_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterImpl(dictionary.get(),
                                 suffix_dictionary.get(),
                                 suppression_dictionary.get(),
                                 connector.get(),
                                 segmenter.get(),
                                 &pos_matcher,
                                 pos_group.get(),
                                 suggestion_filter.get()));
  std::unique_ptr<const SuggestionFilter> suggegstion_filter(
      CreateSuggestionFilter(data_manager));
  std::unique_ptr<ConverterImpl> converter(new ConverterImpl);
  const DictionaryInterface *kNullDictionary = nullptr;
  converter->Init(&pos_matcher,
                  suppression_dictionary.get(),
                  DefaultPredictor::CreateDefaultPredictor(
                      new DictionaryPredictor(
                          data_manager,
                          converter.get(),
                          immutable_converter.get(),
                          dictionary.get(),
                          suffix_dictionary.get(),
                          connector.get(),
                          segmenter.get(),
                          &pos_matcher,
                          suggegstion_filter.get()),
                      new UserHistoryPredictor(dictionary.get(),
                                               &pos_matcher,
                                               suppression_dictionary.get(),
                                               false)),
                  new RewriterImpl(converter.get(),
                                   &data_manager,
                                   pos_group.get(),
                                   kNullDictionary),
                  immutable_converter.get());

  Segments segments;
  {
    // Dictionary suggestion
    EXPECT_TRUE(converter->StartSuggestion(&segments, "てすと"));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_TRUE(FindCandidateByValue("<>!?", segments.conversion_segment(0)));
    EXPECT_FALSE(
        FindCandidateByValue("＜＞！？", segments.conversion_segment(0)));
  }
  {
    // Realtime conversion
    segments.Clear();
    EXPECT_TRUE(converter->StartSuggestion(&segments, "てすとの"));
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_TRUE(FindCandidateByValue("<>!?の", segments.conversion_segment(0)));
    EXPECT_FALSE(
        FindCandidateByValue("＜＞！？の", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, ComposerKeySelection) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();
  composer::Table table;
  config::Config config;
  {
    Segments segments;
    composer::Composer composer(&table, &default_request(), &config);
    composer.InsertCharacterPreedit("わたしh");
    ConversionRequest request(&composer, &default_request(), &config);
    request.set_composer_key_selection(ConversionRequest::CONVERSION_KEY);
    converter->StartConversionForRequest(request, &segments);
    EXPECT_EQ(2, segments.conversion_segments_size());
    EXPECT_EQ("私", segments.conversion_segment(0).candidate(0).value);
    EXPECT_EQ("h", segments.conversion_segment(1).candidate(0).value);
  }
  {
    Segments segments;
    composer::Composer composer(&table, &default_request(), &config);
    composer.InsertCharacterPreedit("わたしh");
    ConversionRequest request(&composer, &default_request(), &config);
    request.set_composer_key_selection(ConversionRequest::PREDICTION_KEY);
    converter->StartConversionForRequest(request, &segments);
    EXPECT_EQ(1, segments.conversion_segments_size());
    EXPECT_EQ("私", segments.conversion_segment(0).candidate(0).value);
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

TEST_F(ConverterTest, EmptyConvertReverse_Issue8661091) {
  // This is a test case against b/8661091.
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();

  Segments segments;
  EXPECT_FALSE(converter->StartReverseConversion(&segments, ""));
}

TEST_F(ConverterTest, StartReverseConversion) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  const ConverterInterface *converter = engine->GetConverter();

  const string kHonKanji = "本";
  const string kHonHiragana = "ほん";
  const string kMuryouKanji = "無料";
  const string kMuryouHiragana = "むりょう";
  const string kFullWidthSpace = "　";  // full-width space
  {
    // Test for single Kanji character.
    const string &kInput = kHonKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Test for multi-Kanji character.
    const string &kInput = kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_LE(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(0).candidate(0).value);
  }
  {
    // Test for multi terms separated by a space.
    const string &kInput = kHonKanji + " " + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(" ", segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for multi terms separated by multiple spaces.
    const string &kInput = kHonKanji + "   " + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ("   ", segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for leading white spaces.
    const string &kInput = "  " + kHonKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(2, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ("  ", segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(1).candidate(0).value);
  }
  {
    // Test for trailing white spaces.
    const string &kInput = kMuryouKanji + "  ";
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(2, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ("  ", segments.conversion_segment(1).candidate(0).value);
  }
  {
    // Test for multi terms separated by a full-width space.
    const string &kInput = kHonKanji + kFullWidthSpace + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kFullWidthSpace,
              segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for multi terms separated by two full-width spaces.
    const string &kFullWidthSpace2 = kFullWidthSpace + kFullWidthSpace;
    const string &kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kFullWidthSpace2,
              segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for multi terms separated by the mix of full- and half-width spaces.
    const string &kFullWidthSpace2 = kFullWidthSpace + " ";
    const string &kInput = kHonKanji + kFullWidthSpace2 + kMuryouKanji;
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInput));
    ASSERT_EQ(3, segments.segments_size());
    ASSERT_LT(0, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kHonHiragana, segments.conversion_segment(0).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(1).candidates_size());
    EXPECT_EQ(kFullWidthSpace2,
              segments.conversion_segment(1).candidate(0).value);
    ASSERT_LT(0, segments.conversion_segment(2).candidates_size());
    EXPECT_EQ(kMuryouHiragana,
              segments.conversion_segment(2).candidate(0).value);
  }
  {
    // Test for math expressions; see b/9398304.
    const string &kInputHalf = "365*24*60*60*1000=";
    Segments segments;
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputHalf));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_EQ(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kInputHalf, segments.conversion_segment(0).candidate(0).value);

    // Test for full-width characters.
    segments.Clear();
    const string &kInputFull = "３６５＊２４＊６０＊６０＊１０００＝";
    EXPECT_TRUE(converter->StartReverseConversion(&segments, kInputFull));
    ASSERT_EQ(1, segments.segments_size());
    ASSERT_EQ(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ(kInputHalf, segments.conversion_segment(0).candidate(0).value);
  }
}

TEST_F(ConverterTest, GetLastConnectivePart) {
  std::unique_ptr<ConverterAndData> converter_and_data(
      CreateStubbedConverterAndData());
  ConverterImpl *converter = converter_and_data->converter.get();

  {
    string key;
    string value;
    uint16 id = 0;
    EXPECT_FALSE(converter->GetLastConnectivePart("", &key, &value, &id));
    EXPECT_FALSE(converter->GetLastConnectivePart(" ", &key, &value, &id));
    EXPECT_FALSE(converter->GetLastConnectivePart("  ", &key, &value, &id));
  }

  {
    string key;
    string value;
    uint16 id = 0;
    EXPECT_TRUE(converter->GetLastConnectivePart("a", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);
    EXPECT_EQ(converter_and_data->converter->pos_matcher_->GetUniqueNounId(),
              id);

    EXPECT_TRUE(converter->GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);

    EXPECT_FALSE(converter->GetLastConnectivePart("a  ", &key, &value, &id));

    EXPECT_TRUE(converter->GetLastConnectivePart("a ", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);

    EXPECT_TRUE(converter->GetLastConnectivePart("a10a", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("a", value);

    EXPECT_TRUE(converter->GetLastConnectivePart("ａ", &key, &value, &id));
    EXPECT_EQ("a", key);
    EXPECT_EQ("ａ", value);
  }

  {
    string key;
    string value;
    uint16 id = 0;
    EXPECT_TRUE(converter->GetLastConnectivePart("10", &key, &value, &id));
    EXPECT_EQ("10", key);
    EXPECT_EQ("10", value);
    EXPECT_EQ(converter_and_data->converter->pos_matcher_->GetNumberId(), id);

    EXPECT_TRUE(converter->GetLastConnectivePart("10a10", &key, &value, &id));
    EXPECT_EQ("10", key);
    EXPECT_EQ("10", value);

    EXPECT_TRUE(converter->GetLastConnectivePart("１０", &key, &value, &id));
    EXPECT_EQ("10", key);
    EXPECT_EQ("１０", value);
  }

  {
    string key;
    string value;
    uint16 id = 0;
    EXPECT_FALSE(converter->GetLastConnectivePart("あ", &key, &value, &id));
  }
}

TEST_F(ConverterTest, ReconstructHistory) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();

  const char kTen[] = "１０";

  Segments segments;
  EXPECT_TRUE(converter->ReconstructHistory(&segments, kTen));
  EXPECT_EQ(1, segments.segments_size());
  const Segment &segment = segments.segment(0);
  EXPECT_EQ(Segment::HISTORY, segment.segment_type());
  EXPECT_EQ("10", segment.key());
  EXPECT_EQ(1, segment.candidates_size());
  const Segment::Candidate &candidate = segment.candidate(0);
  EXPECT_EQ(Segment::Candidate::NO_LEARNING, candidate.attributes);
  EXPECT_EQ("10", candidate.content_key);
  EXPECT_EQ("10", candidate.key);
  EXPECT_EQ(kTen, candidate.content_value);
  EXPECT_EQ(kTen, candidate.value);
  EXPECT_NE(0, candidate.lid);
  EXPECT_NE(0, candidate.rid);
}

TEST_F(ConverterTest, LimitCandidatesSize) {
  std::unique_ptr<EngineInterface> engine(MockDataEngineFactory::Create());
  ConverterInterface *converter = engine->GetConverter();

  composer::Table table;
  const config::Config &config = config::ConfigHandler::DefaultConfig();
  mozc::commands::Request request_proto;
  mozc::composer::Composer composer(&table, &request_proto, &config);
  composer.InsertCharacterPreedit("あ");
  ConversionRequest request(&composer, &request_proto, &config);

  Segments segments;
  ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
  ASSERT_EQ(1, segments.conversion_segments_size());
  const int original_candidates_size =
      segments.segment(0).candidates_size();
  const int original_meta_candidates_size =
      segments.segment(0).meta_candidates_size();
  EXPECT_LT(0, original_candidates_size - 1 - original_meta_candidates_size)
      << "original candidates size: " << original_candidates_size
      << ", original meta candidates size: " << original_meta_candidates_size;

  segments.Clear();
  request_proto.set_candidates_size_limit(original_candidates_size - 1);
  ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
  ASSERT_EQ(1, segments.conversion_segments_size());
  EXPECT_GE(original_candidates_size - 1,
            segments.segment(0).candidates_size());
  EXPECT_LE(original_candidates_size - 1 - original_meta_candidates_size,
            segments.segment(0).candidates_size());
  EXPECT_EQ(original_meta_candidates_size,
            segments.segment(0).meta_candidates_size());

  segments.Clear();
  request_proto.set_candidates_size_limit(0);
  ASSERT_TRUE(converter->StartConversionForRequest(request, &segments));
  ASSERT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(1, segments.segment(0).candidates_size());
  EXPECT_EQ(original_meta_candidates_size,
            segments.segment(0).meta_candidates_size());
}

TEST_F(ConverterTest, UserEntryShouldBePromoted) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));

  std::unique_ptr<ConverterAndData> ret(
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, new StubRewriter, STUB_PREDICTOR));

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "あい"));
    ASSERT_EQ(1, segments.conversion_segments_size());
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
    EXPECT_EQ("哀", segments.conversion_segment(0).candidate(0).value);
  }
}

TEST_F(ConverterTest, UserEntryShouldBePromoted_MobilePrediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));

  std::unique_ptr<ConverterAndData> ret(
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, new StubRewriter, MOBILE_PREDICTOR));

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartPrediction(&segments, "あい"));
    ASSERT_EQ(1, segments.conversion_segments_size());
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());

    // "哀" should be the top result for the key "あい".
    int first_ai_index = -1;
    for (int i = 0; i < segments.conversion_segment(0).candidates_size(); ++i) {
      if (segments.conversion_segment(0).candidate(i).key == "あい") {
        first_ai_index = i;
        break;
      }
    }
    ASSERT_NE(-1, first_ai_index);
    EXPECT_EQ("哀",
              segments.conversion_segment(0).candidate(first_ai_index).value);
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

  std::unique_ptr<ConverterAndData> ret(
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, new StubRewriter, STUB_PREDICTOR));

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "あい"));
    ASSERT_EQ(1, segments.conversion_segments_size());
    ASSERT_LT(1, segments.conversion_segment(0).candidates_size());
    EXPECT_FALSE(FindCandidateByValue("哀", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, SuppressionEntryShouldBePrioritized_Prediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  // "哀" is not in the test dictionary
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::NOUN));
  user_defined_entries.push_back(
      UserDefinedEntry("あい", "哀", UserDictionary::SUPPRESSION_WORD));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < arraysize(types); ++i) {
    std::unique_ptr<ConverterAndData> ret(
        CreateConverterAndDataWithUserDefinedEntries(
            user_defined_entries, new StubRewriter, types[i]));
    ConverterInterface *converter = ret->converter.get();
    CHECK(converter);
    {
      Segments segments;
      EXPECT_TRUE(converter->StartPrediction(&segments, "あい"));
      ASSERT_EQ(1, segments.conversion_segments_size());
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

  std::unique_ptr<ConverterAndData> ret(
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, new StubRewriter, STUB_PREDICTOR));

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "じゅうじか"));
    ASSERT_EQ(1, segments.conversion_segments_size());
    EXPECT_FALSE(
        FindCandidateByValue("Google+うじか", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, AbbreviationShouldBeIndependent_Prediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::ABBREVIATION));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < arraysize(types); ++i) {
    std::unique_ptr<ConverterAndData> ret(
        CreateConverterAndDataWithUserDefinedEntries(
            user_defined_entries, new StubRewriter, types[i]));

    ConverterInterface *converter = ret->converter.get();
    CHECK(converter);

    {
      Segments segments;
      EXPECT_TRUE(converter->StartPrediction(&segments, "じゅうじか"));
      ASSERT_EQ(1, segments.conversion_segments_size());
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

  std::unique_ptr<ConverterAndData> ret(
      CreateConverterAndDataWithUserDefinedEntries(
          user_defined_entries, new StubRewriter, STUB_PREDICTOR));

  ConverterInterface *converter = ret->converter.get();
  CHECK(converter);
  {
    Segments segments;
    EXPECT_TRUE(converter->StartConversion(&segments, "じゅうじか"));
    ASSERT_EQ(1, segments.conversion_segments_size());
    EXPECT_FALSE(
        FindCandidateByValue("Google+うじか", segments.conversion_segment(0)));
  }
}

TEST_F(ConverterTest, SuggestionOnlyShouldBeIndependent_Prediction) {
  using user_dictionary::UserDictionary;
  std::vector<UserDefinedEntry> user_defined_entries;
  user_defined_entries.push_back(
      UserDefinedEntry("じゅ", "Google+", UserDictionary::SUGGESTION_ONLY));

  PredictorType types[] = {DEFAULT_PREDICTOR, MOBILE_PREDICTOR};
  for (int i = 0; i < arraysize(types); ++i) {
    std::unique_ptr<ConverterAndData> ret(
        CreateConverterAndDataWithUserDefinedEntries(
            user_defined_entries, new StubRewriter, types[i]));

    ConverterInterface *converter = ret->converter.get();
    CHECK(converter);
    {
      Segments segments;
      EXPECT_TRUE(converter->StartConversion(&segments, "じゅうじか"));
      ASSERT_EQ(1, segments.conversion_segments_size());
      EXPECT_FALSE(FindCandidateByValue("Google+うじか",
                                        segments.conversion_segment(0)));
    }
  }
}

}  // namespace mozc
