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

#include "prediction/dictionary_prediction_aggregator.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/container/serialized_string_array.h"
#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/internal/typing_model.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "prediction/prediction_aggregator_interface.h"
#include "prediction/result.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "testing/gmock.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "transliteration/transliteration.h"
#include "absl/memory/memory.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace prediction {

class DictionaryPredictionAggregatorTestPeer {
 public:
  DictionaryPredictionAggregatorTestPeer(
      const DataManagerInterface &data_manager,
      const ConverterInterface *converter,
      const ImmutableConverterInterface *immutable_converter,
      const dictionary::DictionaryInterface *dictionary,
      const dictionary::DictionaryInterface *suffix_dictionary,
      const dictionary::PosMatcher *pos_matcher,
      std::unique_ptr<PredictionAggregatorInterface>
          single_kanji_prediction_aggregator,
      const void *user_arg)
      : aggregator_(data_manager, converter, immutable_converter, dictionary,
                    suffix_dictionary, pos_matcher,
                    std::move(single_kanji_prediction_aggregator), user_arg) {}
  virtual ~DictionaryPredictionAggregatorTestPeer() = default;

  PredictionTypes AggregatePredictionForRequest(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const {
    return aggregator_.AggregatePredictionForTesting(request, segments,
                                                     results);
  }

  size_t GetCandidateCutoffThreshold(
      ConversionRequest::RequestType request_type) const {
    return aggregator_.GetCandidateCutoffThreshold(request_type);
  }

  PredictionType AggregateUnigramCandidate(const ConversionRequest &request,
                                           const Segments &segments,
                                           std::vector<Result> *results) const {
    return aggregator_.AggregateUnigramCandidate(request, segments, results);
  }

  PredictionType AggregateUnigramCandidateForMixedConversion(
      const ConversionRequest &request, const Segments &segments,
      std::vector<Result> *results) const {
    return aggregator_.AggregateUnigramCandidateForMixedConversion(
        request, segments, results);
  }

  void AggregateBigramPrediction(const ConversionRequest &request,
                                 const Segments &segments,
                                 Segment::Candidate::SourceInfo source_info,
                                 std::vector<Result> *results) const {
    aggregator_.AggregateBigramPrediction(request, segments, source_info,
                                          results);
  }

  void AggregateRealtimeConversion(const ConversionRequest &request,
                                   size_t realtime_candidates_size,
                                   const Segments &segments,
                                   std::vector<Result> *results) const {
    aggregator_.AggregateRealtimeConversion(request, realtime_candidates_size,
                                            segments, results);
  }

  void AggregateSuffixPrediction(const ConversionRequest &request,
                                 const Segments &segments,
                                 std::vector<Result> *results) const {
    aggregator_.AggregateSuffixPrediction(request, segments, results);
  }

  void AggregateZeroQuerySuffixPrediction(const ConversionRequest &request,
                                          const Segments &segments,
                                          std::vector<Result> *results) const {
    aggregator_.AggregateZeroQuerySuffixPrediction(request, segments, results);
  }

  void AggregateEnglishPrediction(const ConversionRequest &request,
                                  const Segments &segments,
                                  std::vector<Result> *results) const {
    aggregator_.AggregateEnglishPrediction(request, segments, results);
  }

  void AggregateTypeCorrectingPrediction(const ConversionRequest &request,
                                         const Segments &segments,
                                         std::vector<Result> *results) const {
    aggregator_.AggregateTypeCorrectingPrediction(
        request, segments, BIGRAM | UNIGRAM | REALTIME, results);
  }

  size_t GetRealtimeCandidateMaxSize(const ConversionRequest &request,
                                     const Segments &segments,
                                     bool mixed_conversion) const {
    return aggregator_.GetRealtimeCandidateMaxSize(request, segments,
                                                   mixed_conversion);
  }

  static void LookupUnigramCandidateForMixedConversion(
      const dictionary::DictionaryInterface &dictionary,
      const ConversionRequest &request, const Segments &segments,
      int zip_code_id, int unknown_id, std::vector<Result> *results) {
    DictionaryPredictionAggregator::LookupUnigramCandidateForMixedConversion(
        dictionary, request, segments, zip_code_id, unknown_id, results);
  }

  static bool GetZeroQueryCandidatesForKey(
      const ConversionRequest &request, const std::string &key,
      const ZeroQueryDict &dict, std::vector<ZeroQueryResult> *results) {
    return DictionaryPredictionAggregator::GetZeroQueryCandidatesForKey(
        request, key, dict, results);
  }

 private:
  DictionaryPredictionAggregator aggregator_;
};

namespace {

using ::mozc::dictionary::DictionaryInterface;
using ::mozc::dictionary::MockDictionary;
using ::mozc::dictionary::PosMatcher;
using ::mozc::dictionary::SuffixDictionary;
using ::mozc::dictionary::Token;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;
using ::testing::StrEq;
using ::testing::Truly;
using ::testing::WithParamInterface;

// Action to call the third argument of LookupPrefix/LookupPredictive with the
// token <key, value>.
ACTION_P6(InvokeCallbackWithOneToken, key, value, cost, lid, rid, attributes) {
  Token token;
  token.key = key;
  token.value = value;
  token.cost = cost;
  token.lid = lid;
  token.rid = rid;
  token.attributes = attributes;
  arg2->OnToken(key, key, token);
}

ACTION_P(InvokeCallbackWithTokens, token_list) {
  using Callback = DictionaryInterface::Callback;
  Callback *callback = arg2;
  for (const Token &token : token_list) {
    if (callback->OnKey(token.key) != Callback::TRAVERSE_CONTINUE ||
        callback->OnActualKey(token.key, token.key, false) !=
            Callback::TRAVERSE_CONTINUE) {
      return;
    }
    if (callback->OnToken(token.key, token.key, token) !=
        Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

ACTION_P2(InvokeCallbackWithKeyValuesImpl, key_value_list, token_attribute) {
  using Callback = DictionaryInterface::Callback;
  Callback *callback = arg2;
  for (const auto &[key, value] : key_value_list) {
    if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE ||
        callback->OnActualKey(key, key, false) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
    const Token token(key, value, MockDictionary::kDefaultCost,
                      MockDictionary::kDefaultPosId,
                      MockDictionary::kDefaultPosId, token_attribute);
    if (callback->OnToken(key, key, token) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

auto InvokeCallbackWithKeyValues(
    const std::vector<std::pair<std::string, std::string>> &key_value_list,
    Token::Attribute attribute = Token::NONE)
    -> decltype(InvokeCallbackWithKeyValuesImpl(key_value_list, attribute)) {
  return InvokeCallbackWithKeyValuesImpl(key_value_list, attribute);
}

void InitSegmentsWithKey(absl::string_view key, Segments *segments) {
  segments->Clear();

  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void PrependHistorySegments(absl::string_view key, absl::string_view value,
                            Segments *segments) {
  Segment *seg = segments->push_front_segment();
  seg->set_segment_type(Segment::HISTORY);
  seg->set_key(key);
  Segment::Candidate *c = seg->add_candidate();
  c->key.assign(key.data(), key.size());
  c->content_key = c->key;
  c->value.assign(value.data(), value.size());
  c->content_value = c->value;
}

void SetUpInputForSuggestion(absl::string_view key,
                             composer::Composer *composer, Segments *segments) {
  composer->Reset();
  composer->SetPreeditTextForTestOnly(key);
  InitSegmentsWithKey(key, segments);
}

void SetUpInputForSuggestionWithHistory(absl::string_view key,
                                        absl::string_view hist_key,
                                        absl::string_view hist_value,
                                        composer::Composer *composer,
                                        Segments *segments) {
  SetUpInputForSuggestion(key, composer, segments);
  PrependHistorySegments(hist_key, hist_value, segments);
}

void GenerateKeyEvents(absl::string_view text,
                       std::vector<commands::KeyEvent> *keys) {
  keys->clear();
  for (const char32_t w : Util::Utf8ToUtf32(text)) {
    commands::KeyEvent key;
    if (w <= 0x7F) {  // IsAscii, w is unsigned.
      key.set_key_code(w);
    } else {
      key.set_key_code('?');
      Util::Ucs4ToUtf8(w, key.mutable_key_string());
    }
    keys->push_back(key);
  }
}

void InsertInputSequence(absl::string_view text, composer::Composer *composer) {
  std::vector<commands::KeyEvent> keys;
  GenerateKeyEvents(text, &keys);

  for (size_t i = 0; i < keys.size(); ++i) {
    composer->InsertCharacterKeyEvent(keys[i]);
  }
}

void InsertInputSequenceForProbableKeyEvent(absl::string_view text,
                                            const uint32_t *corrected_key_codes,
                                            float corrected_prob,
                                            composer::Composer *composer) {
  std::vector<commands::KeyEvent> keys;
  GenerateKeyEvents(text, &keys);

  for (size_t i = 0; i < keys.size(); ++i) {
    if (keys[i].key_code() != corrected_key_codes[i]) {
      commands::KeyEvent::ProbableKeyEvent *probable_key_event;

      probable_key_event = keys[i].add_probable_key_event();
      probable_key_event->set_key_code(keys[i].key_code());
      probable_key_event->set_probability(1 - corrected_prob);

      probable_key_event = keys[i].add_probable_key_event();
      probable_key_event->set_key_code(corrected_key_codes[i]);
      probable_key_event->set_probability(corrected_prob);
    }
    composer->InsertCharacterKeyEvent(keys[i]);
  }
}

PredictionTypes AddDefaultPredictionTypes(PredictionTypes types,
                                          bool is_mobile) {
  if (!is_mobile) {
    return types;
  }
  return types | REALTIME | PREFIX;
}

bool FindResultByValue(const std::vector<Result> &results,
                       const absl::string_view value) {
  for (const auto &result : results) {
    if (result.value == value && !result.removed) {
      return true;
    }
  }
  return false;
}

DictionaryInterface *CreateSuffixDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  absl::string_view suffix_key_array_data, suffix_value_array_data;
  const uint32_t *token_array;
  data_manager.GetSuffixDictionaryData(&suffix_key_array_data,
                                       &suffix_value_array_data, &token_array);
  return new SuffixDictionary(suffix_key_array_data, suffix_value_array_data,
                              token_array);
}

class MockTypingModel : public mozc::composer::TypingModel {
 public:
  MockTypingModel() : TypingModel(nullptr, 0, nullptr, 0, nullptr) {}
  ~MockTypingModel() override = default;
  int GetCost(absl::string_view key) const override { return 10; }
};

// Simple immutable converter mock for the realtime conversion test
class MockImmutableConverter : public ImmutableConverterInterface {
 public:
  MockImmutableConverter() = default;
  ~MockImmutableConverter() override = default;

  MOCK_METHOD(bool, ConvertForRequest,
              (const ConversionRequest &request, Segments *segments),
              (const override));

  static bool ConvertForRequestImpl(const ConversionRequest &request,
                                    Segments *segments) {
    if (!segments || segments->conversion_segments_size() != 1 ||
        segments->conversion_segment(0).key().empty()) {
      return false;
    }
    const std::string key = segments->conversion_segment(0).key();
    Segment *segment = segments->mutable_conversion_segment(0);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = key;
    candidate->key = key;
    return true;
  }
};

class MockSingleKanjiPredictionAggregator
    : public PredictionAggregatorInterface {
 public:
  MockSingleKanjiPredictionAggregator() = default;
  ~MockSingleKanjiPredictionAggregator() override = default;
  MOCK_METHOD(std::vector<Result>, AggregateResults,
              (const ConversionRequest &request, const Segments &Segments),
              (const override));
};

// Helper class to hold dictionary data and aggregator object.
class MockDataAndAggregator {
 public:
  // Initializes aggregator with the given suffix_dictionary.  When
  // nullptr is passed to the |suffix_dictionary|, MockDataManager's suffix
  // dictionary is used.
  // Note that |suffix_dictionary| is owned by this class.
  void Init(const DictionaryInterface *suffix_dictionary = nullptr,
            const void *user_arg = nullptr) {
    pos_matcher_.Set(data_manager_.GetPosMatcherData());
    mock_dictionary_ = new MockDictionary;
    single_kanji_prediction_aggregator_ =
        new MockSingleKanjiPredictionAggregator;
    dictionary_.reset(mock_dictionary_);
    if (!suffix_dictionary) {
      suffix_dictionary_.reset(
          CreateSuffixDictionaryFromDataManager(data_manager_));
    } else {
      suffix_dictionary_.reset(suffix_dictionary);
    }
    CHECK(suffix_dictionary_.get());

    aggregator_ = std::make_unique<DictionaryPredictionAggregatorTestPeer>(
        data_manager_, &converter_, &mock_immutable_converter_,
        dictionary_.get(), suffix_dictionary_.get(), &pos_matcher_,
        absl::WrapUnique(single_kanji_prediction_aggregator_), user_arg);
  }

  MockDictionary *mutable_dictionary() { return mock_dictionary_; }
  MockConverter *mutable_converter() { return &converter_; }
  MockImmutableConverter *mutable_immutable_converter() {
    return &mock_immutable_converter_;
  }
  MockSingleKanjiPredictionAggregator *
  mutable_single_kanji_prediction_aggregator() {
    return single_kanji_prediction_aggregator_;
  }
  const PosMatcher &pos_matcher() const { return pos_matcher_; }
  const DictionaryPredictionAggregatorTestPeer &aggregator() {
    return *aggregator_;
  }

 private:
  const testing::MockDataManager data_manager_;
  MockConverter converter_;
  MockImmutableConverter mock_immutable_converter_;
  std::unique_ptr<const DictionaryInterface> dictionary_;
  std::unique_ptr<const DictionaryInterface> suffix_dictionary_;
  PosMatcher pos_matcher_;

  MockDictionary *mock_dictionary_;
  MockSingleKanjiPredictionAggregator *single_kanji_prediction_aggregator_;

  std::unique_ptr<DictionaryPredictionAggregatorTestPeer> aggregator_;
};

class DictionaryPredictionAggregatorTest
    : public testing::TestWithTempUserProfile {
 protected:
  void SetUp() override {
    request_ = std::make_unique<commands::Request>();
    config_ = std::make_unique<config::Config>();
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_ = std::make_unique<composer::Table>();
    composer_ = std::make_unique<composer::Composer>(
        table_.get(), request_.get(), config_.get());
    suggestion_convreq_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    suggestion_convreq_->set_request_type(ConversionRequest::SUGGESTION);
    prediction_convreq_ = std::make_unique<ConversionRequest>(
        composer_.get(), request_.get(), config_.get());
    prediction_convreq_->set_request_type(ConversionRequest::PREDICTION);
  }

  static std::unique_ptr<MockDataAndAggregator> CreateAggregatorWithMockData() {
    auto ret = std::make_unique<MockDataAndAggregator>();
    ret->Init();
    AddWordsToMockDic(ret->mutable_dictionary());
    AddDefaultImplToMockImmutableConverter(ret->mutable_immutable_converter());
    return ret;
  }

  static void AddWordsToMockDic(MockDictionary *mock) {
    EXPECT_CALL(*mock, LookupPredictive(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());

    EXPECT_CALL(*mock, LookupPredictive(StrEq("ぐーぐるあ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"ぐーぐるあどせんす", "グーグルアドセンス"},
            {"ぐーぐるあどわーず", "グーグルアドワーズ"},
        }));
    EXPECT_CALL(*mock, LookupPredictive(StrEq("ぐーぐる"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"ぐーぐるあどせんす", "グーグルアドセンス"},
            {"ぐーぐるあどわーず", "グーグルアドワーズ"},
        }));
    EXPECT_CALL(*mock, LookupPrefix(StrEq("ぐーぐる"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"グーグル", "グーグル"},
        }));
    EXPECT_CALL(*mock, LookupPrefix(StrEq("あどせんす"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"アドセンス", "アドセンス"},
        }));
    EXPECT_CALL(*mock, LookupPrefix(StrEq("てすと"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"てすと", "テスト"},
        }));

    // SpellingCorrection entry
    EXPECT_CALL(*mock, LookupPredictive(StrEq("かぷりちょうざ"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues(
            {
                {"かぷりちょーざ", "カプリチョーザ"},
            },
            Token::SPELLING_CORRECTION));

    // user dictionary entry
    EXPECT_CALL(*mock, LookupPredictive(StrEq("ゆーざー"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues(
            {
                {"ゆーざー", "ユーザー"},
            },
            Token::USER_DICTIONARY));

    // Some English entries
    EXPECT_CALL(*mock, LookupPredictive(StrEq("conv"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"converge", "converge"},
            {"converged", "converged"},
            {"convergent", "convergent"},
        }));
    EXPECT_CALL(*mock, LookupPredictive(StrEq("con"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"contraction", "contraction"},
            {"control", "control"},
        }));
  }

  static void AddDefaultImplToMockImmutableConverter(
      MockImmutableConverter *mock) {
    EXPECT_CALL(*mock, ConvertForRequest(_, _))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(MockImmutableConverter::ConvertForRequestImpl));
  }

  std::unique_ptr<composer::Composer> composer_;
  std::unique_ptr<composer::Table> table_;
  std::unique_ptr<config::Config> config_;
  std::unique_ptr<ConversionRequest> suggestion_convreq_;
  std::unique_ptr<ConversionRequest> prediction_convreq_;
  std::unique_ptr<commands::Request> request_;
};

TEST_F(DictionaryPredictionAggregatorTest, OnOffTest) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  {
    // turn off
    Segments segments;
    config_->set_use_dictionary_suggest(false);
    config_->set_use_realtime_conversion(false);

    SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
    std::vector<Result> results;
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              NO_PREDICTION);
  }
  {
    // turn on
    Segments segments;
    config_->set_use_dictionary_suggest(true);
    SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
    std::vector<Result> results;
    EXPECT_NE(NO_PREDICTION, aggregator.AggregatePredictionForRequest(
                                 *suggestion_convreq_, segments, &results));
  }
  {
    // empty query
    Segments segments;
    SetUpInputForSuggestion("", composer_.get(), &segments);
    std::vector<Result> results;
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              NO_PREDICTION);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, PartialSuggestion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  Segment *seg = segments.add_segment();
  seg->set_key("ぐーぐるあ");
  seg->set_segment_type(Segment::FREE);
  suggestion_convreq_->set_request_type(ConversionRequest::PARTIAL_SUGGESTION);
  std::vector<Result> results;
  EXPECT_NE(NO_PREDICTION, aggregator.AggregatePredictionForRequest(
                               *suggestion_convreq_, segments, &results));
}

TEST_F(DictionaryPredictionAggregatorTest,
       PartialSuggestionWithRealtimeConversion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  SetUpInputForSuggestion("ぐーぐるあ", composer_.get(), &segments);
  composer_->MoveCursorLeft();
  segments.mutable_conversion_segment(0)->set_key("ぐーぐる");

  suggestion_convreq_->set_use_actual_converter_for_realtime_conversion(true);
  suggestion_convreq_->set_request_type(ConversionRequest::PARTIAL_SUGGESTION);

  // StartConversion should not be called for partial.
  EXPECT_CALL(*data_and_aggregator->mutable_converter(),
              StartConversionForRequest(_, _))
      .Times(0);
  EXPECT_CALL(*data_and_aggregator->mutable_immutable_converter(),
              ConvertForRequest(_, _))
      .Times(AnyNumber());

  std::vector<Result> results;
  EXPECT_NE(NO_PREDICTION, aggregator.AggregatePredictionForRequest(
                               *suggestion_convreq_, segments, &results));
}

TEST_F(DictionaryPredictionAggregatorTest, BigramTest) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);

  InitSegmentsWithKey("あ", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  // "グーグルアドセンス" will be returned.
  std::vector<Result> results;
  EXPECT_TRUE(BIGRAM | aggregator.AggregatePredictionForRequest(
                           *suggestion_convreq_, segments, &results));
}

TEST_F(DictionaryPredictionAggregatorTest, BigramTestWithZeroQuery) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  request_->set_zero_query_suggestion(true);

  // current query is empty
  InitSegmentsWithKey("", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  std::vector<Result> results;
  EXPECT_TRUE(BIGRAM | aggregator.AggregatePredictionForRequest(
                           *suggestion_convreq_, segments, &results));
}

// Check that previous candidate never be shown at the current candidate.
TEST_F(DictionaryPredictionAggregatorTest, Regression3042706) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  config_->set_use_dictionary_suggest(true);

  InitSegmentsWithKey("だい", &segments);

  // history is "きょうと/京都"
  PrependHistorySegments("きょうと", "京都", &segments);

  std::vector<Result> results;
  EXPECT_TRUE(REALTIME | aggregator.AggregatePredictionForRequest(
                             *suggestion_convreq_, segments, &results));
  for (auto r : results) {
    EXPECT_FALSE(absl::StartsWith(r.value, "京都"));
    EXPECT_TRUE(absl::StartsWith(r.key, "だい"));
  }
}

enum Platform { DESKTOP, MOBILE };

class TriggerConditionsTest : public DictionaryPredictionAggregatorTest,
                              public WithParamInterface<Platform> {};

TEST_P(TriggerConditionsTest, TriggerConditions) {
  bool is_mobile = (GetParam() == MOBILE);

  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  std::vector<Result> results;

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  if (is_mobile) {
    commands::RequestForUnitTest::FillMobileRequest(request_.get());
  }

  // Keys of normal lengths.
  {
    // Unigram is triggered in suggestion and prediction if key length (in UTF8
    // character count) is long enough.
    SetUpInputForSuggestion("てすとだよ", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              AddDefaultPredictionTypes(UNIGRAM, is_mobile));

    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              AddDefaultPredictionTypes(UNIGRAM, is_mobile));
  }

  // Short keys.
  {
    if (is_mobile) {
      // Unigram is triggered even if key length is short.
      SetUpInputForSuggestion("てす", composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                         segments, &results),
                (UNIGRAM | REALTIME | PREFIX));

      EXPECT_EQ(aggregator.AggregatePredictionForRequest(*prediction_convreq_,
                                                         segments, &results),
                (UNIGRAM | REALTIME | PREFIX));
    } else {
      // Unigram is not triggered for SUGGESTION if key length is short.
      SetUpInputForSuggestion("てす", composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                         segments, &results),
                NO_PREDICTION);

      EXPECT_EQ(aggregator.AggregatePredictionForRequest(*prediction_convreq_,
                                                         segments, &results),
                UNIGRAM);
    }
  }

  // Zipcode-like keys.
  {
    SetUpInputForSuggestion("0123", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              NO_PREDICTION);
  }

  // History is short => UNIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだよ", "A", "A", composer_.get(),
                                       &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              AddDefaultPredictionTypes(UNIGRAM, is_mobile));
  }

  // Both history and current segment are long => UNIGRAM or BIGRAM
  {
    SetUpInputForSuggestionWithHistory("てすとだよ", "てすとだよ", "abc",
                                       composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              AddDefaultPredictionTypes(UNIGRAM | BIGRAM, is_mobile));
  }

  // Current segment is short
  {
    if (is_mobile) {
      // For mobile, UNIGRAM and REALTIME are added to BIGRAM.
      SetUpInputForSuggestionWithHistory("A", "てすとだよ", "abc",
                                         composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                         segments, &results),
                (UNIGRAM | BIGRAM | REALTIME | PREFIX));
    } else {
      // No UNIGRAM.
      SetUpInputForSuggestionWithHistory("A", "てすとだよ", "abc",
                                         composer_.get(), &segments);
      composer_->SetInputMode(transliteration::HIRAGANA);
      EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                         segments, &results),
                BIGRAM);
    }
  }

  // Typing correction shouldn't be appended.
  {
    SetUpInputForSuggestion("ｐはよう", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    const auto ret = aggregator.AggregatePredictionForRequest(
        *suggestion_convreq_, segments, &results);
    EXPECT_FALSE(TYPING_CORRECTION & ret);
  }

  // When romaji table is qwerty mobile => ENGLISH is included depending on
  // the language aware input setting.
  {
    const auto orig_input_mode = composer_->GetInputMode();
    const auto orig_table = request_->special_romanji_table();
    const auto orig_lang_aware = request_->language_aware_input();
    const bool orig_use_dictionary_suggest = config_->use_dictionary_suggest();

    SetUpInputForSuggestion("てすとだよ", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HIRAGANA);
    config_->set_use_dictionary_suggest(true);

    // The case where romaji table is set to qwerty.  ENGLISH is turned on if
    // language aware input is enabled.
    for (const auto table :
         {commands::Request::QWERTY_MOBILE_TO_HIRAGANA,
          commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII}) {
      config_->set_use_dictionary_suggest(orig_use_dictionary_suggest);
      request_->set_language_aware_input(orig_lang_aware);
      request_->set_special_romanji_table(orig_table);
      composer_->SetInputMode(orig_input_mode);

      request_->set_special_romanji_table(table);

      // Language aware input is default: No English prediction.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                           segments, &results);
      EXPECT_FALSE(ENGLISH & type);

      // Language aware input is off: No English prediction.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                      segments, &results);
      EXPECT_FALSE(type & ENGLISH);

      // Language aware input is on: English prediction is included.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                      segments, &results);
      EXPECT_TRUE(type & ENGLISH);
    }

    // The case where romaji table is not qwerty.  ENGLISH is turned off
    // regardless of language aware input setting.
    for (const auto table : {
             commands::Request::FLICK_TO_HALFWIDTHASCII,
             commands::Request::FLICK_TO_HIRAGANA,
             commands::Request::GODAN_TO_HALFWIDTHASCII,
             commands::Request::GODAN_TO_HIRAGANA,
             commands::Request::NOTOUCH_TO_HALFWIDTHASCII,
             commands::Request::NOTOUCH_TO_HIRAGANA,
             commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII,
             commands::Request::TOGGLE_FLICK_TO_HIRAGANA,
             commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII,
             commands::Request::TWELVE_KEYS_TO_HIRAGANA,
         }) {
      config_->set_use_dictionary_suggest(orig_use_dictionary_suggest);
      request_->set_language_aware_input(orig_lang_aware);
      request_->set_special_romanji_table(orig_table);
      composer_->SetInputMode(orig_input_mode);

      request_->set_special_romanji_table(table);

      // Language aware input is default.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                           segments, &results);
      EXPECT_FALSE(type & ENGLISH);

      // Language aware input is off.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                      segments, &results);
      EXPECT_FALSE(type & ENGLISH);

      // Language aware input is on.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                      segments, &results);
      EXPECT_FALSE(type & ENGLISH);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    TriggerConditionsForPlatforms, TriggerConditionsTest,
    ::testing::Values(DESKTOP, MOBILE),
    [](const ::testing::TestParamInfo<TriggerConditionsTest::ParamType> &info) {
      if (info.param == DESKTOP) {
        return "DESKTOP";
      }
      return "MOBILE";
    });

TEST_F(DictionaryPredictionAggregatorTest, TriggerConditionsLatinInputMode) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  struct TestCase {
    Platform platform;
    transliteration::TransliterationType input_mode;
  } kTestCases[] = {
      {DESKTOP, transliteration::HALF_ASCII},
      {DESKTOP, transliteration::FULL_ASCII},
      {MOBILE, transliteration::HALF_ASCII},
      {MOBILE, transliteration::FULL_ASCII},
  };

  ConversionRequest partial_suggestion_convreq(*suggestion_convreq_);
  partial_suggestion_convreq.set_request_type(
      ConversionRequest::PARTIAL_SUGGESTION);
  for (const auto &test_case : kTestCases) {
    config::ConfigHandler::GetDefaultConfig(config_.get());
    // Resets to default value.
    // Implementation note: Since the value of |request_| is used to initialize
    // composer_ and convreq_, it is not safe to reset |request_| with new
    // instance.
    request_->Clear();
    const bool is_mobile = test_case.platform == MOBILE;
    if (is_mobile) {
      commands::RequestForUnitTest::FillMobileRequest(request_.get());
    }

    Segments segments;
    std::vector<Result> results;

    // Implementation note: SetUpInputForSuggestion() resets the state of
    // composer. So we have to call SetInputMode() after this method.
    SetUpInputForSuggestion("hel", composer_.get(), &segments);
    composer_->SetInputMode(test_case.input_mode);

    config_->set_use_dictionary_suggest(true);

    // Input mode is Latin(HALF_ASCII or FULL_ASCII) => ENGLISH
    config_->set_use_realtime_conversion(false);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              AddDefaultPredictionTypes(ENGLISH, is_mobile));

    config_->set_use_realtime_conversion(true);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              AddDefaultPredictionTypes(ENGLISH | REALTIME, is_mobile));

    // When dictionary suggest is turned off, English prediction should be
    // disabled.
    config_->set_use_dictionary_suggest(false);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(*suggestion_convreq_,
                                                       segments, &results),
              NO_PREDICTION);

    // Has realtime results for PARTIAL_SUGGESTION request.
    config_->set_use_dictionary_suggest(true);
    EXPECT_EQ(aggregator.AggregatePredictionForRequest(
                  partial_suggestion_convreq, segments, &results),
              REALTIME);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateUnigramCandidate) {
  Segments segments;
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr char kKey[] = "ぐーぐるあ";
  SetUpInputForSuggestion(kKey, composer_.get(), &segments);

  std::vector<Result> results;
  EXPECT_TRUE(UNIGRAM | aggregator.AggregateUnigramCandidate(
                            *suggestion_convreq_, segments, &results));
  EXPECT_FALSE(results.empty());

  for (const auto &result : results) {
    EXPECT_EQ(result.types, UNIGRAM);
    EXPECT_TRUE(absl::StartsWith(result.key, kKey));
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       LookupUnigramCandidateForMixedConversion) {
  constexpr char kHiraganaA[] = "あ";
  constexpr char kHiraganaAA[] = "ああ";
  constexpr auto kCost = MockDictionary::kDefaultCost;
  constexpr auto kPosId = MockDictionary::kDefaultPosId;
  constexpr int kZipcodeId = 100;
  constexpr int kUnknownId = 100;

  const std::vector<Token> a_tokens = {
      // A system dictionary entry "a".
      {kHiraganaA, "a", kCost, kPosId, kPosId, Token::NONE},
      // System dictionary entries "a0", ..., "a9", which are detected as
      // redundant
      // by MaybeRedundant(); see dictionary_predictor.cc.
      {kHiraganaA, "a0", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a1", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a2", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a3", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a4", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a5", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a6", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a7", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a8", kCost, kPosId, kPosId, Token::NONE},
      {kHiraganaA, "a9", kCost, kPosId, kPosId, Token::NONE},
      // A user dictionary entry "aaa".  MaybeRedundant() detects this entry as
      // redundant but it should not be filtered in prediction.
      {kHiraganaA, "aaa", kCost, kPosId, kPosId, Token::USER_DICTIONARY},
      {kHiraganaAA, "bbb", 0, kUnknownId, kUnknownId, Token::USER_DICTIONARY},
  };
  const std::vector<Token> aa_tokens = {
      {kHiraganaAA, "bbb", 0, kUnknownId, kUnknownId, Token::USER_DICTIONARY},
  };
  MockDictionary mock_dict;
  EXPECT_CALL(mock_dict, LookupPredictive(_, _, _)).Times(AnyNumber());
  EXPECT_CALL(mock_dict, LookupPredictive(StrEq(kHiraganaA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens(a_tokens));
  EXPECT_CALL(mock_dict, LookupPredictive(StrEq(kHiraganaAA), _, _))
      .WillRepeatedly(InvokeCallbackWithTokens(aa_tokens));

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  table_->LoadFromFile("system://12keys-hiragana.tsv");
  composer_->SetTable(table_.get());

  {
    // Test prediction from input あ.
    InsertInputSequence(kHiraganaA, composer_.get());
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kHiraganaA);

    std::vector<Result> results;
    DictionaryPredictionAggregatorTestPeer::
        LookupUnigramCandidateForMixedConversion(
            mock_dict, *prediction_convreq_, segments, kZipcodeId, kUnknownId,
            &results);

    // Check if "aaa" is not filtered.
    auto iter = std::find_if(
        results.begin(), results.end(), [&kHiraganaA](const Result &res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 res.IsUserDictionaryResult();
        });
    EXPECT_NE(results.end(), iter);

    // "bbb" is looked up from input "あ" but it will be filtered because it is
    // from user dictionary with unknown POS ID.
    iter = std::find_if(results.begin(), results.end(),
                        [&kHiraganaAA](const Result &res) {
                          return res.key == kHiraganaAA && res.value == "bbb" &&
                                 res.IsUserDictionaryResult();
                        });
    EXPECT_EQ(iter, results.end());
  }

  {
    // Test prediction from input ああ.
    composer_->Reset();
    InsertInputSequence(kHiraganaAA, composer_.get());
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kHiraganaAA);

    std::vector<Result> results;
    DictionaryPredictionAggregatorTestPeer::
        LookupUnigramCandidateForMixedConversion(
            mock_dict, *prediction_convreq_, segments, kZipcodeId, kUnknownId,
            &results);

    // Check if "aaa" is not found as its key is あ.
    auto iter = std::find_if(
        results.begin(), results.end(), [&kHiraganaA](const Result &res) {
          return res.key == kHiraganaA && res.value == "aaa" &&
                 res.IsUserDictionaryResult();
        });
    EXPECT_EQ(iter, results.end());

    // Unlike the above case for "あ", "bbb" is now found because input key is
    // exactly "ああ".
    iter = std::find_if(results.begin(), results.end(),
                        [&kHiraganaAA](const Result &res) {
                          return res.key == kHiraganaAA && res.value == "bbb" &&
                                 res.IsUserDictionaryResult();
                        });
    EXPECT_NE(results.end(), iter);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, MobileUnigram) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  constexpr char kKey[] = "とうきょう";
  SetUpInputForSuggestion(kKey, composer_.get(), &segments);

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    constexpr auto kPosId = MockDictionary::kDefaultPosId;
    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPredictive(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPredictive(StrEq("とうきょう"), _, _))
        .WillRepeatedly(InvokeCallbackWithTokens(std::vector<Token>{
            {"とうきょう", "東京", 100, kPosId, kPosId, Token::NONE},
            {"とうきょう", "TOKYO", 100, kPosId, kPosId, Token::NONE},
            {"とうきょうと", "東京都", 110, kPosId, kPosId, Token::NONE},
            {"とうきょうわん", "東京湾", 120, kPosId, kPosId, Token::NONE},
            {"とうきょうえき", "東京駅", 130, kPosId, kPosId, Token::NONE},
            {"とうきょうべい", "東京ベイ", 140, kPosId, kPosId, Token::NONE},
            {"とうきょうゆき", "東京行", 150, kPosId, kPosId, Token::NONE},
            {"とうきょうしぶ", "東京支部", 160, kPosId, kPosId, Token::NONE},
            {"とうきょうてん", "東京店", 170, kPosId, kPosId, Token::NONE},
            {"とうきょうがす", "東京ガス", 180, kPosId, kPosId, Token::NONE}}));
  }

  std::vector<Result> results;
  aggregator.AggregateUnigramCandidateForMixedConversion(*prediction_convreq_,
                                                         segments, &results);

  EXPECT_TRUE(FindResultByValue(results, "東京"));

  int prefix_count = 0;
  for (const auto &result : results) {
    if (absl::StartsWith(result.value, "東京")) {
      ++prefix_count;
    }
  }
  // Should not have same prefix candidates a lot.
  EXPECT_LE(prefix_count, 6);
}

// We are not sure what should we suggest after the end of sentence for now.
// However, we decided to show zero query suggestion rather than stopping
// zero query completely. Users may be confused if they cannot see suggestion
// window only after the certain conditions.
// TODO(toshiyuki): Show useful zero query suggestions after EOS.
TEST_F(DictionaryPredictionAggregatorTest, DISABLED_MobileZeroQueryAfterEOS) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();

  const struct TestCase {
    const char *key;
    const char *value;
    int rid;
    bool expected_result;
  } kTestcases[] = {
      {"ですよね｡", "ですよね。", pos_matcher.GetEOSSymbolId(), false},
      {"｡", "。", pos_matcher.GetEOSSymbolId(), false},
      {"まるいち", "①", pos_matcher.GetEOSSymbolId(), false},
      {"そう", "そう", pos_matcher.GetGeneralNounId(), true},
      {"そう!", "そう！", pos_matcher.GetGeneralNounId(), false},
      {"むすめ。", "娘。", pos_matcher.GetUniqueNounId(), true},
  };

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  for (const auto &test_case : kTestcases) {
    Segments segments;
    InitSegmentsWithKey("", &segments);

    Segment *seg = segments.push_front_segment();
    seg->set_segment_type(Segment::HISTORY);
    seg->set_key(test_case.key);
    Segment::Candidate *c = seg->add_candidate();
    c->key = test_case.key;
    c->content_key = test_case.key;
    c->value = test_case.value;
    c->content_value = test_case.value;
    c->rid = test_case.rid;

    std::vector<Result> results;
    aggregator.AggregatePredictionForRequest(*prediction_convreq_, segments,
                                             &results);
    EXPECT_EQ(!results.empty(), test_case.expected_result);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateBigramPrediction) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  {
    Segments segments;

    InitSegmentsWithKey("あ", &segments);

    // history is "グーグル"
    constexpr char kHistoryKey[] = "ぐーぐる";
    constexpr char kHistoryValue[] = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    aggregator.AggregateBigramPrediction(*suggestion_convreq_, segments,
                                         Segment::Candidate::SOURCE_INFO_NONE,
                                         &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      // "グーグルアドセンス", "グーグル", "アドセンス"
      // are in the dictionary.
      if (results[i].value == "グーグルアドセンス") {
        EXPECT_FALSE(results[i].removed);
      } else {
        EXPECT_TRUE(results[i].removed);
      }
      EXPECT_EQ(results[i].types, BIGRAM);
      EXPECT_TRUE(absl::StartsWith(results[i].key, kHistoryKey));
      EXPECT_TRUE(absl::StartsWith(results[i].value, kHistoryValue));
      // Not zero query
      EXPECT_FALSE(results[i].source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
    }

    EXPECT_EQ(segments.conversion_segments_size(), 1);
  }

  {
    Segments segments;

    InitSegmentsWithKey("あ", &segments);

    constexpr char kHistoryKey[] = "てす";
    constexpr char kHistoryValue[] = "テス";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    aggregator.AggregateBigramPrediction(*suggestion_convreq_, segments,
                                         Segment::Candidate::SOURCE_INFO_NONE,
                                         &results);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateZeroQueryBigramPrediction) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    Segments segments;

    // Zero query
    InitSegmentsWithKey("", &segments);

    // history is "グーグル"
    constexpr char kHistoryKey[] = "ぐーぐる";
    constexpr char kHistoryValue[] = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    aggregator.AggregateBigramPrediction(
        *suggestion_convreq_, segments,
        Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM, &results);
    EXPECT_FALSE(results.empty());

    for (const auto &result : results) {
      EXPECT_TRUE(absl::StartsWith(result.key, kHistoryKey));
      EXPECT_TRUE(absl::StartsWith(result.value, kHistoryValue));
      // Zero query
      EXPECT_FALSE(result.source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
    }
  }

  {
    constexpr char kHistory[] = "ありがとう";

    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPredictive(_, _, _)).Times(AnyNumber());
    EXPECT_CALL(*mock, LookupPrefix(StrEq(kHistory), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {kHistory, kHistory},
        }));
    EXPECT_CALL(*mock, LookupPredictive(StrEq(kHistory), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({
            {"ありがとうございます", "ありがとうございます"},
            {"ありがとうございます", "ありがとう御座います"},
            {"ありがとうございました", "ありがとうございました"},
            {"ありがとうございました", "ありがとう御座いました"},

            {"ございます", "ございます"},
            {"ございます", "御座います"},
            // ("ございました", "ございました") is not in the dictionary.
            {"ございました", "御座いました"},

            // Word less than 10.
            {"ありがとうね", "ありがとうね"},
            {"ね", "ね"},
        }));
    EXPECT_CALL(*mock, HasKey(StrEq("ございます")))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock, HasKey(StrEq("ございました")))
        .WillRepeatedly(Return(true));

    Segments segments;

    // Zero query
    InitSegmentsWithKey("", &segments);

    PrependHistorySegments(kHistory, kHistory, &segments);

    std::vector<Result> results;

    aggregator.AggregateBigramPrediction(
        *suggestion_convreq_, segments,
        Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM, &results);
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results.size(), 5);

    EXPECT_TRUE(FindResultByValue(results, "ありがとうございます"));
    EXPECT_TRUE(FindResultByValue(results, "ありがとう御座います"));
    EXPECT_TRUE(FindResultByValue(results, "ありがとう御座いました"));
    // "ございました" is not in the dictionary, but suggested
    // because it is used as the key of other words (i.e. 御座いました).
    EXPECT_TRUE(FindResultByValue(results, "ありがとうございました"));
    // "ね" is in the dictionary, but filtered due to the word length.
    EXPECT_FALSE(FindResultByValue(results, "ありがとうね"));

    for (const auto &result : results) {
      EXPECT_TRUE(absl::StartsWith(result.key, kHistory));
      EXPECT_TRUE(absl::StartsWith(result.value, kHistory));
      // Zero query
      EXPECT_FALSE(result.source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
      if (result.key == "ありがとうね") {
        EXPECT_TRUE(result.removed);
      } else {
        EXPECT_FALSE(result.removed);
      }
    }
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       AggregateZeroQueryPredictionLatinInputMode) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // No history
    constexpr char kHistoryKey[] = "";
    constexpr char kHistoryValue[] = "";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    constexpr char kHistoryKey[] = "when";
    constexpr char kHistoryValue[] = "when";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_TRUE(results.empty());
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input numbers from Latin input mode.
    constexpr char kHistoryKey[] = "12";
    constexpr char kHistoryValue[] = "12";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }

  {
    Segments segments;

    // Zero query
    SetUpInputForSuggestion("", composer_.get(), &segments);
    composer_->SetInputMode(transliteration::HALF_ASCII);

    // We can input some symbols from Latin input mode.
    constexpr char kHistoryKey[] = "@";
    constexpr char kHistoryValue[] = "@";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<Result> results;

    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());  // Should have results.
  }
}

TEST_F(DictionaryPredictionAggregatorTest, GetRealtimeCandidateMaxSize) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;

  // GetRealtimeCandidateMaxSize has some heuristics so here we test following
  // conditions.
  // - The result must be equal or less than kMaxSize;
  // - If mixed_conversion is the same, the result of SUGGESTION is
  //        equal or less than PREDICTION.
  // - If mixed_conversion is the same, the result of PARTIAL_SUGGESTION is
  //        equal or less than PARTIAL_PREDICTION.
  // - Partial version has equal or greater than non-partial version.

  constexpr size_t kMaxSize = 100;
  segments.push_back_segment();
  suggestion_convreq_->set_max_dictionary_prediction_candidates_size(kMaxSize);

  // non-partial, non-mixed-conversion
  const size_t prediction_no_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *prediction_convreq_, segments, false);
  EXPECT_GE(kMaxSize, prediction_no_mixed);

  const size_t suggestion_no_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *suggestion_convreq_, segments, false);
  EXPECT_GE(kMaxSize, suggestion_no_mixed);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);

  // non-partial, mixed-conversion
  const size_t prediction_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *prediction_convreq_, segments, true);
  EXPECT_GE(kMaxSize, prediction_mixed);

  const size_t suggestion_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *suggestion_convreq_, segments, true);
  EXPECT_GE(kMaxSize, suggestion_mixed);

  // partial, non-mixed-conversion
  ConversionRequest partial_suggestion_convreq(*suggestion_convreq_);
  partial_suggestion_convreq.set_request_type(
      ConversionRequest::PARTIAL_SUGGESTION);
  ConversionRequest partial_prediction_convreq(*prediction_convreq_);
  partial_prediction_convreq.set_request_type(
      ConversionRequest::PARTIAL_PREDICTION);

  const size_t partial_prediction_no_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_prediction_convreq,
                                             segments, false);
  EXPECT_GE(kMaxSize, partial_prediction_no_mixed);

  const size_t partial_suggestion_no_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_suggestion_convreq,
                                             segments, false);
  EXPECT_GE(kMaxSize, partial_suggestion_no_mixed);
  EXPECT_LE(partial_suggestion_no_mixed, partial_prediction_no_mixed);

  // partial, mixed-conversion
  const size_t partial_prediction_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_prediction_convreq,
                                             segments, true);
  EXPECT_GE(kMaxSize, partial_prediction_mixed);

  const size_t partial_suggestion_mixed =
      aggregator.GetRealtimeCandidateMaxSize(partial_suggestion_convreq,
                                             segments, true);
  EXPECT_GE(kMaxSize, partial_suggestion_mixed);
  EXPECT_LE(partial_suggestion_mixed, partial_prediction_mixed);

  EXPECT_GE(partial_prediction_no_mixed, prediction_no_mixed);
  EXPECT_GE(partial_prediction_mixed, prediction_mixed);
  EXPECT_GE(partial_suggestion_no_mixed, suggestion_no_mixed);
  EXPECT_GE(partial_suggestion_mixed, suggestion_mixed);
}

TEST_F(DictionaryPredictionAggregatorTest,
       GetRealtimeCandidateMaxSizeForMixed) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;
  Segment *segment = segments.add_segment();

  constexpr size_t kMaxSize = 100;
  suggestion_convreq_->set_max_dictionary_prediction_candidates_size(kMaxSize);
  prediction_convreq_->set_max_dictionary_prediction_candidates_size(kMaxSize);

  // for short key, try to provide many results as possible
  segment->set_key("short");
  const size_t short_suggestion_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *suggestion_convreq_, segments, true);
  EXPECT_GE(kMaxSize, short_suggestion_mixed);

  const size_t short_prediction_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *prediction_convreq_, segments, true);
  EXPECT_GE(kMaxSize, short_prediction_mixed);

  // for long key, provide few results
  segment->set_key("long_request_key");
  const size_t long_suggestion_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *suggestion_convreq_, segments, true);
  EXPECT_GE(kMaxSize, long_suggestion_mixed);
  EXPECT_GT(short_suggestion_mixed, long_suggestion_mixed);

  const size_t long_prediction_mixed = aggregator.GetRealtimeCandidateMaxSize(
      *prediction_convreq_, segments, true);
  EXPECT_GE(kMaxSize, long_prediction_mixed);
  EXPECT_GT(kMaxSize, long_prediction_mixed + long_suggestion_mixed);
  EXPECT_GT(short_prediction_mixed, long_prediction_mixed);
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateRealtimeConversion) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init();

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr char kKey[] = "わたしのなまえはなかのです";

  // Set up mock converter
  {
    // Make segments like:
    // "わたしの"    | "なまえは" | "なかのです"
    // "Watashino" | "Namaeha" | "Nakanodesu"
    Segments segments;

    auto add_segment = [&segments](absl::string_view key,
                                   absl::string_view value) {
      Segment *segment = segments.add_segment();
      segment->set_key(key);
      Segment::Candidate *candidate = segment->add_candidate();
      candidate->key = std::string(key);
      candidate->value = std::string(value);
    };

    add_segment("わたしの", "Watashino");
    add_segment("なまえは", "Namaeha");
    add_segment("なかのです", "Nakanodesu");

    EXPECT_CALL(*data_and_aggregator->mutable_converter(),
                StartConversionForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }
  // Set up mock immutable converter
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key("わたしのなまえはなかのです");
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "私の名前は中野です";
    candidate->key = ("わたしのなまえはなかのです");
    // "わたしの, 私の", "わたし, 私"
    candidate->PushBackInnerSegmentBoundary(12, 6, 9, 3);
    // "なまえは, 名前は", "なまえ, 名前"
    candidate->PushBackInnerSegmentBoundary(12, 9, 9, 6);
    // "なかのです, 中野です", "なかの, 中野"
    candidate->PushBackInnerSegmentBoundary(15, 12, 9, 6);
    //    data_and_aggregator->mutable_immutable_converter()->Reset();
    EXPECT_CALL(*data_and_aggregator->mutable_immutable_converter(),
                ConvertForRequest(_, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  // A test case with use_actual_converter_for_realtime_conversion being
  // false, i.e., realtime conversion result is generated by
  // ImmutableConverterMock.
  {
    Segments segments;

    InitSegmentsWithKey(kKey, &segments);

    // User history predictor can add candidates before dictionary predictor
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history1";
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history2";

    std::vector<Result> results;
    suggestion_convreq_->set_use_actual_converter_for_realtime_conversion(
        false);

    aggregator.AggregateRealtimeConversion(*suggestion_convreq_, 10, segments,
                                           &results);
    ASSERT_EQ(results.size(), 1);
    EXPECT_EQ(results[0].types, REALTIME);
    EXPECT_EQ(results[0].key, kKey);
    EXPECT_EQ(results[0].inner_segment_boundary.size(), 3);
  }

  // A test case with use_actual_converter_for_realtime_conversion being
  // true, i.e., realtime conversion result is generated by MockConverter.
  {
    Segments segments;

    InitSegmentsWithKey(kKey, &segments);

    // User history predictor can add candidates before dictionary predictor
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history1";
    segments.mutable_conversion_segment(0)->add_candidate()->value = "history2";

    std::vector<Result> results;
    suggestion_convreq_->set_use_actual_converter_for_realtime_conversion(true);

    aggregator.AggregateRealtimeConversion(*suggestion_convreq_, 10, segments,
                                           &results);

    // When |request.use_actual_converter_for_realtime_conversion| is true,
    // the extra label REALTIME_TOP is expected to be added.
    ASSERT_EQ(2, results.size());
    bool realtime_top_found = false;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].types, REALTIME | REALTIME_TOP);
      if (results[i].key == kKey &&
          results[i].value == "WatashinoNamaehaNakanodesu" &&
          results[i].inner_segment_boundary.size() == 3) {
        realtime_top_found = true;
        break;
      }
    }
    EXPECT_TRUE(realtime_top_found);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, GetCandidateCutoffThreshold) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;

  const size_t prediction =
      aggregator.GetCandidateCutoffThreshold(ConversionRequest::PREDICTION);
  const size_t suggestion =
      aggregator.GetCandidateCutoffThreshold(ConversionRequest::SUGGESTION);
  EXPECT_LE(suggestion, prediction);
}

namespace {
struct SimpleSuffixToken {
  const char *key;
  const char *value;
};

const SimpleSuffixToken kSuffixTokens[] = {{"いか", "以下"}};

class TestSuffixDictionary : public DictionaryInterface {
 public:
  TestSuffixDictionary() = default;
  ~TestSuffixDictionary() override = default;

  bool HasKey(absl::string_view value) const override { return false; }

  bool HasValue(absl::string_view value) const override { return false; }

  void LookupPredictive(absl::string_view key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override {
    Token token;
    for (size_t i = 0; i < std::size(kSuffixTokens); ++i) {
      const SimpleSuffixToken &suffix_token = kSuffixTokens[i];
      if (!key.empty() && !absl::StartsWith(suffix_token.key, key)) {
        continue;
      }
      switch (callback->OnKey(suffix_token.key)) {
        case Callback::TRAVERSE_DONE:
          return;
        case Callback::TRAVERSE_NEXT_KEY:
          continue;
        case Callback::TRAVERSE_CULL:
          LOG(FATAL) << "Culling is not supported.";
          break;
        default:
          break;
      }
      token.key = suffix_token.key;
      token.value = suffix_token.value;
      token.cost = 1000;
      token.lid = token.rid = 0;
      if (callback->OnToken(token.key, token.key, token) ==
          Callback::TRAVERSE_DONE) {
        break;
      }
    }
  }

  void LookupPrefix(absl::string_view key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override {}

  void LookupExact(absl::string_view key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override {}

  void LookupReverse(absl::string_view str,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override {}
};

}  // namespace

TEST_F(DictionaryPredictionAggregatorTest, AggregateSuffixPrediction) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init(new TestSuffixDictionary());

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  Segments segments;

  // history is "グーグル"
  constexpr char kHistoryKey[] = "ぐーぐる";
  constexpr char kHistoryValue[] = "グーグル";

  // Since SuffixDictionary only returns for key "い", the result
  // should be empty for "あ".
  std::vector<Result> results;
  SetUpInputForSuggestionWithHistory("あ", kHistoryKey, kHistoryValue,
                                     composer_.get(), &segments);
  aggregator.AggregateSuffixPrediction(*suggestion_convreq_, segments,
                                       &results);
  EXPECT_TRUE(results.empty());

  // Candidates generated by AggregateSuffixPrediction from nonempty
  // key should have SUFFIX type.
  results.clear();
  SetUpInputForSuggestionWithHistory("い", kHistoryKey, kHistoryValue,
                                     composer_.get(), &segments);
  aggregator.AggregateSuffixPrediction(*suggestion_convreq_, segments,
                                       &results);
  EXPECT_FALSE(results.empty());
  for (const auto &result : results) {
    EXPECT_EQ(result.types, SUFFIX);
    // Not zero query
    EXPECT_FALSE(Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX &
                 result.source_info);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, AggregateZeroQuerySuffixPrediction) {
  auto data_and_aggregator = std::make_unique<MockDataAndAggregator>();
  data_and_aggregator->Init(new TestSuffixDictionary());

  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;

  // Zero query
  InitSegmentsWithKey("", &segments);

  // history is "グーグル"
  constexpr char kHistoryKey[] = "ぐーぐる";
  constexpr char kHistoryValue[] = "グーグル";

  PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

  {
    std::vector<Result> results;

    // Candidates generated by AggregateZeroQuerySuffixPrediction should
    // have SUFFIX type.
    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].types, SUFFIX);
      // Zero query
      EXPECT_TRUE(Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX &
                  results[i].source_info);
    }
  }
  {
    // If the feature is disabled and `results` is nonempty, nothing should be
    // generated.
    request_->mutable_decoder_experiment_params()
        ->set_disable_zero_query_suffix_prediction(true);
    std::vector<Result> results = {Result()};
    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_EQ(results.size(), 1);
  }
}

struct EnglishPredictionTestEntry {
  std::string name;
  transliteration::TransliterationType input_mode;
  std::string key;
  std::string expected_prefix;
  std::vector<std::string> expected_values;
};

class AggregateEnglishPredictionTest
    : public DictionaryPredictionAggregatorTest,
      public WithParamInterface<EnglishPredictionTestEntry> {};

TEST_P(AggregateEnglishPredictionTest, AggregateEnglishPrediction) {
  const EnglishPredictionTestEntry &entry = GetParam();
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->Reset();
  composer_->SetTable(table_.get());
  composer_->SetInputMode(entry.input_mode);
  InsertInputSequence(entry.key, composer_.get());

  Segments segments;
  InitSegmentsWithKey(entry.key, &segments);

  std::vector<Result> results;
  aggregator.AggregateEnglishPrediction(*prediction_convreq_, segments,
                                        &results);

  std::set<std::string> values;
  for (const auto &result : results) {
    EXPECT_EQ(result.types, ENGLISH);
    EXPECT_TRUE(absl::StartsWith(result.value, entry.expected_prefix))
        << result.value << " doesn't start with " << entry.expected_prefix;
    values.insert(result.value);
  }
  for (const auto &expected_value : entry.expected_values) {
    EXPECT_TRUE(values.find(expected_value) != values.end())
        << expected_value << " isn't in the results";
  }
}

const std::vector<EnglishPredictionTestEntry> *kEnglishPredictionTestEntries =
    new std::vector<EnglishPredictionTestEntry>(
        {{"HALF_ASCII_lower_case",
          transliteration::HALF_ASCII,
          "conv",
          "conv",
          {"converge", "converged", "convergent"}},
         {"HALF_ASCII_upper_case",
          transliteration::HALF_ASCII,
          "CONV",
          "CONV",
          {"CONVERGE", "CONVERGED", "CONVERGENT"}},
         {"HALF_ASCII_capitalized",
          transliteration::HALF_ASCII,
          "Conv",
          "Conv",
          {"Converge", "Converged", "Convergent"}},
         {"FULL_ASCII_lower_case",
          transliteration::FULL_ASCII,
          "conv",
          "ｃｏｎｖ",
          {"ｃｏｎｖｅｒｇｅ", "ｃｏｎｖｅｒｇｅｄ", "ｃｏｎｖｅｒｇｅｎｔ"}},
         {"FULL_ASCII_upper_case",
          transliteration::FULL_ASCII,
          "CONV",
          "ＣＯＮＶ",
          {"ＣＯＮＶＥＲＧＥ", "ＣＯＮＶＥＲＧＥＤ", "ＣＯＮＶＥＲＧＥＮＴ"}},
         {"FULL_ASCII_capitalized",
          transliteration::FULL_ASCII,
          "Conv",
          "Ｃｏｎｖ",
          {"Ｃｏｎｖｅｒｇｅ", "Ｃｏｎｖｅｒｇｅｄ", "Ｃｏｎｖｅｒｇｅｎｔ"}}});

INSTANTIATE_TEST_SUITE_P(
    AggregateEnglishPredictioForInputMode, AggregateEnglishPredictionTest,
    ::testing::ValuesIn(*kEnglishPredictionTestEntries),
    [](const ::testing::TestParamInfo<AggregateEnglishPredictionTest::ParamType>
           &info) { return info.param.name; });

TEST_F(DictionaryPredictionAggregatorTest, AggregateTypeCorrectingPrediction) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  constexpr char kInputText[] = "gu-huru";
  constexpr uint32_t kCorrectedKeyCodes[] = {'g', 'u', '-', 'g', 'u', 'r', 'u'};
  const char *kExpectedValues[] = {
      "グーグルアドセンス",
      "グーグルアドワーズ",
  };

  config_->set_use_typing_correction(true);
  request_->set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HIRAGANA);
  table_->LoadFromFile("system://qwerty_mobile-hiragana.tsv");
  table_->SetTypingModelForTesting(std::make_unique<MockTypingModel>());
  InsertInputSequenceForProbableKeyEvent(kInputText, kCorrectedKeyCodes, 0.1f,
                                         composer_.get());
  Segments segments;
  InitSegmentsWithKey(kInputText, &segments);

  std::vector<Result> results;
  aggregator.AggregateTypeCorrectingPrediction(*prediction_convreq_, segments,
                                               &results);

  std::set<std::string> values;
  for (const auto &result : results) {
    EXPECT_EQ(result.types, TYPING_CORRECTION);
    values.insert(result.value);
  }
  for (const auto expected_value : kExpectedValues) {
    EXPECT_TRUE(values.find(expected_value) != values.end())
        << expected_value << " isn't in the results";
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       AggregateTypeCorrectingPredictionWithDiffCost) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_use_typing_correction_diff_cost(true);

  constexpr char kInputText[] = "gu-huru";
  constexpr uint32_t kCorrectedKeyCodes[] = {'g', 'u', '-', 'g', 'u', 'r', 'u'};
  const char *kExpectedValues[] = {
      "グーグルアドセンス",
      "グーグルアドワーズ",
  };

  config_->set_use_typing_correction(true);
  request_->set_special_romanji_table(
      commands::Request::QWERTY_MOBILE_TO_HIRAGANA);
  table_->LoadFromFile("system://qwerty_mobile-hiragana.tsv");
  table_->SetTypingModelForTesting(std::make_unique<MockTypingModel>());
  // Correctd key may have smaller query cost.
  InsertInputSequenceForProbableKeyEvent(kInputText, kCorrectedKeyCodes, 0.8f,
                                         composer_.get());
  Segments segments;
  InitSegmentsWithKey(kInputText, &segments);

  std::vector<Result> results;
  aggregator.AggregateTypeCorrectingPrediction(*prediction_convreq_, segments,
                                               &results);

  std::set<std::string> values;
  for (const auto &result : results) {
    EXPECT_EQ(result.types, TYPING_CORRECTION);
    // Should not have the smaller cost than the original entry wcost (=
    // 0).
    EXPECT_GE(result.wcost, 0);
    values.insert(result.value);
  }
  for (const auto expected_value : kExpectedValues) {
    EXPECT_TRUE(values.find(expected_value) != values.end())
        << expected_value << " isn't in the results";
  }
}

TEST_F(DictionaryPredictionAggregatorTest, ZeroQuerySuggestionAfterNumbers) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();
  Segments segments;

  {
    InitSegmentsWithKey("", &segments);

    constexpr char kHistoryKey[] = "12";
    constexpr char kHistoryValue[] = "12";
    constexpr char kExpectedValue[] = "月";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<Result> results;
    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    auto target = results.end();
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, SUFFIX);

      EXPECT_TRUE(
          Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX &
          it->source_info);

      if (it->value == kExpectedValue) {
        target = it;
        break;
      }
    }
    EXPECT_NE(results.end(), target);
    EXPECT_EQ(target->value, kExpectedValue);
    EXPECT_EQ(target->lid, pos_matcher.GetCounterSuffixWordId());
    EXPECT_EQ(target->rid, pos_matcher.GetCounterSuffixWordId());
  }

  {
    InitSegmentsWithKey("", &segments);

    constexpr char kHistoryKey[] = "66050713";  // A random number
    constexpr char kHistoryValue[] = "66050713";
    constexpr char kExpectedValue[] = "個";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<Result> results;
    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, SUFFIX);
      if (it->value == kExpectedValue) {
        EXPECT_TRUE(
            Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX &
            it->source_info);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, TriggerNumberZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();

  const struct TestCase {
    const char *history_key;
    const char *history_value;
    const char *find_suffix_value;
    bool expected_result;
  } kTestCases[] = {
      {"12", "12", "月", true},      {"12", "１２", "月", true},
      {"12", "壱拾弐", "月", false}, {"12", "十二", "月", false},
      {"12", "一二", "月", false},   {"12", "Ⅻ", "月", false},
      {"あか", "12", "月", true},    // T13N
      {"あか", "１２", "月", true},  // T13N
      {"じゅう", "10", "時", true},  {"じゅう", "１０", "時", true},
      {"じゅう", "十", "時", false}, {"じゅう", "拾", "時", false},
  };

  for (const auto &test_case : kTestCases) {
    Segments segments;
    InitSegmentsWithKey("", &segments);

    PrependHistorySegments(test_case.history_key, test_case.history_value,
                           &segments);
    std::vector<Result> results;
    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (auto it = results.begin(); it != results.end(); ++it) {
      EXPECT_EQ(it->types, SUFFIX);
      if (it->value == test_case.find_suffix_value &&
          it->lid == pos_matcher.GetCounterSuffixWordId()) {
        EXPECT_TRUE(
            Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX &
            it->source_info);
        found = true;
        break;
      }
    }
    EXPECT_EQ(found, test_case.expected_result) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictionAggregatorTest, TriggerZeroQuerySuggestion) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  const struct TestCase {
    const char *history_key;
    const char *history_value;
    const char *find_value;
    int expected_rank;  // -1 when don't appear.
  } kTestCases[] = {
      {"@", "@", "gmail.com", 0},      {"@", "@", "docomo.ne.jp", 1},
      {"@", "@", "ezweb.ne.jp", 2},    {"@", "@", "i.softbank.jp", 3},
      {"@", "@", "softbank.ne.jp", 4}, {"!", "!", "?", -1},
  };

  for (const auto &test_case : kTestCases) {
    Segments segments;
    InitSegmentsWithKey("", &segments);

    PrependHistorySegments(test_case.history_key, test_case.history_value,
                           &segments);
    std::vector<Result> results;
    aggregator.AggregateZeroQuerySuffixPrediction(*suggestion_convreq_,
                                                  segments, &results);
    EXPECT_FALSE(results.empty());

    int rank = -1;
    for (size_t i = 0; i < results.size(); ++i) {
      const auto &result = results[i];
      EXPECT_EQ(result.types, SUFFIX);
      if (result.value == test_case.find_value && result.lid == 0 /* EOS */) {
        rank = static_cast<int>(i);
        break;
      }
    }
    EXPECT_EQ(rank, test_case.expected_rank) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictionAggregatorTest, ZipCodeRequest) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  struct TestCase {
    ConversionRequest request;
    const char *key;
    bool should_aggregate;
  } kTestCases[] = {
      {*suggestion_convreq_, "", false},  // No ZeroQuery entry
      {*suggestion_convreq_, "000", false},
      {*suggestion_convreq_, "---", false},
      {*suggestion_convreq_, "0124-", false},
      {*suggestion_convreq_, "012-0", false},
      {*suggestion_convreq_, "0124-0", true},    // key length >= 6
      {*suggestion_convreq_, "012-3456", true},  // key length >= 6
      {*suggestion_convreq_, "ABC", true},
      {*suggestion_convreq_, "０１２-０", true},

      {*prediction_convreq_, "", false},  // No ZeroQuery entry
      {*prediction_convreq_, "000", true},
      {*prediction_convreq_, "---", true},
      {*prediction_convreq_, "0124-", true},
      {*prediction_convreq_, "012-0", true},
      {*prediction_convreq_, "0124-0", true},
      {*prediction_convreq_, "012-3456", true},
      {*prediction_convreq_, "ABC", true},
      {*prediction_convreq_, "０１２-０", true},
  };

  for (const auto &test_case : kTestCases) {
    Segments segments;
    InitSegmentsWithKey(test_case.key, &segments);
    std::vector<Result> results;
    const bool has_result =
        (aggregator.AggregatePredictionForRequest(test_case.request, segments,
                                                  &results) != NO_PREDICTION);
    EXPECT_EQ(has_result, test_case.should_aggregate) << test_case.key;
  }
}

TEST_F(DictionaryPredictionAggregatorTest, MobileZipcodeEntries) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();
  MockDictionary *mock = data_and_aggregator->mutable_dictionary();
  EXPECT_CALL(*mock, LookupPredictive(StrEq("101-000"), _, _))
      .WillOnce(InvokeCallbackWithOneToken(
          "101-0001", "東京都千代田", 100 /* cost */,
          pos_matcher.GetZipcodeId(), pos_matcher.GetZipcodeId(), Token::NONE));
  EXPECT_CALL(*mock, LookupPredictive(StrEq("101-0001"), _, _))
      .WillOnce(InvokeCallbackWithOneToken(
          "101-0001", "東京都千代田", 100 /* cost */,
          pos_matcher.GetZipcodeId(), pos_matcher.GetZipcodeId(), Token::NONE));
  {
    Segments segments;
    SetUpInputForSuggestion("101-000", composer_.get(), &segments);
    std::vector<Result> results;
    aggregator.AggregatePredictionForRequest(*prediction_convreq_, segments,
                                             &results);
    EXPECT_FALSE(FindResultByValue(results, "東京都千代田"));
  }
  {
    // Aggregate zip code entries only for exact key match.
    Segments segments;
    SetUpInputForSuggestion("101-0001", composer_.get(), &segments);
    std::vector<Result> results;
    aggregator.AggregatePredictionForRequest(*prediction_convreq_, segments,
                                             &results);
    EXPECT_TRUE(FindResultByValue(results, "東京都千代田"));
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       RealtimeConversionStartingWithAlphabets) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  constexpr char kKey[] = "PCてすと";
  const char *kExpectedSuggestionValues[] = {
      "PCテスト",
      "PCてすと",
  };

  {
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    auto has_conversion_segment_key = [kKey](Segments *segments) {
      if (segments->conversion_segments_size() != 1) {
        return false;
      }
      return segments->conversion_segment(0).key() == kKey;
    };
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kKey);
    seg->add_candidate()->value = "PCテスト";
    seg->add_candidate()->value = "PCてすと";
    EXPECT_CALL(*immutable_converter,
                ConvertForRequest(_, Truly(has_conversion_segment_key)))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  InitSegmentsWithKey(kKey, &segments);

  std::vector<Result> results;

  suggestion_convreq_->set_use_actual_converter_for_realtime_conversion(false);
  aggregator.AggregateRealtimeConversion(*suggestion_convreq_, 10, segments,
                                         &results);
  ASSERT_EQ(2, results.size());

  EXPECT_EQ(results[0].types, REALTIME);
  EXPECT_EQ(results[0].value, kExpectedSuggestionValues[0]);
  EXPECT_EQ(results[1].value, kExpectedSuggestionValues[1]);
}

TEST_F(DictionaryPredictionAggregatorTest,
       RealtimeConversionWithSpellingCorrection) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  constexpr char kCapriHiragana[] = "かぷりちょうざ";

  {
    // No realtime conversion result
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillRepeatedly(Return(false));
  }
  std::vector<Result> results;
  SetUpInputForSuggestion(kCapriHiragana, composer_.get(), &segments);
  suggestion_convreq_->set_use_actual_converter_for_realtime_conversion(false);
  aggregator.AggregateUnigramCandidate(*suggestion_convreq_, segments,
                                       &results);
  ASSERT_FALSE(results.empty());
  EXPECT_TRUE(results[0].candidate_attributes &
              Segment::Candidate::SPELLING_CORRECTION);  // From unigram

  results.clear();

  constexpr char kKeyWithDe[] = "かぷりちょうざで";
  constexpr char kExpectedSuggestionValueWithDe[] = "カプリチョーザで";
  {
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    auto has_conversion_segment_key = [kKeyWithDe](Segments *segments) {
      if (segments->conversion_segments_size() != 1) {
        return false;
      }
      return segments->conversion_segment(0).key() == kKeyWithDe;
    };
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kKeyWithDe);
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = kExpectedSuggestionValueWithDe;
    candidate->attributes = Segment::Candidate::SPELLING_CORRECTION;
    EXPECT_CALL(*immutable_converter,
                ConvertForRequest(_, Truly(has_conversion_segment_key)))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  SetUpInputForSuggestion(kKeyWithDe, composer_.get(), &segments);
  aggregator.AggregateRealtimeConversion(*suggestion_convreq_, 1, segments,
                                         &results);
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].types, REALTIME);
  EXPECT_NE(0, (results[0].candidate_attributes &
                Segment::Candidate::SPELLING_CORRECTION));
  EXPECT_EQ(results[0].value, kExpectedSuggestionValueWithDe);
}

TEST_F(DictionaryPredictionAggregatorTest, PropagateUserDictionaryAttribute) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);

  {
    // No realtime conversion result
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillOnce(Return(false));

    Segments segments;
    SetUpInputForSuggestion("ゆーざー", composer_.get(), &segments);
    std::vector<Result> results;
    EXPECT_NE(NO_PREDICTION, aggregator.AggregatePredictionForRequest(
                                 *suggestion_convreq_, segments, &results));
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, "ユーザー");
    EXPECT_TRUE(results[0].candidate_attributes &
                Segment::Candidate::USER_DICTIONARY);
  }

  constexpr char kKey[] = "ゆーざーの";
  constexpr char kValue[] = "ユーザーの";
  {
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);
    auto has_conversion_segment_key = [kKey](Segments *segments) {
      if (segments->conversion_segments_size() != 1) {
        return false;
      }
      return segments->conversion_segment(0).key() == kKey;
    };
    Segments segments;
    Segment *seg = segments.add_segment();
    seg->set_key(kKey);
    Segment::Candidate *candidate = seg->add_candidate();
    candidate->value = kValue;
    candidate->attributes = Segment::Candidate::USER_DICTIONARY;
    EXPECT_CALL(*immutable_converter,
                ConvertForRequest(_, Truly(has_conversion_segment_key)))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  {
    Segments segments;
    SetUpInputForSuggestion(kKey, composer_.get(), &segments);
    std::vector<Result> results;
    EXPECT_NE(NO_PREDICTION, aggregator.AggregatePredictionForRequest(
                                 *suggestion_convreq_, segments, &results));
    EXPECT_FALSE(results.empty());
    EXPECT_EQ(results[0].value, kValue);
    EXPECT_TRUE(results[0].candidate_attributes &
                Segment::Candidate::USER_DICTIONARY);
  }
}

TEST_F(DictionaryPredictionAggregatorTest, EnrichPartialCandidates) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  Segments segments;
  SetUpInputForSuggestion("ぐーぐる", composer_.get(), &segments);

  std::vector<Result> results;
  EXPECT_TRUE(PREFIX & aggregator.AggregatePredictionForRequest(
                           *prediction_convreq_, segments, &results));
}

TEST_F(DictionaryPredictionAggregatorTest, CandidatesFromUserDictionary) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  const PosMatcher &pos_matcher = data_and_aggregator->pos_matcher();

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
    ::testing::Mock::VerifyAndClearExpectations(mock);
    const std::vector<Token> tokens = {
        // Suggest-only (only for exact key) USER DICTIONARY entry
        {"しょーとかっと", "ショートカット", 0, pos_matcher.GetUnknownId(),
         pos_matcher.GetUnknownId(), Token::USER_DICTIONARY},
        // Normal USER DICTIONARY entry
        {"しょーとかっと", "しょうとかっと", 0, pos_matcher.GetGeneralNounId(),
         pos_matcher.GetGeneralNounId(), Token::USER_DICTIONARY},
    };
    EXPECT_CALL(*mock, LookupPredictive(_, _, _))
        .WillRepeatedly(InvokeCallbackWithTokens(tokens));
    EXPECT_CALL(*mock, LookupPrefix(_, _, _)).Times(AnyNumber());
  }

  {
    Segments segments;
    SetUpInputForSuggestion("しょーとか", composer_.get(), &segments);

    std::vector<Result> results;
    EXPECT_TRUE(UNIGRAM & aggregator.AggregatePredictionForRequest(
                              *prediction_convreq_, segments, &results));
    EXPECT_TRUE(FindResultByValue(results, "しょうとかっと"));
    EXPECT_FALSE(FindResultByValue(results, "ショートカット"));
  }
  {
    Segments segments;
    SetUpInputForSuggestion("しょーとかっと", composer_.get(), &segments);

    std::vector<Result> results;
    EXPECT_TRUE(UNIGRAM & aggregator.AggregatePredictionForRequest(
                              *prediction_convreq_, segments, &results));
    EXPECT_TRUE(FindResultByValue(results, "しょうとかっと"));
    EXPECT_TRUE(FindResultByValue(results, "ショートカット"));
  }
}

namespace {
constexpr char kTestZeroQueryTokenArray[] =
    // The last two items must be 0x00, because they are now unused field.
    // {"あ", "❕", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x04\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"ああ", "( •̀ㅁ•́;)", ZERO_QUERY_EMOTICON, 0x00, 0x00}
    "\x05\x00\x00\x00"
    "\x01\x00\x00\x00"
    "\x02\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あい", "❕", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x06\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"あい", "❣", ZERO_QUERY_NONE, 0x00, 0x00}
    "\x06\x00\x00\x00"
    "\x03\x00\x00\x00"
    "\x00\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"猫", "😾", ZERO_QUERY_EMOJI, 0x00, 0x00}
    "\x07\x00\x00\x00"
    "\x08\x00\x00\x00"
    "\x03\x00"
    "\x00\x00"
    "\x00\x00\x00\x00";

const char *kTestZeroQueryStrings[] = {"",     "( •̀ㅁ•́;)", "❕", "❣", "あ",
                                       "ああ", "あい",     "猫", "😾"};
}  // namespace

TEST_F(DictionaryPredictionAggregatorTest, GetZeroQueryCandidates) {
  // Create test zero query data.
  std::unique_ptr<uint32_t[]> string_data_buffer;
  ZeroQueryDict zero_query_dict;
  {
    // kTestZeroQueryTokenArray contains a trailing '\0', so create a
    // absl::string_view that excludes it by subtracting 1.
    const absl::string_view token_array_data(
        kTestZeroQueryTokenArray, std::size(kTestZeroQueryTokenArray) - 1);
    std::vector<absl::string_view> strs;
    for (const char *str : kTestZeroQueryStrings) {
      strs.push_back(str);
    }
    const absl::string_view string_array_data =
        SerializedStringArray::SerializeToBuffer(strs, &string_data_buffer);
    zero_query_dict.Init(token_array_data, string_array_data);
  }

  struct TestCase {
    std::string key;
    bool expected_result;
    // candidate value and ZeroQueryType.
    std::vector<std::string> expected_candidates;
    std::vector<int32_t> expected_types;

    std::string DebugString() const {
      const std::string candidates = absl::StrJoin(expected_candidates, ", ");
      std::string types;
      for (size_t i = 0; i < expected_types.size(); ++i) {
        if (i != 0) {
          types.append(", ");
        }
        absl::StrAppendFormat(&types, "%d", types[i]);
      }
      return absl::StrFormat(
          "key: %s\n"
          "expected_result: %d\n"
          "expected_candidates: %s\n"
          "expected_types: %s",
          key, expected_result, candidates, types);
    }
  } kTestCases[] = {
      {"あい", true, {"❕", "❣"}, {ZERO_QUERY_EMOJI, ZERO_QUERY_NONE}},
      {"猫", true, {"😾"}, {ZERO_QUERY_EMOJI}},
      {"あ", false, {}, {}},  // Do not look up for one-char non-Kanji key
      {"あい", true, {"❕", "❣"}, {ZERO_QUERY_EMOJI, ZERO_QUERY_NONE}},
      {"あいう", false, {}, {}},
      {"", false, {}, {}},
      {"ああ", true, {"( •̀ㅁ•́;)"}, {ZERO_QUERY_EMOTICON}}};

  for (const auto &test_case : kTestCases) {
    ASSERT_EQ(test_case.expected_candidates.size(),
              test_case.expected_types.size());

    commands::Request client_request;
    composer::Table table;
    const config::Config &config = config::ConfigHandler::DefaultConfig();
    composer::Composer composer(&table, &client_request, &config);
    const ConversionRequest request(&composer, &client_request, &config);

    std::vector<ZeroQueryResult> actual_candidates;
    const bool actual_result =
        DictionaryPredictionAggregatorTestPeer::GetZeroQueryCandidatesForKey(
            request, test_case.key, zero_query_dict, &actual_candidates);
    EXPECT_EQ(actual_result, test_case.expected_result)
        << test_case.DebugString();
    for (size_t i = 0; i < test_case.expected_candidates.size(); ++i) {
      EXPECT_EQ(actual_candidates[i].first, test_case.expected_candidates[i])
          << "Failed at " << i << " : " << test_case.DebugString();
      EXPECT_EQ(actual_candidates[i].second, test_case.expected_types[i])
          << "Failed at " << i << " : " << test_case.DebugString();
    }
  }
}

// b/235917071
TEST_F(DictionaryPredictionAggregatorTest, DoNotModifyHistorySegment) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();

  {
    // Set up mock immutable converter.
    MockImmutableConverter *immutable_converter =
        data_and_aggregator->mutable_immutable_converter();
    ::testing::Mock::VerifyAndClearExpectations(immutable_converter);

    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_segment_type(Segment::HISTORY);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->key = "key_can_be_modified";
    candidate->value = "history_value";

    segment = segments.add_segment();
    candidate = segment->add_candidate();
    candidate->value = "conversion_result";

    EXPECT_CALL(*immutable_converter, ConvertForRequest(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(segments), Return(true)));
  }

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  request_->set_mixed_conversion(true);

  Segments segments;
  SetUpInputForSuggestionWithHistory("てすと", "103", "103", composer_.get(),
                                     &segments);
  prediction_convreq_->set_use_actual_converter_for_realtime_conversion(false);

  std::vector<Result> results;
  EXPECT_TRUE(aggregator.AggregatePredictionForRequest(*prediction_convreq_,
                                                       segments, &results));
  EXPECT_EQ(results.size(), 1);
  EXPECT_EQ(results[0].value, "conversion_result");
  EXPECT_EQ(segments.history_segment(0).candidate(0).value, "103");
}

TEST_F(DictionaryPredictionAggregatorTest, NumberDecoderCandidates) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  Segments segments;
  SetUpInputForSuggestion("よんじゅうごかい", composer_.get(), &segments);

  std::vector<Result> results;
  EXPECT_NE(NO_PREDICTION, aggregator.AggregatePredictionForRequest(
                               *prediction_convreq_, segments, &results));
  const auto &result =
      std::find_if(results.begin(), results.end(),
                   [](Result r) { return r.value == "45" && !r.removed; });
  ASSERT_NE(result, results.end());
  EXPECT_TRUE(result->candidate_attributes &
              Segment::Candidate::PARTIALLY_KEY_CONSUMED);
  EXPECT_TRUE(result->candidate_attributes &
              Segment::Candidate::NO_SUGGEST_LEARNING);
}

TEST_F(DictionaryPredictionAggregatorTest, DoNotPredictNoisyNumberEntries) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    MockDictionary *mock = data_and_aggregator->mutable_dictionary();
    EXPECT_CALL(*mock, LookupPredictive(StrEq("1"), _, _))
        .WillRepeatedly(InvokeCallbackWithKeyValues({{"1", "一"},
                                                     {"1じ", "一時"},
                                                     {"1じ", "1時"},
                                                     {"10じ", "10時"},
                                                     {"10じ", "十時"},
                                                     {"1じすぎ", "1時過ぎ"},
                                                     {"19じ", "19時"}}));
  }

  composer_->SetInputMode(transliteration::HALF_ASCII);
  Segments segments;
  SetUpInputForSuggestion("1", composer_.get(), &segments);

  std::vector<Result> results;
  EXPECT_NE(NO_PREDICTION, aggregator.AggregatePredictionForRequest(
                               *prediction_convreq_, segments, &results));
  EXPECT_FALSE(FindResultByValue(results, "10時"));
  EXPECT_FALSE(FindResultByValue(results, "十時"));
  EXPECT_FALSE(FindResultByValue(results, "1時過ぎ"));
  EXPECT_FALSE(FindResultByValue(results, "19時"));

  EXPECT_TRUE(FindResultByValue(results, "一"));
  EXPECT_TRUE(FindResultByValue(results, "一時"));
  EXPECT_TRUE(FindResultByValue(results, "1時"));
}

TEST_F(DictionaryPredictionAggregatorTest, SingleKanji) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_enable_single_kanji_prediction(true);

  {
    auto create_single_kanji_result = [](absl::string_view key,
                                         absl::string_view value) {
      Result result;
      result.key = std::string(key);
      result.value = std::string(value);
      result.SetTypesAndTokenAttributes(SINGLE_KANJI, Token::NONE);
      return result;
    };
    MockSingleKanjiPredictionAggregator *mock =
        data_and_aggregator->mutable_single_kanji_prediction_aggregator();
    EXPECT_CALL(*mock, AggregateResults(_, _))
        .WillOnce(Return(
            std::vector<Result>{create_single_kanji_result("て", "手")}));
  }

  Segments segments;
  SetUpInputForSuggestion("てすと", composer_.get(), &segments);

  std::vector<Result> results;
  EXPECT_TRUE(aggregator.AggregatePredictionForRequest(*prediction_convreq_,
                                                       segments, &results) &
              SINGLE_KANJI);
  EXPECT_FALSE(results.empty());
  for (const auto &result : results) {
    if (!(result.types & SINGLE_KANJI)) {
      EXPECT_GT(Util::CharsLen(result.value), 1);
    }
  }
}

TEST_F(DictionaryPredictionAggregatorTest,
       SingleKanjiForMobileHardwareKeyboard) {
  std::unique_ptr<MockDataAndAggregator> data_and_aggregator =
      CreateAggregatorWithMockData();
  const DictionaryPredictionAggregatorTestPeer &aggregator =
      data_and_aggregator->aggregator();
  commands::RequestForUnitTest::FillMobileRequestWithHardwareKeyboard(
      request_.get());
  request_->mutable_decoder_experiment_params()
      ->set_enable_single_kanji_prediction(true);

  {
    MockSingleKanjiPredictionAggregator *mock =
        data_and_aggregator->mutable_single_kanji_prediction_aggregator();
    EXPECT_CALL(*mock, AggregateResults(_, _)).Times(0);
  }

  Segments segments;
  SetUpInputForSuggestion("てすと", composer_.get(), &segments);

  std::vector<Result> results;
  EXPECT_FALSE(aggregator.AggregatePredictionForRequest(*prediction_convreq_,
                                                        segments, &results) &
               SINGLE_KANJI);
}

}  // namespace
}  // namespace prediction
}  // namespace mozc
