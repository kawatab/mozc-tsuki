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

#include "prediction/dictionary_predictor.h"

#include <algorithm>
#include <memory>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/flags.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/serialized_string_array.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/internal/typing_model.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/connector.h"
#include "converter/converter_interface.h"
#include "converter/converter_mock.h"
#include "converter/immutable_converter.h"
#include "converter/immutable_converter_interface.h"
#include "converter/node_allocator.h"
#include "converter/segmenter.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_mock.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suffix_dictionary.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/system/system_dictionary.h"
#include "prediction/suggestion_filter.h"
#include "prediction/zero_query_dict.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "session/request_test_util.h"
#include "testing/base/public/gmock.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "transliteration/transliteration.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

DECLARE_bool(enable_expansion_for_dictionary_predictor);

namespace mozc {
namespace {

using std::unique_ptr;

using dictionary::DictionaryInterface;
using dictionary::DictionaryMock;
using dictionary::POSMatcher;
using dictionary::PosGroup;
using dictionary::SuffixDictionary;
using dictionary::SuppressionDictionary;
using dictionary::Token;
using ::testing::_;

const int kInfinity = (2 << 20);

DictionaryInterface *CreateSystemDictionaryFromDataManager(
    const DataManagerInterface &data_manager) {
  const char *data = NULL;
  int size = 0;
  data_manager.GetSystemDictionaryData(&data, &size);
  using mozc::dictionary::SystemDictionary;
  return SystemDictionary::Builder(data, size).Build();
}

DictionaryInterface *CreateSuffixDictionaryFromDataManager(
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

SuggestionFilter *CreateSuggestionFilter(
    const DataManagerInterface &data_manager) {
  const char *data = NULL;
  size_t size = 0;
  data_manager.GetSuggestionFilterData(&data, &size);
  return new SuggestionFilter(data, size);
}

// Simple immutable converter mock for the realtime conversion test
class ImmutableConverterMock : public ImmutableConverterInterface {
 public:
  ImmutableConverterMock() {
    Segment *segment = segments_.add_segment();
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
  }

  bool ConvertForRequest(
      const ConversionRequest &request, Segments *segments) const override {
    segments->CopyFrom(segments_);
    return true;
  }

 private:
  Segments segments_;
};

class TestableDictionaryPredictor : public DictionaryPredictor {
  // Test-only subclass: Just changing access levels
 public:
  TestableDictionaryPredictor(
      const DataManagerInterface &data_manager,
      const ConverterInterface *converter,
      const ImmutableConverterInterface *immutable_converter,
      const DictionaryInterface *dictionary,
      const DictionaryInterface *suffix_dictionary,
      const Connector *connector,
      const Segmenter *segmenter,
      const POSMatcher *pos_matcher,
      const SuggestionFilter *suggestion_filter)
      : DictionaryPredictor(data_manager,
                            converter,
                            immutable_converter,
                            dictionary,
                            suffix_dictionary,
                            connector,
                            segmenter,
                            pos_matcher,
                            suggestion_filter) {}

  using DictionaryPredictor::PredictionTypes;
  using DictionaryPredictor::NO_PREDICTION;
  using DictionaryPredictor::UNIGRAM;
  using DictionaryPredictor::BIGRAM;
  using DictionaryPredictor::REALTIME;
  using DictionaryPredictor::REALTIME_TOP;
  using DictionaryPredictor::SUFFIX;
  using DictionaryPredictor::ENGLISH;
  using DictionaryPredictor::Result;
  using DictionaryPredictor::MakeEmptyResult;
  using DictionaryPredictor::AddPredictionToCandidates;
  using DictionaryPredictor::AggregateRealtimeConversion;
  using DictionaryPredictor::AggregateUnigramPrediction;
  using DictionaryPredictor::AggregateBigramPrediction;
  using DictionaryPredictor::AggregateSuffixPrediction;
  using DictionaryPredictor::AggregateEnglishPrediction;
  using DictionaryPredictor::ApplyPenaltyForKeyExpansion;
  using DictionaryPredictor::TYPING_CORRECTION;
  using DictionaryPredictor::AggregateTypeCorrectingPrediction;
};

// Helper class to hold dictionary data and predictor objects.
class MockDataAndPredictor {
 public:
  // Initializes predictor with given dictionary and suffix_dictionary.  When
  // NULL is passed to the first argument |dictionary|, the default
  // DictionaryMock is used. For the second, the default is MockDataManager's
  // suffix dictionary. Note that |dictionary| is owned by this class but
  // |suffix_dictionary| is NOT owned because the current design assumes that
  // suffix dictionary is singleton.
  void Init(const DictionaryInterface *dictionary = NULL,
            const DictionaryInterface *suffix_dictionary = NULL) {
    pos_matcher_.Set(data_manager_.GetPOSMatcherData());
    suppression_dictionary_.reset(new SuppressionDictionary);
    if (!dictionary) {
      dictionary_mock_ = new DictionaryMock;
      dictionary_.reset(dictionary_mock_);
    } else {
      dictionary_mock_ = NULL;
      dictionary_.reset(dictionary);
    }
    if (!suffix_dictionary) {
      suffix_dictionary_.reset(
          CreateSuffixDictionaryFromDataManager(data_manager_));
    } else {
      suffix_dictionary_.reset(suffix_dictionary);
    }
    CHECK(suffix_dictionary_.get());

    connector_.reset(Connector::CreateFromDataManager(data_manager_));
    CHECK(connector_.get());

    segmenter_.reset(Segmenter::CreateFromDataManager(data_manager_));
    CHECK(segmenter_.get());

    pos_group_.reset(new PosGroup(data_manager_.GetPosGroupData()));
    suggestion_filter_.reset(CreateSuggestionFilter(data_manager_));
    immutable_converter_.reset(
        new ImmutableConverterImpl(dictionary_.get(),
                                   suffix_dictionary_.get(),
                                   suppression_dictionary_.get(),
                                   connector_.get(),
                                   segmenter_.get(),
                                   &pos_matcher_,
                                   pos_group_.get(),
                                   suggestion_filter_.get()));
    converter_.reset(new ConverterMock());
    dictionary_predictor_.reset(
        new TestableDictionaryPredictor(data_manager_,
                                        converter_.get(),
                                        immutable_converter_.get(),
                                        dictionary_.get(),
                                        suffix_dictionary_.get(),
                                        connector_.get(),
                                        segmenter_.get(),
                                        &pos_matcher_,
                                        suggestion_filter_.get()));
  }

  const POSMatcher &pos_matcher() const {
    return pos_matcher_;
  }

  DictionaryMock *mutable_dictionary() {
    return dictionary_mock_;
  }

  ConverterMock *mutable_converter_mock() {
    return converter_.get();
  }

  const TestableDictionaryPredictor *dictionary_predictor() {
    return dictionary_predictor_.get();
  }

  TestableDictionaryPredictor *mutable_dictionary_predictor() {
    return dictionary_predictor_.get();
  }

 private:
  const testing::MockDataManager data_manager_;
  POSMatcher pos_matcher_;
  unique_ptr<SuppressionDictionary> suppression_dictionary_;
  unique_ptr<const Connector> connector_;
  unique_ptr<const Segmenter> segmenter_;
  unique_ptr<const DictionaryInterface> suffix_dictionary_;
  unique_ptr<const DictionaryInterface> dictionary_;
  DictionaryMock *dictionary_mock_;
  unique_ptr<const PosGroup> pos_group_;
  unique_ptr<ImmutableConverterInterface> immutable_converter_;
  unique_ptr<ConverterMock> converter_;
  unique_ptr<const SuggestionFilter> suggestion_filter_;
  unique_ptr<TestableDictionaryPredictor> dictionary_predictor_;
};

class CallCheckDictionary : public DictionaryInterface {
 public:
  CallCheckDictionary() = default;
  ~CallCheckDictionary() override = default;

  MOCK_CONST_METHOD1(HasKey,
                     bool(StringPiece));
  MOCK_CONST_METHOD1(HasValue,
                     bool(StringPiece));
  MOCK_CONST_METHOD3(LookupPredictive,
                     void(StringPiece key,
                          const ConversionRequest& convreq,
                          Callback *callback));
  MOCK_CONST_METHOD3(LookupPrefix,
                     void(StringPiece key,
                          const ConversionRequest& convreq,
                          Callback *callback));
  MOCK_CONST_METHOD3(LookupExact,
                     void(StringPiece key,
                          const ConversionRequest& convreq,
                          Callback *callback));
  MOCK_CONST_METHOD3(LookupReverse,
                     void(StringPiece str,
                          const ConversionRequest& convreq,
                          Callback *callback));
};

// Action to call the third argument of LookupPrefix with the token
// <key, value>.
ACTION_P4(LookupPrefixOneToken, key, value, lid, rid) {
  Token token;
  token.key = key;
  token.value = value;
  token.lid = lid;
  token.rid = rid;
  arg2->OnToken(key, key, token);
}

void MakeSegmentsForSuggestion(const string key, Segments *segments) {
  segments->Clear();
  segments->set_max_prediction_candidates_size(10);
  segments->set_request_type(Segments::SUGGESTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void MakeSegmentsForPrediction(const string key, Segments *segments) {
  segments->Clear();
  segments->set_max_prediction_candidates_size(50);
  segments->set_request_type(Segments::PREDICTION);
  Segment *seg = segments->add_segment();
  seg->set_key(key);
  seg->set_segment_type(Segment::FREE);
}

void PrependHistorySegments(const string &key,
                            const string &value,
                            Segments *segments) {
  Segment *seg = segments->push_front_segment();
  seg->set_segment_type(Segment::HISTORY);
  seg->set_key(key);
  Segment::Candidate *c = seg->add_candidate();
  c->key = key;
  c->content_key = key;
  c->value = value;
  c->content_value = value;
}

class MockTypingModel : public mozc::composer::TypingModel {
 public:
  MockTypingModel() : TypingModel(nullptr, 0, nullptr, 0, nullptr) {}
  ~MockTypingModel() override = default;
  int GetCost(StringPiece key) const override {
    return 10;
  }
};

}  // namespace

class DictionaryPredictorTest : public ::testing::Test {
 public:
  DictionaryPredictorTest() :
      default_expansion_flag_(
          FLAGS_enable_expansion_for_dictionary_predictor) {
  }

  ~DictionaryPredictorTest() override {
    FLAGS_enable_expansion_for_dictionary_predictor = default_expansion_flag_;
  }

 protected:
  void SetUp() override {
    FLAGS_enable_expansion_for_dictionary_predictor = false;
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    request_.reset(new commands::Request);
    config_.reset(new config::Config);
    config::ConfigHandler::GetDefaultConfig(config_.get());
    table_.reset(new composer::Table);
    composer_.reset(
        new composer::Composer(table_.get(), request_.get(), config_.get()));
    convreq_.reset(
        new ConversionRequest(composer_.get(), request_.get(), config_.get()));

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  void TearDown() override {
    FLAGS_enable_expansion_for_dictionary_predictor = false;
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  static void AddWordsToMockDic(DictionaryMock *mock) {
    const char kGoogleA[] = "ぐーぐるあ";

    const char kGoogleAdsenseHiragana[] = "ぐーぐるあどせんす";
    const char kGoogleAdsenseKatakana[] = "グーグルアドセンス";
    mock->AddLookupPredictive(kGoogleA, kGoogleAdsenseHiragana,
                              kGoogleAdsenseKatakana, Token::NONE);

    const char kGoogleAdwordsHiragana[] = "ぐーぐるあどわーず";
    const char kGoogleAdwordsKatakana[] = "グーグルアドワーズ";
    mock->AddLookupPredictive(kGoogleA, kGoogleAdwordsHiragana,
                              kGoogleAdwordsKatakana, Token::NONE);

    const char kGoogle[] = "ぐーぐる";
    mock->AddLookupPredictive(kGoogle, kGoogleAdsenseHiragana,
                              kGoogleAdsenseKatakana, Token::NONE);
    mock->AddLookupPredictive(kGoogle, kGoogleAdwordsHiragana,
                              kGoogleAdwordsKatakana, Token::NONE);

    const char kGoogleKatakana[] = "グーグル";
    mock->AddLookupPrefix(kGoogle, kGoogleKatakana, kGoogleKatakana,
                          Token::NONE);

    const char kAdsense[] = "あどせんす";
    const char kAdsenseKatakana[] = "アドセンス";
    mock->AddLookupPrefix(kAdsense, kAdsenseKatakana, kAdsenseKatakana,
                          Token::NONE);

    const char kTestHiragana[] = "てすと";
    const char kTestKatakana[] = "テスト";
    mock->AddLookupPrefix(kTestHiragana, kTestHiragana, kTestKatakana,
                          Token::NONE);

    const char kFilterHiragana[] = "ふぃるたーたいしょう";
    const char kFilterPrefixHiragana[] = "ふぃるたーたいし";

    // Note: This is in the filter
    const char kFilterWord[] = "フィルター対象";

    // Note: This is NOT in the filter
    const char kNonFilterWord[] = "フィルター大将";

    mock->AddLookupPrefix(kFilterHiragana, kFilterHiragana, kFilterWord,
                          Token::NONE);

    mock->AddLookupPrefix(kFilterHiragana, kFilterHiragana, kNonFilterWord,
                          Token::NONE);

    mock->AddLookupPredictive(kFilterHiragana, kFilterHiragana, kFilterWord,
                              Token::NONE);

    mock->AddLookupPredictive(kFilterHiragana, kFilterPrefixHiragana,
                              kFilterWord, Token::NONE);

    const char kWrongCapriHiragana[] = "かぷりちょうざ";
    const char kRightCapriHiragana[] = "かぷりちょーざ";
    const char kCapriKatakana[] = "カプリチョーザ";

    mock->AddLookupPrefix(kWrongCapriHiragana, kRightCapriHiragana,
                          kCapriKatakana, Token::SPELLING_CORRECTION);

    mock->AddLookupPredictive(kWrongCapriHiragana, kRightCapriHiragana,
                              kCapriKatakana, Token::SPELLING_CORRECTION);

    const char kDe[] = "で";

    mock->AddLookupPrefix(kDe, kDe, kDe, Token::NONE);

    const char kHirosueHiragana[] = "ひろすえ";
    const char kHirosue[] = "広末";

    mock->AddLookupPrefix(kHirosueHiragana, kHirosueHiragana, kHirosue,
                          Token::NONE);

    const char kYuzaHiragana[] = "ゆーざー";
    const char kYuza[] = "ユーザー";
    // For dictionary suggestion
    mock->AddLookupPredictive(kYuzaHiragana, kYuzaHiragana, kYuza,
                              Token::USER_DICTIONARY);
    // For realtime conversion
    mock->AddLookupPrefix(kYuzaHiragana, kYuzaHiragana, kYuza,
                          Token::USER_DICTIONARY);

    // Some English entries
    mock->AddLookupPredictive("conv", "converge", "converge", Token::NONE);
    mock->AddLookupPredictive("conv", "converged", "converged", Token::NONE);
    mock->AddLookupPredictive("conv", "convergent", "convergent", Token::NONE);
    mock->AddLookupPredictive("con", "contraction", "contraction", Token::NONE);
    mock->AddLookupPredictive("con", "control", "control", Token::NONE);
  }

  MockDataAndPredictor *CreateDictionaryPredictorWithMockData() {
    MockDataAndPredictor *ret = new MockDataAndPredictor;
    ret->Init();
    AddWordsToMockDic(ret->mutable_dictionary());
    return ret;
  }

  void GenerateKeyEvents(const string &text,
                         std::vector<commands::KeyEvent> *keys) {
    keys->clear();

    const char *begin = text.data();
    const char *end = text.data() + text.size();
    size_t mblen = 0;

    while (begin < end) {
      commands::KeyEvent key;
      const char32 w = Util::UTF8ToUCS4(begin, end, &mblen);
      if (Util::GetCharacterSet(w) == Util::ASCII) {
        key.set_key_code(*begin);
      } else {
        key.set_key_code('?');
        key.set_key_string(string(begin, mblen));
      }
      begin += mblen;
      keys->push_back(key);
    }
  }

  void InsertInputSequence(const string &text, composer::Composer *composer) {
    std::vector<commands::KeyEvent> keys;
    GenerateKeyEvents(text, &keys);

    for (size_t i = 0; i < keys.size(); ++i) {
      composer->InsertCharacterKeyEvent(keys[i]);
    }
  }

  void InsertInputSequenceForProbableKeyEvent(const string &text,
                                              const uint32 *corrected_key_codes,
                                              composer::Composer *composer) {
    std::vector<commands::KeyEvent> keys;
    GenerateKeyEvents(text, &keys);

    for (size_t i = 0; i < keys.size(); ++i) {
      if (keys[i].key_code() != corrected_key_codes[i]) {
        commands::KeyEvent::ProbableKeyEvent *probable_key_event;

        probable_key_event = keys[i].add_probable_key_event();
        probable_key_event->set_key_code(keys[i].key_code());
        probable_key_event->set_probability(0.9f);

        probable_key_event = keys[i].add_probable_key_event();
        probable_key_event->set_key_code(corrected_key_codes[i]);
        probable_key_event->set_probability(0.1f);
      }
      composer->InsertCharacterKeyEvent(keys[i]);
    }
  }

  void ExpansionForUnigramTestHelper(bool use_expansion) {
    config_->set_use_dictionary_suggest(true);
    config_->set_use_realtime_conversion(false);
    config_->set_use_kana_modifier_insensitive_conversion(use_expansion);

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->SetTable(table_.get());
    unique_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor;
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(check_dictionary, NULL);
    const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      request_->set_kana_modifier_insensitive_conversion(use_expansion);
      InsertInputSequence("gu-g", composer_.get());
      Segment *segment = segments.add_segment();
      CHECK(segment);
      string query;
      composer_->GetQueryForPrediction(&query);
      segment->set_key(query);

      EXPECT_CALL(*check_dictionary,
                  LookupPredictive(::testing::Ne(""),
                                   ::testing::Ref(*convreq_), _))
          .Times(::testing::AtLeast(1));

      std::vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateUnigramPrediction(
          TestableDictionaryPredictor::UNIGRAM,
          *convreq_, segments, &results);
    }
  }

  void ExpansionForBigramTestHelper(bool use_expansion) {
    config_->set_use_dictionary_suggest(true);
    config_->set_use_realtime_conversion(false);
    config_->set_use_kana_modifier_insensitive_conversion(use_expansion);

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->SetTable(table_.get());
    unique_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor;
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(check_dictionary, NULL);
    const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      // History segment's key and value should be in the dictionary
      Segment *segment = segments.add_segment();
      CHECK(segment);
      segment->set_segment_type(Segment::HISTORY);
      segment->set_key("ぐーぐる");
      Segment::Candidate *cand = segment->add_candidate();
      cand->key = "ぐーぐる";
      cand->content_key = "ぐーぐる";
      cand->value = "グーグル";
      cand->content_value = "グーグル";

      segment = segments.add_segment();
      CHECK(segment);

      request_->set_kana_modifier_insensitive_conversion(use_expansion);
      InsertInputSequence("m", composer_.get());
      string query;
      composer_->GetQueryForPrediction(&query);
      segment->set_key(query);

      // History key and value should be in the dictionary.
      EXPECT_CALL(*check_dictionary,
                  LookupPrefix(_, ::testing::Ref(*convreq_), _))
          .WillOnce(LookupPrefixOneToken("ぐーぐる", "グーグル", 1, 1));
      EXPECT_CALL(*check_dictionary,
                  LookupPredictive(_, ::testing::Ref(*convreq_), _));

      std::vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateBigramPrediction(TestableDictionaryPredictor::BIGRAM,
                                           *convreq_, segments, &results);
    }
  }

  void ExpansionForSuffixTestHelper(bool use_expansion) {
    config_->set_use_dictionary_suggest(true);
    config_->set_use_realtime_conversion(false);
    config_->set_use_kana_modifier_insensitive_conversion(use_expansion);

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->SetTable(table_.get());
    unique_ptr<MockDataAndPredictor> data_and_predictor(
        new MockDataAndPredictor);
    // CallCheckDictionary is managed by data_and_predictor.
    CallCheckDictionary *check_dictionary = new CallCheckDictionary;
    data_and_predictor->Init(NULL, check_dictionary);
    const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

    {
      Segments segments;
      segments.set_request_type(Segments::PREDICTION);
      Segment *segment = segments.add_segment();
      CHECK(segment);

      request_->set_kana_modifier_insensitive_conversion(use_expansion);
      InsertInputSequence("des", composer_.get());
      string query;
      composer_->GetQueryForPrediction(&query);
      segment->set_key(query);

      EXPECT_CALL(*check_dictionary,
                  LookupPredictive(::testing::Ne(""),
                                   ::testing::Ref(*convreq_), _))
          .Times(::testing::AtLeast(1));

      std::vector<TestableDictionaryPredictor::Result> results;
      predictor->AggregateSuffixPrediction(
          TestableDictionaryPredictor::SUFFIX,
          *convreq_, segments, &results);
    }
  }

  bool FindCandidateByValue(
      const Segment &segment,
      const string &value) {
    for (size_t i = 0; i < segment.candidates_size(); ++i) {
      const Segment::Candidate &c = segment.candidate(i);
      if (c.value == value) {
        return true;
      }
    }
    return false;
  }

  bool FindResultByValue(
      const std::vector<TestableDictionaryPredictor::Result> &results,
      const string &value) {
    for (size_t i = 0; i < results.size(); ++i) {
      if (results[i].value == value) {
        return true;
      }
    }
    return false;
  }

  void AggregateEnglishPredictionTestHelper(
      transliteration::TransliterationType input_mode,
      const char *key, const char *expected_prefix,
      const char *expected_values[], size_t expected_values_size) {
    unique_ptr<MockDataAndPredictor> data_and_predictor(
        CreateDictionaryPredictorWithMockData());
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    table_->LoadFromFile("system://romanji-hiragana.tsv");
    composer_->Reset();
    composer_->SetTable(table_.get());
    composer_->SetInputMode(input_mode);
    InsertInputSequence(key, composer_.get());

    Segments segments;
    MakeSegmentsForPrediction(key, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    predictor->AggregateEnglishPrediction(
        TestableDictionaryPredictor::ENGLISH,
        *convreq_, segments, &results);

    std::set<string> values;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::ENGLISH, results[i].types);
      EXPECT_TRUE(Util::StartsWith(results[i].value, expected_prefix))
          << results[i].value
          << " doesn't start with " << expected_prefix;
      values.insert(results[i].value);
    }
    for (size_t i = 0; i < expected_values_size; ++i) {
      EXPECT_TRUE(values.find(expected_values[i]) != values.end())
          << expected_values[i] << " isn't in the results";
    }
  }

  void AggregateTypeCorrectingTestHelper(
      const char *key,
      const uint32 *corrected_key_codes,
      const char *expected_values[],
      size_t expected_values_size) {
    request_->set_special_romanji_table(
        commands::Request::QWERTY_MOBILE_TO_HIRAGANA);

    unique_ptr<MockDataAndPredictor> data_and_predictor(
        CreateDictionaryPredictorWithMockData());
    const TestableDictionaryPredictor *predictor =
        data_and_predictor->dictionary_predictor();

    table_->LoadFromFile("system://qwerty_mobile-hiragana.tsv");
    table_->typing_model_.reset(new MockTypingModel());
    InsertInputSequenceForProbableKeyEvent(
        key, corrected_key_codes, composer_.get());

    Segments segments;
    MakeSegmentsForPrediction(key, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    predictor->AggregateTypeCorrectingPrediction(
        TestableDictionaryPredictor::TYPING_CORRECTION,
        *convreq_, segments, &results);

    std::set<string> values;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::TYPING_CORRECTION,
                results[i].types);
      values.insert(results[i].value);
    }
    for (size_t i = 0; i < expected_values_size; ++i) {
      EXPECT_TRUE(values.find(expected_values[i]) != values.end())
          << expected_values[i] << " isn't in the results";
    }
  }

  unique_ptr<composer::Composer> composer_;
  unique_ptr<composer::Table> table_;
  unique_ptr<ConversionRequest> convreq_;
  unique_ptr<config::Config> config_;
  unique_ptr<commands::Request> request_;

 private:
  const bool default_expansion_flag_;
  unique_ptr<ImmutableConverterInterface> immutable_converter_;
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(DictionaryPredictorTest, OnOffTest) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // turn off
  Segments segments;
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(false);

  MakeSegmentsForSuggestion("ぐーぐるあ", &segments);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));

  // turn on
  config_->set_use_dictionary_suggest(true);
  MakeSegmentsForSuggestion("ぐーぐるあ", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));

  // empty query
  MakeSegmentsForSuggestion("", &segments);
  EXPECT_FALSE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(DictionaryPredictorTest, PartialSuggestion) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  {
    // Set up mock converter.
    Segments segments;
    Segment *segment = segments.add_segment();
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "Realtime top result";
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);
  // turn on mobile mode
  request_->set_mixed_conversion(true);

  segments.Clear();
  segments.set_max_prediction_candidates_size(10);
  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  Segment *seg = segments.add_segment();
  seg->set_key("ぐーぐるあ");
  seg->set_segment_type(Segment::FREE);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(DictionaryPredictorTest, BigramTest) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);

  MakeSegmentsForSuggestion("あ", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  // "グーグルアドセンス" will be returned.
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

TEST_F(DictionaryPredictorTest, BigramTestWithZeroQuery) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);
  request_->set_zero_query_suggestion(true);

  // current query is empty
  MakeSegmentsForSuggestion("", &segments);

  // history is "グーグル"
  PrependHistorySegments("ぐーぐる", "グーグル", &segments);

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
}

// Check that previous candidate never be shown at the current candidate.
TEST_F(DictionaryPredictorTest, Regression3042706) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);

  MakeSegmentsForSuggestion("だい", &segments);

  // history is "きょうと/京都"
  PrependHistorySegments("きょうと", "京都", &segments);

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_EQ(2, segments.segments_size());  // history + current
  for (int i = 0; i < segments.segment(1).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(1).candidate(i);
    EXPECT_FALSE(Util::StartsWith(candidate.content_value, "京都"));
    EXPECT_TRUE(Util::StartsWith(candidate.content_key, "だい"));
  }
}

TEST_F(DictionaryPredictorTest, GetPredictionTypes) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);

  // empty segments
  {
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // normal segments
  {
    MakeSegmentsForSuggestion("てすとだよ", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    segments.set_request_type(Segments::CONVERSION);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // short key
  {
    MakeSegmentsForSuggestion("てす", &segments);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    // on prediction mode, return UNIGRAM
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // zipcode-like key
  {
    MakeSegmentsForSuggestion("0123", &segments);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // History is short => UNIGRAM
  {
    MakeSegmentsForSuggestion("てすとだよ", &segments);
    PrependHistorySegments("A", "A", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // both History and current segment are long => UNIGRAM|BIGRAM
  {
    MakeSegmentsForSuggestion("てすとだよ", &segments);
    PrependHistorySegments("てすとだよ", "abc", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // Current segment is short => BIGRAM
  {
    MakeSegmentsForSuggestion("A", &segments);
    PrependHistorySegments("てすとだよ", "abc", &segments);
    EXPECT_EQ(DictionaryPredictor::BIGRAM,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // Typing correction type shouldn't be appended.
  {
    MakeSegmentsForSuggestion("ｐはよう", &segments);
    EXPECT_FALSE(DictionaryPredictor::TYPING_CORRECTION &
                 DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // Input mode is HALF_ASCII or FULL_ASCII => ENGLISH
  {
    config_->set_use_dictionary_suggest(true);

    MakeSegmentsForSuggestion("hel", &segments);

    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(DictionaryPredictor::ENGLISH,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    composer_->SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(DictionaryPredictor::ENGLISH,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    // When dictionary suggest is turned off, English prediction should be
    // disabled.
    config_->set_use_dictionary_suggest(false);

    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    composer_->SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    config_->set_use_dictionary_suggest(true);

    segments.set_request_type(Segments::PARTIAL_SUGGESTION);
    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(DictionaryPredictor::ENGLISH | DictionaryPredictor::REALTIME,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    composer_->SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(DictionaryPredictor::ENGLISH | DictionaryPredictor::REALTIME,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    config_->set_use_dictionary_suggest(false);

    composer_->SetInputMode(transliteration::HALF_ASCII);
    EXPECT_EQ(DictionaryPredictor::REALTIME,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));

    composer_->SetInputMode(transliteration::FULL_ASCII);
    EXPECT_EQ(DictionaryPredictor::REALTIME,
              DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
  }

  // When romaji table is qwerty mobile => ENGLISH is included depending on the
  // language aware input setting.
  {
    const auto orig_input_mode = composer_->GetInputMode();
    const auto orig_table = request_->special_romanji_table();
    const auto orig_lang_aware = request_->language_aware_input();
    const bool orig_use_dictionary_suggest = config_->use_dictionary_suggest();

    composer_->SetInputMode(transliteration::HIRAGANA);
    config_->set_use_dictionary_suggest(true);

    // The case where romaji table is set to qwerty.  ENGLISH is turned on if
    // language aware input is enabled.
    for (const auto table :
         {commands::Request::QWERTY_MOBILE_TO_HIRAGANA,
          commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII}) {
      request_->set_special_romanji_table(table);

      // Language aware input is default: No English prediction.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = DictionaryPredictor::GetPredictionTypes(*convreq_, segments);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is off: No English prediction.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = DictionaryPredictor::GetPredictionTypes(*convreq_, segments);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is on: English prediction is included.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = DictionaryPredictor::GetPredictionTypes(*convreq_, segments);
      EXPECT_EQ(DictionaryPredictor::ENGLISH,
                type & DictionaryPredictor::ENGLISH);
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
      request_->set_special_romanji_table(table);

      // Language aware input is default.
      request_->set_language_aware_input(
          commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR);
      auto type = DictionaryPredictor::GetPredictionTypes(*convreq_, segments);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is off.
      request_->set_language_aware_input(
          commands::Request::NO_LANGUAGE_AWARE_INPUT);
      type = DictionaryPredictor::GetPredictionTypes(*convreq_, segments);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);

      // Language aware input is on.
      request_->set_language_aware_input(
          commands::Request::LANGUAGE_AWARE_SUGGESTION);
      type = DictionaryPredictor::GetPredictionTypes(*convreq_, segments);
      EXPECT_EQ(0, type & DictionaryPredictor::ENGLISH);
    }

    config_->set_use_dictionary_suggest(orig_use_dictionary_suggest);
    request_->set_language_aware_input(orig_lang_aware);
    request_->set_special_romanji_table(orig_table);
    composer_->SetInputMode(orig_input_mode);
  }
}

TEST_F(DictionaryPredictorTest, GetPredictionTypesTestWithTypingCorrection) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  config_->set_use_typing_correction(true);

  MakeSegmentsForSuggestion("ｐはよう", &segments);
  EXPECT_EQ(
      DictionaryPredictor::UNIGRAM | DictionaryPredictor::TYPING_CORRECTION,
      DictionaryPredictor::GetPredictionTypes(*convreq_, segments));
}

TEST_F(DictionaryPredictorTest, GetPredictionTypesTestWithZeroQuerySuggestion) {
  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  request_->set_zero_query_suggestion(true);

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // empty segments
  {
    EXPECT_EQ(
        DictionaryPredictor::NO_PREDICTION,
        predictor->GetPredictionTypes(*convreq_, segments));
  }

  // normal segments
  {
    MakeSegmentsForSuggestion("てすとだよ", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              predictor->GetPredictionTypes(*convreq_, segments));

    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              predictor->GetPredictionTypes(*convreq_, segments));

    segments.set_request_type(Segments::CONVERSION);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION,
              predictor->GetPredictionTypes(*convreq_, segments));
  }

  // short key
  {
    MakeSegmentsForSuggestion("て", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              predictor->GetPredictionTypes(*convreq_, segments));

    // on prediction mode, return UNIGRAM
    segments.set_request_type(Segments::PREDICTION);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM,
              predictor->GetPredictionTypes(*convreq_, segments));
  }

  // History is short => UNIGRAM
  {
    MakeSegmentsForSuggestion("てすとだよ", &segments);
    PrependHistorySegments("A", "A", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::SUFFIX,
              predictor->GetPredictionTypes(*convreq_, segments));
  }

  // both History and current segment are long => UNIGRAM|BIGRAM
  {
    MakeSegmentsForSuggestion("てすとだよ", &segments);
    PrependHistorySegments("てすとだよ", "abc", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::BIGRAM |
                  DictionaryPredictor::SUFFIX,
              predictor->GetPredictionTypes(*convreq_, segments));
  }

  {
    MakeSegmentsForSuggestion("A", &segments);
    PrependHistorySegments("てすとだよ", "abc", &segments);
    EXPECT_EQ(DictionaryPredictor::BIGRAM | DictionaryPredictor::UNIGRAM |
                  DictionaryPredictor::SUFFIX,
              predictor->GetPredictionTypes(*convreq_, segments));
  }

  {
    MakeSegmentsForSuggestion("", &segments);
    PrependHistorySegments("て", "abc", &segments);
    EXPECT_EQ(DictionaryPredictor::SUFFIX,
              predictor->GetPredictionTypes(*convreq_, segments));
  }

  {
    MakeSegmentsForSuggestion("A", &segments);
    PrependHistorySegments("て", "abc", &segments);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM | DictionaryPredictor::SUFFIX,
              predictor->GetPredictionTypes(*convreq_, segments));
  }

  {
    MakeSegmentsForSuggestion("", &segments);
    PrependHistorySegments("てすとだよ", "abc", &segments);
    EXPECT_EQ(DictionaryPredictor::BIGRAM | DictionaryPredictor::SUFFIX,
              predictor->GetPredictionTypes(*convreq_, segments));
  }
}

TEST_F(DictionaryPredictorTest, AggregateUnigramPrediction) {
  Segments segments;
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  const char kKey[] = "ぐーぐるあ";

  MakeSegmentsForSuggestion(kKey, &segments);

  std::vector<DictionaryPredictor::Result> results;

  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::BIGRAM,
      *convreq_, segments, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::REALTIME,
      *convreq_, segments, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::UNIGRAM,
      *convreq_, segments, &results);
  EXPECT_FALSE(results.empty());

  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[i].types);
    EXPECT_TRUE(Util::StartsWith(results[i].key, kKey));
  }

  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, AggregateUnigramCandidateForMixedConversion) {
  const char kHiraganaA[] = "あ";

  DictionaryMock mock_dict;
  // A system dictionary entry "a".
  mock_dict.AddLookupPredictive(kHiraganaA, kHiraganaA, "a", Token::NONE);
  // System dictionary entries "a0", ..., "a9", which are detected as redundant
  // by MaybeRedundant(); see dictionary_predictor.cc.
  for (int i = 0; i < 10; ++i) {
    mock_dict.AddLookupPredictive(kHiraganaA, kHiraganaA,
                                  Util::StringPrintf("a%d", i), Token::NONE);
  }
  // A user dictionary entry "aaa".  MaybeRedundant() detects this entry as
  // redundant but it should not be filtered in prediction.
  mock_dict.AddLookupPredictive(kHiraganaA, kHiraganaA, "aaa",
                                Token::USER_DICTIONARY);

  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);
  table_->LoadFromFile("system://12keys-hiragana.tsv");
  composer_->SetTable(table_.get());
  InsertInputSequence(kHiraganaA, composer_.get());
  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  segment->set_key(kHiraganaA);

  std::vector<DictionaryPredictor::Result> results;
  DictionaryPredictor::AggregateUnigramCandidateForMixedConversion(
      mock_dict, *convreq_, segments, &results);

  // Check if "aaa" is not filtered.
  auto iter = results.begin();
  for (; iter != results.end(); ++iter) {
    if (iter->key == kHiraganaA && iter->value == "aaa" &&
        iter->IsUserDictionaryResult()) {
      break;
    }
  }
  EXPECT_NE(results.end(), iter);
}

TEST_F(DictionaryPredictorTest, AggregateBigramPrediction) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  {
    Segments segments;

    MakeSegmentsForSuggestion("あ", &segments);

    // history is "グーグル"
    const char kHistoryKey[] = "ぐーぐる";
    const char kHistoryValue[] = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(DictionaryPredictor::UNIGRAM,
                                         *convreq_, segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateBigramPrediction(DictionaryPredictor::REALTIME,
                                         *convreq_, segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateBigramPrediction(DictionaryPredictor::BIGRAM, *convreq_,
                                         segments, &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      // "グーグルアドセンス", "グーグル", "アドセンス"
      // are in the dictionary.
      if (results[i].value == "グーグルアドセンス") {
        EXPECT_EQ(DictionaryPredictor::BIGRAM, results[i].types);
      } else {
        EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[i].types);
      }
      EXPECT_TRUE(Util::StartsWith(results[i].key, kHistoryKey));
      EXPECT_TRUE(Util::StartsWith(results[i].value, kHistoryValue));
      // Not zero query
      EXPECT_FALSE(results[i].source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
    }

    EXPECT_EQ(1, segments.conversion_segments_size());
  }

  {
    Segments segments;

    MakeSegmentsForSuggestion("あ", &segments);

    const char kHistoryKey[] = "てす";
    const char kHistoryValue[] = "テス";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(DictionaryPredictor::BIGRAM, *convreq_,
                                         segments, &results);
    EXPECT_TRUE(results.empty());
  }
}

TEST_F(DictionaryPredictorTest, AggregateZeroQueryBigramPrediction) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  {
    Segments segments;

    // Zero query
    MakeSegmentsForSuggestion("", &segments);

    // history is "グーグル"
    const char kHistoryKey[] = "ぐーぐる";
    const char kHistoryValue[] = "グーグル";

    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

    std::vector<DictionaryPredictor::Result> results;

    predictor->AggregateBigramPrediction(DictionaryPredictor::UNIGRAM,
                                         *convreq_, segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateBigramPrediction(DictionaryPredictor::REALTIME,
                                         *convreq_, segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateBigramPrediction(DictionaryPredictor::BIGRAM, *convreq_,
                                         segments, &results);
    EXPECT_FALSE(results.empty());

    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_TRUE(Util::StartsWith(results[i].key, kHistoryKey));
      EXPECT_TRUE(Util::StartsWith(results[i].value, kHistoryValue));
      // Zero query
      EXPECT_FALSE(results[i].source_info &
                   Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX);
    }
  }
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSize) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;

  // GetRealtimeCandidateMaxSize has some heuristics so here we test following
  // conditions.
  // - The result must be equal or less than kMaxSize;
  // - If mixed_conversion is the same, the result of SUGGESTION is
  //        equal or less than PREDICTION.
  // - If mixed_conversion is the same, the result of PARTIAL_SUGGESTION is
  //        equal or less than PARTIAL_PREDICTION.
  // - Partial version has equal or greater than non-partial version.

  const size_t kMaxSize = 100;

  // non-partial, non-mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, prediction_no_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, suggestion_no_mixed);
  EXPECT_LE(suggestion_no_mixed, prediction_no_mixed);

  // non-partial, mixed-conversion
  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, prediction_mixed);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, suggestion_mixed);

  // partial, non-mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, partial_prediction_no_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_no_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, false, kMaxSize);
  EXPECT_GE(kMaxSize, partial_suggestion_no_mixed);
  EXPECT_LE(partial_suggestion_no_mixed, partial_prediction_no_mixed);

  // partial, mixed-conversion
  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  const size_t partial_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, partial_prediction_mixed);

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  const size_t partial_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, partial_suggestion_mixed);
  EXPECT_LE(partial_suggestion_mixed, partial_prediction_mixed);

  EXPECT_GE(partial_prediction_no_mixed, prediction_no_mixed);
  EXPECT_GE(partial_prediction_mixed, prediction_mixed);
  EXPECT_GE(partial_suggestion_no_mixed, suggestion_no_mixed);
  EXPECT_GE(partial_suggestion_mixed, suggestion_mixed);
}

TEST_F(DictionaryPredictorTest, GetRealtimeCandidateMaxSizeForMixed) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;
  Segment *segment = segments.add_segment();

  const size_t kMaxSize = 100;

  // for short key, try to provide many results as possible
  segment->set_key("short");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t short_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, short_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t short_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, short_prediction_mixed);

  // for long key, provide few results
  segment->set_key("long_request_key");
  segments.set_request_type(Segments::SUGGESTION);
  const size_t long_suggestion_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, long_suggestion_mixed);
  EXPECT_GT(short_suggestion_mixed, long_suggestion_mixed);

  segments.set_request_type(Segments::PREDICTION);
  const size_t long_prediction_mixed =
      predictor->GetRealtimeCandidateMaxSize(segments, true, kMaxSize);
  EXPECT_GE(kMaxSize, long_prediction_mixed);
  EXPECT_GT(kMaxSize, long_prediction_mixed + long_suggestion_mixed);
  EXPECT_GT(short_prediction_mixed, long_prediction_mixed);
}

TEST_F(DictionaryPredictorTest, AggregateRealtimeConversion) {
  testing::MockDataManager data_manager;
  unique_ptr<const DictionaryInterface> dictionary(new DictionaryMock);
  unique_ptr<ConverterMock> converter(new ConverterMock);
  unique_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterMock);
  unique_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  unique_ptr<const Connector> connector(
      Connector::CreateFromDataManager(data_manager));
  unique_ptr<const Segmenter> segmenter(
      Segmenter::CreateFromDataManager(data_manager));
  unique_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  const dictionary::POSMatcher pos_matcher(data_manager.GetPOSMatcherData());
  unique_ptr<TestableDictionaryPredictor> predictor(
      new TestableDictionaryPredictor(data_manager,
                                      converter.get(),
                                      immutable_converter.get(),
                                      dictionary.get(),
                                      suffix_dictionary.get(),
                                      connector.get(),
                                      segmenter.get(),
                                      &pos_matcher,
                                      suggestion_filter.get()));

  const char kKey[] = "わたしのなまえはなかのです";

  // Set up mock converter
  {
    // Make segments like:
    // "わたしの"    | "なまえは" | "なかのです"
    // "Watashino" | "Namaeha" | "Nakanodesu"
    Segments segments;

    Segment *segment = segments.add_segment();
    segment->set_key("わたしの");
    segment->add_candidate()->value = "Watashino";

    segment = segments.add_segment();
    segment->set_key("なまえは");
    segment->add_candidate()->value = "Namaeha";

    segment = segments.add_segment();
    segment->set_key("なかのです");
    segment->add_candidate()->value = "Nakanodesu";

    converter->SetStartConversionForRequest(&segments, true);
  }

  // A test case with use_actual_converter_for_realtime_conversion being false,
  // i.e., realtime conversion result is generated by ImmutableConverterMock.
  {
    Segments segments;

    MakeSegmentsForSuggestion(kKey, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    convreq_->set_use_actual_converter_for_realtime_conversion(false);

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::UNIGRAM, *convreq_, &segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::BIGRAM, *convreq_, &segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::REALTIME, *convreq_, &segments, &results);

    ASSERT_EQ(1, results.size());
    EXPECT_EQ(TestableDictionaryPredictor::REALTIME, results[0].types);
    EXPECT_EQ(kKey, results[0].key);
    EXPECT_EQ(3, results[0].inner_segment_boundary.size());
  }

  // A test case with use_actual_converter_for_realtime_conversion being true,
  // i.e., realtime conversion result is generated by ConverterMock.
  {
    Segments segments;

    MakeSegmentsForSuggestion(kKey, &segments);

    std::vector<TestableDictionaryPredictor::Result> results;
    convreq_->set_use_actual_converter_for_realtime_conversion(true);

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::UNIGRAM, *convreq_, &segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::BIGRAM, *convreq_, &segments, &results);
    EXPECT_TRUE(results.empty());

    predictor->AggregateRealtimeConversion(
        TestableDictionaryPredictor::REALTIME, *convreq_, &segments, &results);

    // When |request.use_actual_converter_for_realtime_conversion| is true, the
    // extra label REALTIME_TOP is expected to be added.
    ASSERT_EQ(2, results.size());
    bool realtime_top_found = false;
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(TestableDictionaryPredictor::REALTIME |
                TestableDictionaryPredictor::REALTIME_TOP, results[i].types);
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

namespace {

struct SimpleSuffixToken {
  const char *key;
  const char *value;
};

const SimpleSuffixToken kSuffixTokens[] = {
    {"いか", "以下"}
};

class TestSuffixDictionary : public DictionaryInterface {
 public:
  TestSuffixDictionary() = default;
  ~TestSuffixDictionary() override = default;

  bool HasKey(StringPiece value) const override { return false; }

  bool HasValue(StringPiece value) const override { return false; }

  void LookupPredictive(StringPiece key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override {
    Token token;
    for (size_t i = 0; i < arraysize(kSuffixTokens); ++i) {
      const SimpleSuffixToken &suffix_token = kSuffixTokens[i];
      if (!key.empty() && !Util::StartsWith(suffix_token.key, key)) {
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

  void LookupPrefix(StringPiece key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override {}

  void LookupExact(StringPiece key, const ConversionRequest &conversion_request,
                   Callback *callback) const override {}

  void LookupReverse(StringPiece str,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override {}
};

}  // namespace

TEST_F(DictionaryPredictorTest, GetCandidateCutoffThreshold) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  Segments segments;

  segments.set_request_type(Segments::PREDICTION);
  const size_t prediction =
      predictor->GetCandidateCutoffThreshold(segments);

  segments.set_request_type(Segments::SUGGESTION);
  const size_t suggestion =
      predictor->GetCandidateCutoffThreshold(segments);
  EXPECT_LE(suggestion, prediction);
}

TEST_F(DictionaryPredictorTest, AggregateSuffixPrediction) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(new MockDataAndPredictor);
  data_and_predictor->Init(NULL, new TestSuffixDictionary());

  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;

  MakeSegmentsForSuggestion("あ", &segments);

  // history is "グーグル"
  const char kHistoryKey[] = "ぐーぐる";
  const char kHistoryValue[] = "グーグル";

  PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

  std::vector<DictionaryPredictor::Result> results;

  // Since SuffixDictionary only returns when key is "い".
  // result should be empty.
  predictor->AggregateSuffixPrediction(DictionaryPredictor::SUFFIX, *convreq_,
                                       segments, &results);
  EXPECT_TRUE(results.empty());

  results.clear();
  segments.mutable_conversion_segment(0)->set_key("");
  predictor->AggregateSuffixPrediction(DictionaryPredictor::SUFFIX, *convreq_,
                                       segments, &results);
  EXPECT_FALSE(results.empty());

  results.clear();
  predictor->AggregateSuffixPrediction(DictionaryPredictor::UNIGRAM, *convreq_,
                                       segments, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateSuffixPrediction(DictionaryPredictor::REALTIME, *convreq_,
                                       segments, &results);
  EXPECT_TRUE(results.empty());

  predictor->AggregateSuffixPrediction(DictionaryPredictor::BIGRAM, *convreq_,
                                       segments, &results);
  EXPECT_TRUE(results.empty());

  // Candidates generated by AggregateSuffixPrediction should have SUFFIX type.
  results.clear();
  segments.mutable_conversion_segment(0)->set_key("い");
  predictor->AggregateSuffixPrediction(
      DictionaryPredictor::SUFFIX | DictionaryPredictor::BIGRAM, *convreq_,
      segments, &results);
  EXPECT_FALSE(results.empty());
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(DictionaryPredictor::SUFFIX, results[i].types);
    // Not zero query
    EXPECT_FALSE(Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX &
                 results[i].source_info);
  }
}

TEST_F(DictionaryPredictorTest, AggregateZeroQuerySuffixPrediction) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(new MockDataAndPredictor);
  data_and_predictor->Init(NULL, new TestSuffixDictionary());

  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  Segments segments;

  // Zero query
  MakeSegmentsForSuggestion("", &segments);

  // history is "グーグル"
  const char kHistoryKey[] = "ぐーぐる";
  const char kHistoryValue[] = "グーグル";

  PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);

  std::vector<DictionaryPredictor::Result> results;

  // Candidates generated by AggregateSuffixPrediction should have SUFFIX type.
  predictor->AggregateSuffixPrediction(DictionaryPredictor::SUFFIX, *convreq_,
                                       segments, &results);
  EXPECT_FALSE(results.empty());
  for (size_t i = 0; i < results.size(); ++i) {
    EXPECT_EQ(DictionaryPredictor::SUFFIX, results[i].types);
    // Zero query
    EXPECT_TRUE(Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX &
                results[i].source_info);
  }
}

TEST_F(DictionaryPredictorTest, AggregateEnglishPrediction) {
  // Input mode: HALF_ASCII, Key: lower case
  //   => Prediction should be in half-width lower case.
  {
    const char *kExpectedValues[] = {
        "converge",
        "converged",
        "convergent",
    };
    AggregateEnglishPredictionTestHelper(transliteration::HALF_ASCII, "conv",
                                         "conv", kExpectedValues,
                                         arraysize(kExpectedValues));
  }
  // Input mode: HALF_ASCII, Key: upper case
  //   => Prediction should be in half-width upper case.
  {
    const char *kExpectedValues[] = {
        "CONVERGE",
        "CONVERGED",
        "CONVERGENT",
    };
    AggregateEnglishPredictionTestHelper(transliteration::HALF_ASCII, "CONV",
                                         "CONV", kExpectedValues,
                                         arraysize(kExpectedValues));
  }
  // Input mode: HALF_ASCII, Key: capitalized
  //   => Prediction should be half-width and capitalized
  {
    const char *kExpectedValues[] = {
        "Converge",
        "Converged",
        "Convergent",
    };
    AggregateEnglishPredictionTestHelper(transliteration::HALF_ASCII, "Conv",
                                         "Conv", kExpectedValues,
                                         arraysize(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: lower case
  //   => Prediction should be in full-wdith lower case.
  {
    const char *kExpectedValues[] = {
        "ｃｏｎｖｅｒｇｅ",
        "ｃｏｎｖｅｒｇｅｄ",
        "ｃｏｎｖｅｒｇｅｎｔ",
    };
    AggregateEnglishPredictionTestHelper(transliteration::FULL_ASCII, "conv",
                                         "ｃｏｎｖ",
                                         kExpectedValues,
                                         arraysize(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: upper case
  //   => Prediction should be in full-width upper case.
  {
    const char *kExpectedValues[] = {
        "ＣＯＮＶＥＲＧＥ",
        "ＣＯＮＶＥＲＧＥＤ",
        "ＣＯＮＶＥＲＧＥＮＴ",
    };
    AggregateEnglishPredictionTestHelper(transliteration::FULL_ASCII, "CONV",
                                         "ＣＯＮＶ",
                                         kExpectedValues,
                                         arraysize(kExpectedValues));
  }
  // Input mode: FULL_ASCII, Key: capitalized
  //   => Prediction should be full-width and capitalized
  {
    const char *kExpectedValues[] = {
        "Ｃｏｎｖｅｒｇｅ",
        "Ｃｏｎｖｅｒｇｅｄ",
        "Ｃｏｎｖｅｒｇｅｎｔ",
    };
    AggregateEnglishPredictionTestHelper(transliteration::FULL_ASCII, "Conv",
                                         "Ｃｏｎｖ",
                                         kExpectedValues,
                                         arraysize(kExpectedValues));
  }
}

TEST_F(DictionaryPredictorTest, AggregateTypeCorrectingPrediction) {
  config_->set_use_typing_correction(true);

  const char kInputText[] = "gu-huru";
  const uint32 kCorrectedKeyCodes[] = {'g', 'u', '-', 'g', 'u', 'r', 'u'};
  const char *kExpectedValues[] = {
      "グーグルアドセンス",
      "グーグルアドワーズ",
  };
  AggregateTypeCorrectingTestHelper(kInputText, kCorrectedKeyCodes,
                                    kExpectedValues,
                                    arraysize(kExpectedValues));
}

TEST_F(DictionaryPredictorTest, ZeroQuerySuggestionAfterNumbers) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  const POSMatcher &pos_matcher = data_and_predictor->pos_matcher();
  Segments segments;

  {
    MakeSegmentsForSuggestion("", &segments);

    const char kHistoryKey[] = "12";
    const char kHistoryValue[] = "12";
    const char kExpectedValue[] = "月";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateSuffixPrediction(DictionaryPredictor::SUFFIX, *convreq_,
                                         segments, &results);
    EXPECT_FALSE(results.empty());

    std::vector<DictionaryPredictor::Result>::const_iterator target =
        results.end();
    for (std::vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);

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

    // Make sure number suffixes are not suggested when there is a key
    results.clear();
    MakeSegmentsForSuggestion("あ", &segments);
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    predictor->AggregateSuffixPrediction(DictionaryPredictor::SUFFIX, *convreq_,
                                         segments, &results);
    target = results.end();
    for (std::vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->value == kExpectedValue) {
        target = it;
        break;
      }
    }
    EXPECT_EQ(results.end(), target);
  }

  {
    MakeSegmentsForSuggestion("", &segments);

    const char kHistoryKey[] = "66050713";  // A random number
    const char kHistoryValue[] = "66050713";
    const char kExpectedValue[] = "個";
    PrependHistorySegments(kHistoryKey, kHistoryValue, &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateSuffixPrediction(DictionaryPredictor::SUFFIX, *convreq_,
                                         segments, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (std::vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
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

TEST_F(DictionaryPredictorTest, TriggerNumberZeroQuerySuggestion) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();
  const POSMatcher &pos_matcher = data_and_predictor->pos_matcher();

  const struct TestCase {
    const char *history_key;
    const char *history_value;
    const char *find_suffix_value;
    bool expected_result;
  } kTestCases[] = {
      {"12", "12", "月", true},
      {"12", "１２", "月", true},
      {"12", "壱拾弐", "月", false},
      {"12", "十二", "月", false},
      {"12", "一二", "月", false},
      {"12", "Ⅻ", "月", false},
      {"あか", "12", "月", true},    // T13N
      {"あか", "１２", "月", true},  // T13N
      {"じゅう", "10", "時", true},
      {"じゅう", "１０", "時", true},
      {"じゅう", "十", "時", false},
      {"じゅう", "拾", "時", false},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    Segments segments;
    MakeSegmentsForSuggestion("", &segments);

    const TestCase &test_case = kTestCases[i];
    PrependHistorySegments(
        test_case.history_key, test_case.history_value, &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateSuffixPrediction(
        DictionaryPredictor::SUFFIX,
        *convreq_, segments, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (std::vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->value == test_case.find_suffix_value &&
          it->lid == pos_matcher.GetCounterSuffixWordId()) {
        EXPECT_TRUE(
          Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX &
          it->source_info);
        found = true;
        break;
      }
    }
    EXPECT_EQ(test_case.expected_result, found) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictorTest, TriggerZeroQuerySuggestion) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  const struct TestCase {
    const char *history_key;
    const char *history_value;
    const char *find_value;
    bool expected_result;
  } kTestCases[] = {
      {"@", "@", "gmail.com", true},
      {"!", "!", "?", false},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    Segments segments;
    MakeSegmentsForSuggestion("", &segments);

    const TestCase &test_case = kTestCases[i];
    PrependHistorySegments(
        test_case.history_key, test_case.history_value, &segments);
    std::vector<DictionaryPredictor::Result> results;
    predictor->AggregateSuffixPrediction(
        DictionaryPredictor::SUFFIX,
        *convreq_, segments, &results);
    EXPECT_FALSE(results.empty());

    bool found = false;
    for (std::vector<DictionaryPredictor::Result>::const_iterator it =
             results.begin();
         it != results.end(); ++it) {
      EXPECT_EQ(it->types, DictionaryPredictor::SUFFIX);
      if (it->value == test_case.find_value &&
          it->lid == 0 /* EOS */) {
        found = true;
        break;
      }
    }
    EXPECT_EQ(test_case.expected_result, found) << test_case.history_value;
  }
}

TEST_F(DictionaryPredictorTest, GetHistoryKeyAndValue) {
  Segments segments;
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  MakeSegmentsForSuggestion("test", &segments);

  string key, value;
  EXPECT_FALSE(predictor->GetHistoryKeyAndValue(segments, &key, &value));

  PrependHistorySegments("key", "value", &segments);
  EXPECT_TRUE(predictor->GetHistoryKeyAndValue(segments, &key, &value));
  EXPECT_EQ("key", key);
  EXPECT_EQ("value", value);
}

TEST_F(DictionaryPredictorTest, IsZipCodeRequest) {
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest(""));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("000"));
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest("ABC"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("---"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("0124-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-0"));
  EXPECT_TRUE(DictionaryPredictor::IsZipCodeRequest("012-3456"));
  EXPECT_FALSE(DictionaryPredictor::IsZipCodeRequest("０１２-０"));
}

TEST_F(DictionaryPredictorTest, IsAggressiveSuggestion) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // "ただしい",
  // "ただしいけめんにかぎる",
  EXPECT_TRUE(predictor->IsAggressiveSuggestion(
      4,      // query_len
      11,     // key_len
      6000,   // cost
      true,   // is_suggestion
      20));   // total_candidates_size

  // cost <= 4000
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      11,
      4000,
      true,
      20));

  // not suggestion
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      11,
      4000,
      false,
      20));

  // total_candidates_size is small
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      11,
      4000,
      true,
      5));

  // query_length = 5
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      5,
      11,
      6000,
      true,
      20));

  // "それでも",
  // "それでもぼくはやっていない",
  EXPECT_TRUE(predictor->IsAggressiveSuggestion(
      4,
      13,
      6000,
      true,
      20));

  // cost <= 4000
  EXPECT_FALSE(predictor->IsAggressiveSuggestion(
      4,
      13,
      4000,
      true,
      20));
}

TEST_F(DictionaryPredictorTest, RealtimeConversionStartingWithAlphabets) {
  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  const char kKey[] = "PCてすと";
  const char *kExpectedSuggestionValues[] = {
      "Realtime top result",
      "PCテスト",
  };

  // Set up mock converter for realtime top result.
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kKey);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = kExpectedSuggestionValues[0];
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }

  MakeSegmentsForSuggestion(kKey, &segments);

  std::vector<DictionaryPredictor::Result> results;

  convreq_->set_use_actual_converter_for_realtime_conversion(false);
  predictor->AggregateRealtimeConversion(
      DictionaryPredictor::REALTIME, *convreq_, &segments, &results);
  ASSERT_EQ(1, results.size());

  EXPECT_EQ(DictionaryPredictor::REALTIME, results[0].types);
  EXPECT_EQ(kExpectedSuggestionValues[1], results[0].value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, RealtimeConversionWithSpellingCorrection) {
  Segments segments;
  // turn on real-time conversion
  config_->set_use_dictionary_suggest(false);
  config_->set_use_realtime_conversion(true);

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  const char kCapriHiragana[] = "かぷりちょうざ";

  // Set up mock converter for realtime top result.
  {
    Segments segments;
    Segment *segment = segments.add_segment();
    segment->set_key(kCapriHiragana);
    Segment::Candidate *candidate = segment->add_candidate();
    candidate->value = "Dummy";
    ConverterMock *converter = data_and_predictor->mutable_converter_mock();
    converter->SetStartConversionForRequest(&segments, true);
  }

  MakeSegmentsForSuggestion(kCapriHiragana, &segments);

  std::vector<DictionaryPredictor::Result> results;

  convreq_->set_use_actual_converter_for_realtime_conversion(false);
  predictor->AggregateUnigramPrediction(
      DictionaryPredictor::UNIGRAM,
      *convreq_, segments, &results);
  ASSERT_FALSE(results.empty());
  EXPECT_NE(0, (results[0].candidate_attributes &
                Segment::Candidate::SPELLING_CORRECTION));

  results.clear();

  const char kKeyWithDe[] = "かぷりちょうざで";
  const char kExpectedSuggestionValueWithDe[] = "カプリチョーザで";

  MakeSegmentsForSuggestion(kKeyWithDe, &segments);
  predictor->AggregateRealtimeConversion(
      DictionaryPredictor::REALTIME, *convreq_, &segments, &results);
  EXPECT_EQ(1, results.size());

  EXPECT_EQ(results[0].types, DictionaryPredictor::REALTIME);
  EXPECT_NE(0, (results[0].candidate_attributes &
                Segment::Candidate::SPELLING_CORRECTION));
  EXPECT_EQ(kExpectedSuggestionValueWithDe, results[0].value);
  EXPECT_EQ(1, segments.conversion_segments_size());
}

TEST_F(DictionaryPredictorTest, GetMissSpelledPosition) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  EXPECT_EQ(0, predictor->GetMissSpelledPosition("", ""));
  EXPECT_EQ(3,
            predictor->GetMissSpelledPosition("れみおめろん", "レミオロメン"));
  EXPECT_EQ(5,
            predictor->GetMissSpelledPosition("とーとばっく", "トートバッグ"));
  EXPECT_EQ(
      4, predictor->GetMissSpelledPosition("おーすとりらあ", "オーストラリア"));
  EXPECT_EQ(7, predictor->GetMissSpelledPosition("じきそうしょう", "時期尚早"));
}

TEST_F(DictionaryPredictorTest, RemoveMissSpelledCandidates) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっぐ";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バック";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(1, &results);
    ASSERT_EQ(3, results.size());

    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[0].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[1].types);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[2].types);
  }

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "てすと";
    result->value = "テスト";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[0].types);
    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[1].types);
  }

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バック";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(1, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[0].types);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[1].types);
  }

  {
    std::vector<DictionaryPredictor::Result> results;
    DictionaryPredictor::Result *result;

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バッグ";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::SPELLING_CORRECTION);

    results.push_back(DictionaryPredictor::Result());
    result = &results.back();
    result->key = "ばっく";
    result->value = "バック";
    result->SetTypesAndTokenAttributes(DictionaryPredictor::UNIGRAM,
                                       Token::NONE);

    predictor->RemoveMissSpelledCandidates(3, &results);
    CHECK_EQ(2, results.size());

    EXPECT_EQ(DictionaryPredictor::UNIGRAM, results[0].types);
    EXPECT_EQ(DictionaryPredictor::NO_PREDICTION, results[1].types);
  }
}

TEST_F(DictionaryPredictorTest, UseExpansionForUnigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForUnigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForUnigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForUnigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForBigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForBigramTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForBigramTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForBigramTestHelper(false);
}

TEST_F(DictionaryPredictorTest, UseExpansionForSuffixTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  ExpansionForSuffixTestHelper(true);
}

TEST_F(DictionaryPredictorTest, UnuseExpansionForSuffixTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = false;
  ExpansionForSuffixTestHelper(false);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForRomanTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);

  table_->LoadFromFile("system://romanji-hiragana.tsv");
  composer_->SetTable(table_.get());
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  InsertInputSequence("ak", composer_.get());
  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    string query;
    composer_->GetQueryForPrediction(&query);
    segment->set_key(query);
    EXPECT_EQ("あ", query);
  }
  {
    string base;
    std::set<string> expanded;
    composer_->GetQueriesForPrediction(&base, &expanded);
    EXPECT_EQ("あ", base);
    EXPECT_GT(expanded.size(), 5);
  }

  std::vector<TestableDictionaryPredictor::Result> results;
  TestableDictionaryPredictor::Result *result;

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あか";
  result->value = "赤";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あき";
  result->value = "秋";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あかぎ";
  result->value = "アカギ";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  EXPECT_EQ(3, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);

  predictor->ApplyPenaltyForKeyExpansion(segments, &results);

  // no penalties
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
}

TEST_F(DictionaryPredictorTest, ExpansionPenaltyForKanaTest) {
  FLAGS_enable_expansion_for_dictionary_predictor = true;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(false);

  table_->LoadFromFile("system://kana.tsv");
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  InsertInputSequence("あし", composer_.get());

  Segment *segment = segments.add_segment();
  CHECK(segment);
  {
    string query;
    composer_->GetQueryForPrediction(&query);
    segment->set_key(query);
    EXPECT_EQ("あし", query);
  }
  {
    string base;
    std::set<string> expanded;
    composer_->GetQueriesForPrediction(&base, &expanded);
    EXPECT_EQ("あ", base);
    EXPECT_EQ(2, expanded.size());
  }

  std::vector<TestableDictionaryPredictor::Result> results;
  TestableDictionaryPredictor::Result *result;

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あし";
  result->value = "足";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あじ";
  result->value = "味";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あした";
  result->value = "明日";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "あじあ";
  result->value = "アジア";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  EXPECT_EQ(4, results.size());
  EXPECT_EQ(0, results[0].cost);
  EXPECT_EQ(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_EQ(0, results[3].cost);

  predictor->ApplyPenaltyForKeyExpansion(segments, &results);

  EXPECT_EQ(0, results[0].cost);
  EXPECT_LT(0, results[1].cost);
  EXPECT_EQ(0, results[2].cost);
  EXPECT_LT(0, results[3].cost);
}

TEST_F(DictionaryPredictorTest, SetLMCost) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  CHECK(segment);
  segment->set_key("てすと");

  std::vector<TestableDictionaryPredictor::Result> results;
  TestableDictionaryPredictor::Result *result;

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "てすと";
  result->value = "てすと";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "てすと";
  result->value = "テスト";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
  result = &results.back();
  result->key = "てすとてすと";
  result->value = "テストテスト";
  result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::UNIGRAM,
                                     Token::NONE);

  predictor->SetLMCost(segments, &results);

  EXPECT_EQ(3, results.size());
  EXPECT_EQ("てすと", results[0].value);
  EXPECT_EQ("テスト", results[1].value);
  EXPECT_EQ("テストテスト", results[2].value);
  EXPECT_GT(results[2].cost, results[0].cost);
  EXPECT_GT(results[2].cost, results[1].cost);
}

namespace {

void AddTestableDictionaryPredictorResult(
    const char *key, const char *value, int wcost,
    TestableDictionaryPredictor::PredictionTypes prediction_types,
    Token::AttributesBitfield attributes,
    std::vector<TestableDictionaryPredictor::Result> *results) {
  results->push_back(TestableDictionaryPredictor::MakeEmptyResult());
  TestableDictionaryPredictor::Result *result = &results->back();
  result->key = key;
  result->value = value;
  result->wcost = wcost;
  result->SetTypesAndTokenAttributes(prediction_types, attributes);
}

}  // namespace

TEST_F(DictionaryPredictorTest, SetLMCostForUserDictionaryWord) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  const char *kAikaHiragana = "あいか";
  const char *kAikaKanji = "愛佳";

  Segments segments;
  segments.set_request_type(Segments::PREDICTION);
  Segment *segment = segments.add_segment();
  ASSERT_NE(nullptr, segment);
  segment->set_key(kAikaHiragana);

  {
    // Cost of words in user dictionary should be decreased.
    const int kOrigianlWordCost = 10000;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOrigianlWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::USER_DICTIONARY,
        &results);

    predictor->SetLMCost(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_GT(kOrigianlWordCost, results[0].cost);
    EXPECT_LE(1, results[0].cost);
  }

  {
    // Cost of words in user dictionary should not be decreased to below 1.
    const int kOrigianlWordCost = 10;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOrigianlWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::USER_DICTIONARY,
        &results);

    predictor->SetLMCost(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_GT(kOrigianlWordCost, results[0].cost);
    EXPECT_LE(1, results[0].cost);
  }

  {
    // Cost of general symbols should not be decreased.
    const int kOrigianlWordCost = 10000;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOrigianlWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::USER_DICTIONARY,
        &results);
    ASSERT_EQ(1, results.size());
    results[0].lid = data_and_predictor->pos_matcher().GetGeneralSymbolId();
    results[0].rid = results[0].lid;
    predictor->SetLMCost(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_LE(kOrigianlWordCost, results[0].cost);
  }

  {
    // Cost of words not in user dictionary should not be decreased.
    const int kOrigianlWordCost = 10000;
    std::vector<TestableDictionaryPredictor::Result> results;
    AddTestableDictionaryPredictorResult(
        kAikaHiragana, kAikaKanji, kOrigianlWordCost,
        TestableDictionaryPredictor::UNIGRAM, Token::NONE,
        &results);

    predictor->SetLMCost(segments, &results);

    EXPECT_EQ(1, results.size());
    EXPECT_EQ(kAikaKanji, results[0].value);
    EXPECT_EQ(kOrigianlWordCost, results[0].cost);
  }
}

TEST_F(DictionaryPredictorTest, SuggestSpellingCorrection) {
  testing::MockDataManager data_manager;

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  MakeSegmentsForPrediction("あぼがど", &segments);

  predictor->PredictForRequest(*convreq_, &segments);

  EXPECT_TRUE(FindCandidateByValue(segments.conversion_segment(0), "アボカド"));
}

TEST_F(DictionaryPredictorTest, DoNotSuggestSpellingCorrectionBeforeMismatch) {
  testing::MockDataManager data_manager;

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  MakeSegmentsForPrediction("あぼが", &segments);

  predictor->PredictForRequest(*convreq_, &segments);

  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "アボカド"));
}

TEST_F(DictionaryPredictorTest, MobileUnigramSuggestion) {
  testing::MockDataManager data_manager;

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  const char kKey[] = "とうきょう";

  MakeSegmentsForSuggestion(kKey, &segments);

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  std::vector<TestableDictionaryPredictor::Result> results;
  predictor->AggregateUnigramPrediction(TestableDictionaryPredictor::UNIGRAM,
                                        *convreq_, segments, &results);

  EXPECT_TRUE(FindResultByValue(results, "東京"));

  int prefix_count = 0;
  for (size_t i = 0; i < results.size(); ++i) {
    if (Util::StartsWith(results[i].value, "東京")) {
      ++prefix_count;
    }
  }
  // Should not have same prefix candidates a lot.
  EXPECT_LE(prefix_count, 6);
}

TEST_F(DictionaryPredictorTest, MobileZeroQuerySuggestion) {
  testing::MockDataManager data_manager;

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  MakeSegmentsForPrediction("", &segments);

  PrependHistorySegments("だいがく", "大学", &segments);

  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  predictor->PredictForRequest(*convreq_, &segments);

  EXPECT_TRUE(FindCandidateByValue(segments.conversion_segment(0), "入試"));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "入試センター"));
}

// We are not sure what should we suggest after the end of sentence for now.
// However, we decided to show zero query suggestion rather than stopping
// zero query completely. Users may be confused if they cannot see suggestion
// window only after the certain conditions.
// TODO(toshiyuki): Show useful zero query suggestions after EOS.
TEST_F(DictionaryPredictorTest, DISABLED_MobileZeroQuerySuggestionAfterEOS) {
  testing::MockDataManager data_manager;

  unique_ptr<MockDataAndPredictor> data_and_predictor(
      new MockDataAndPredictor());
  data_and_predictor->Init(CreateSystemDictionaryFromDataManager(data_manager),
                           CreateSuffixDictionaryFromDataManager(data_manager));

  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  const POSMatcher &pos_matcher = data_and_predictor->pos_matcher();

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

  for (size_t i = 0; i < arraysize(kTestcases); ++i) {
    const TestCase &test_case = kTestcases[i];

    Segments segments;
    MakeSegmentsForPrediction("", &segments);

    Segment *seg = segments.push_front_segment();
    seg->set_segment_type(Segment::HISTORY);
    seg->set_key(test_case.key);
    Segment::Candidate *c = seg->add_candidate();
    c->key = test_case.key;
    c->content_key = test_case.key;
    c->value = test_case.value;
    c->content_value = test_case.value;
    c->rid = test_case.rid;

    predictor->PredictForRequest(*convreq_, &segments);
    const bool candidates_inserted =
        segments.conversion_segment(0).candidates_size() > 0;
    EXPECT_EQ(test_case.expected_result, candidates_inserted);
  }
}

TEST_F(DictionaryPredictorTest, PropagateUserDictionaryAttribute) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const DictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  config_->set_use_dictionary_suggest(true);
  config_->set_use_realtime_conversion(true);

  {
    segments.Clear();
    segments.set_max_prediction_candidates_size(10);
    segments.set_request_type(Segments::SUGGESTION);
    Segment *seg = segments.add_segment();
    seg->set_key("ゆーざー");
    seg->set_segment_type(Segment::FREE);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_,
                                             &segments));
    EXPECT_EQ(1, segments.conversion_segments_size());
    bool find_yuza_candidate = false;
    for (size_t i = 0;
         i < segments.conversion_segment(0).candidates_size();
         ++i) {
      const Segment::Candidate &cand =
          segments.conversion_segment(0).candidate(i);
      if (cand.value == "ユーザー" &&
          (cand.attributes & (Segment::Candidate::NO_VARIANTS_EXPANSION |
                              Segment::Candidate::USER_DICTIONARY))) {
        find_yuza_candidate = true;
      }
    }
    EXPECT_TRUE(find_yuza_candidate);
  }

  {
    segments.Clear();
    segments.set_max_prediction_candidates_size(10);
    segments.set_request_type(Segments::SUGGESTION);
    Segment *seg = segments.add_segment();
    seg->set_key("ゆーざーの");
    seg->set_segment_type(Segment::FREE);
    EXPECT_TRUE(predictor->PredictForRequest(*convreq_,
                                             &segments));
    EXPECT_EQ(1, segments.conversion_segments_size());
    bool find_yuza_candidate = false;
    for (size_t i = 0;
         i < segments.conversion_segment(0).candidates_size();
         ++i) {
      const Segment::Candidate &cand =
          segments.conversion_segment(0).candidate(i);
      if ((cand.value == "ユーザーの") &&
          (cand.attributes & (Segment::Candidate::NO_VARIANTS_EXPANSION |
                              Segment::Candidate::USER_DICTIONARY))) {
        find_yuza_candidate = true;
      }
    }
    EXPECT_TRUE(find_yuza_candidate);
  }
}

TEST_F(DictionaryPredictorTest, SetDescription) {
  {
    string description;
    DictionaryPredictor::SetDescription(
        TestableDictionaryPredictor::TYPING_CORRECTION, 0, &description);
    EXPECT_EQ("補正", description);

    description.clear();
    DictionaryPredictor::SetDescription(
        0, Segment::Candidate::AUTO_PARTIAL_SUGGESTION, &description);
    EXPECT_EQ("部分", description);
  }
}

TEST_F(DictionaryPredictorTest, SetDebugDescription) {
  {
    string description;
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::UNIGRAM |
        TestableDictionaryPredictor::ENGLISH;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("UE", description);
  }
  {
    string description = "description";
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::REALTIME |
        TestableDictionaryPredictor::BIGRAM;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("description BR", description);
  }
  {
    string description;
    const TestableDictionaryPredictor::PredictionTypes types =
        TestableDictionaryPredictor::BIGRAM |
        TestableDictionaryPredictor::REALTIME |
        TestableDictionaryPredictor::SUFFIX;
    DictionaryPredictor::SetDebugDescription(types, &description);
    EXPECT_EQ("BRS", description);
  }
}

TEST_F(DictionaryPredictorTest, PropagateRealtimeConversionBoundary) {
  testing::MockDataManager data_manager;
  unique_ptr<const DictionaryInterface> dictionary(new DictionaryMock);
  unique_ptr<ConverterInterface> converter(new ConverterMock);
  unique_ptr<ImmutableConverterInterface> immutable_converter(
      new ImmutableConverterMock);
  unique_ptr<const DictionaryInterface> suffix_dictionary(
      CreateSuffixDictionaryFromDataManager(data_manager));
  unique_ptr<const Connector> connector(
      Connector::CreateFromDataManager(data_manager));
  unique_ptr<const Segmenter> segmenter(
      Segmenter::CreateFromDataManager(data_manager));
  unique_ptr<const SuggestionFilter> suggestion_filter(
      CreateSuggestionFilter(data_manager));
  const dictionary::POSMatcher pos_matcher(data_manager.GetPOSMatcherData());
  unique_ptr<TestableDictionaryPredictor> predictor(
      new TestableDictionaryPredictor(data_manager,
                                      converter.get(),
                                      immutable_converter.get(),
                                      dictionary.get(),
                                      suffix_dictionary.get(),
                                      connector.get(),
                                      segmenter.get(),
                                      &pos_matcher,
                                      suggestion_filter.get()));
  Segments segments;
  const char kKey[] =
      "わたしのなまえはなかのです";
  MakeSegmentsForSuggestion(kKey, &segments);

  std::vector<TestableDictionaryPredictor::Result> results;
  predictor->AggregateRealtimeConversion(
      TestableDictionaryPredictor::REALTIME, *convreq_,
      &segments, &results);

  // mock results
  EXPECT_EQ(1, results.size());
  predictor->AddPredictionToCandidates(*convreq_,
                                       &segments, &results);
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(1, segments.conversion_segment(0).candidates_size());
  const Segment::Candidate &cand = segments.conversion_segment(0).candidate(0);
  EXPECT_EQ("わたしのなまえはなかのです", cand.key);
  EXPECT_EQ("私の名前は中野です", cand.value);
  EXPECT_EQ(3, cand.inner_segment_boundary.size());
}

TEST_F(DictionaryPredictorTest, PropagateResultCosts) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  std::vector<TestableDictionaryPredictor::Result> results;
  const int kTestSize = 20;
  for (size_t i = 0; i < kTestSize; ++i) {
    results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
    TestableDictionaryPredictor::Result *result = &results.back();
    result->key = string(1, 'a' + i);
    result->value = string(1, 'A' + i);
    result->wcost = i;
    result->cost = i + 1000;
    result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::REALTIME,
                                       Token::NONE);
  }
  std::random_device rd;
  std::mt19937 urbg(rd());
  std::shuffle(results.begin(), results.end(), urbg);

  Segments segments;
  MakeSegmentsForSuggestion("test", &segments);
  segments.set_max_prediction_candidates_size(kTestSize);

  predictor->AddPredictionToCandidates(*convreq_,
                                       &segments, &results);

  EXPECT_EQ(1, segments.conversion_segments_size());
  ASSERT_EQ(kTestSize, segments.conversion_segment(0).candidates_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(i + 1000, segment.candidate(i).cost);
  }
}

TEST_F(DictionaryPredictorTest, PredictNCandidates) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  std::vector<TestableDictionaryPredictor::Result> results;
  const int kTotalCandidateSize = 100;
  const int kLowCostCandidateSize = 5;
  for (size_t i = 0; i < kTotalCandidateSize; ++i) {
    results.push_back(TestableDictionaryPredictor::MakeEmptyResult());
    TestableDictionaryPredictor::Result *result = &results.back();
    result->key = string(1, 'a' + i);
    result->value = string(1, 'A' + i);
    result->wcost = i;
    result->SetTypesAndTokenAttributes(TestableDictionaryPredictor::REALTIME,
                                       Token::NONE);
    if (i < kLowCostCandidateSize) {
      result->cost = i + 1000;
    } else {
      result->cost = i + kInfinity;
    }
  }
  std::random_shuffle(results.begin(), results.end());

  Segments segments;
  MakeSegmentsForSuggestion("test", &segments);
  segments.set_max_prediction_candidates_size(kLowCostCandidateSize + 1);

  predictor->AddPredictionToCandidates(*convreq_,
                                       &segments, &results);

  ASSERT_EQ(1, segments.conversion_segments_size());
  ASSERT_EQ(kLowCostCandidateSize,
            segments.conversion_segment(0).candidates_size());
  const Segment &segment = segments.conversion_segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    EXPECT_EQ(i + 1000, segment.candidate(i).cost);
  }
}

TEST_F(DictionaryPredictorTest, SuggestFilteredwordForExactMatchOnMobile) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  // turn on mobile mode
  commands::RequestForUnitTest::FillMobileRequest(request_.get());

  Segments segments;
  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by realtime conversion.
  // Here, we want to confirm the behavior including unigram prediction.
  MakeSegmentsForSuggestion("ふぃるたーたいしょう", &segments);

  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
  EXPECT_TRUE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター大将"));

  // However, filtered word should not be the top.
  EXPECT_EQ("フィルター大将",
            segments.conversion_segment(0).candidate(0).value);

  // Should not be there for non-exact suggestion.
  MakeSegmentsForSuggestion("ふぃるたーたいし", &segments);
  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
}

TEST_F(DictionaryPredictorTest, SuppressFilteredwordForExactMatch) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  const TestableDictionaryPredictor *predictor =
      data_and_predictor->dictionary_predictor();

  Segments segments;
  // Note: The suggestion filter entry "フィルター" for test is not
  // appropriate here, as Katakana entry will be added by realtime conversion.
  // Here, we want to confirm the behavior including unigram prediction.
  MakeSegmentsForSuggestion("ふぃるたーたいしょう", &segments);

  EXPECT_TRUE(predictor->PredictForRequest(*convreq_, &segments));
  EXPECT_FALSE(
      FindCandidateByValue(segments.conversion_segment(0), "フィルター対象"));
}

namespace {

const char kTestTokenArray[] =
    // {"あ", "", ZERO_QUERY_EMOJI, EMOJI_DOCOMO | EMOJI_SOFTBANK, 0xfeb04}
    "\x04\x00\x00\x00"
    "\x00\x00\x00\x00"
    "\x03\x00"
    "\x06\x00"
    "\x04\xeb\x0f\x00"
    // {"あ", "❕", ZERO_QUERY_EMOJI, EMOJI_UNICODE, 0xfeb0b},
    "\x04\x00\x00\x00"
    "\x02\x00\x00\x00"
    "\x03\x00"
    "\x01\x00"
    "\x0b\xeb\x0f\x00"
    // {"あ", "❣", ZERO_QUERY_NONE, EMOJI_NONE, 0x00},
    "\x04\x00\x00\x00"
    "\x03\x00\x00\x00"
    "\x00\x00"
    "\x00\x00"
    "\x00\x00\x00\x00"
    // {"ああ", "( •̀ㅁ•́;)", ZERO_QUERY_EMOTICON, EMOJI_NONE, 0x00}
    "\x05\x00\x00\x00"
    "\x01\x00\x00\x00"
    "\x02\x00"
    "\x00\x00"
    "\x00\x00\x00\x00";

const char *kTestStrings[] = {
    "", "( •̀ㅁ•́;)", "❕", "❣", "あ", "ああ",
};

struct TestEntry {
  int32 available_emoji_carrier;
  string key;
  bool expected_result;
  // candidate value and ZeroQueryType.
  std::vector<string> expected_candidates;
  std::vector<int32> expected_types;

  string DebugString() const {
    string candidates;
    Util::JoinStrings(expected_candidates, ", ", &candidates);
    string types;
    for (size_t i = 0; i < expected_types.size(); ++i) {
      if (i != 0) {
        types.append(", ");
      }
      types.append(Util::StringPrintf("%d", types[i]));
    }
    return Util::StringPrintf(
        "available_emoji_carrier: %d\n"
        "key: %s\n"
        "expected_result: %d\n"
        "expected_candidates: %s\n"
        "expected_types: %s",
        available_emoji_carrier,
        key.c_str(),
        expected_result,
        candidates.c_str(),
        types.c_str());
  }
};

}  // namespace

TEST_F(DictionaryPredictorTest, GetZeroQueryCandidates) {
  // Create test zero query data.
  std::unique_ptr<uint32[]> string_data_buffer;
  ZeroQueryDict zero_query_dict;
  {
    // kTestTokenArray contains a trailing '\0', so create a StringPiece that
    // excludes it by subtracting 1.
    const StringPiece token_array_data(kTestTokenArray,
                                       arraysize(kTestTokenArray) - 1);
    std::vector<StringPiece> strs;
    for (const char *str : kTestStrings) {
      strs.push_back(str);
    }
    const StringPiece string_array_data =
        SerializedStringArray::SerializeToBuffer(strs, &string_data_buffer);
    zero_query_dict.Init(token_array_data, string_array_data);
  }

  std::vector<TestEntry> test_entries;
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "a";
    entry.expected_result = false;
    entry.expected_candidates.clear();
    entry.expected_types.clear();
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "ん";
    entry.expected_result = false;
    entry.expected_candidates.clear();
    entry.expected_types.clear();
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "ああ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("( •̀ㅁ•́;)");
    entry.expected_types.push_back(ZERO_QUERY_EMOTICON);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = 0;
    entry.key = "あ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = commands::Request::UNICODE_EMOJI;
    entry.key = "あ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("❕");
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = commands::Request::DOCOMO_EMOJI;
    entry.key = "あ";
    entry.expected_result = true;
    string candidate;
    Util::UCS4ToUTF8(0xfeb04, &candidate);  // exclamation
    entry.expected_candidates.push_back(candidate);
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier = commands::Request::KDDI_EMOJI;
    entry.key = "あ";
    entry.expected_result = true;
    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }
  {
    TestEntry entry;
    entry.available_emoji_carrier =
        (commands::Request::DOCOMO_EMOJI | commands::Request::SOFTBANK_EMOJI |
         commands::Request::UNICODE_EMOJI);
    entry.key = "あ";
    entry.expected_result = true;
    string candidate;
    Util::UCS4ToUTF8(0xfeb04, &candidate);  // exclamation
    entry.expected_candidates.push_back(candidate);
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❕");
    entry.expected_types.push_back(ZERO_QUERY_EMOJI);

    entry.expected_candidates.push_back("❣");
    entry.expected_types.push_back(ZERO_QUERY_NONE);
    test_entries.push_back(entry);
  }

  for (size_t i = 0; i < test_entries.size(); ++i) {
    const TestEntry &test_entry = test_entries[i];
    ASSERT_EQ(test_entry.expected_candidates.size(),
              test_entry.expected_types.size());

    commands::Request client_request;
    client_request.set_available_emoji_carrier(
        test_entry.available_emoji_carrier);
    composer::Table table;
    const config::Config &config = config::ConfigHandler::DefaultConfig();
    composer::Composer composer(&table, &client_request, &config);
    const ConversionRequest request(&composer, &client_request, &config);

    std::vector<DictionaryPredictor::ZeroQueryResult> actual_candidates;
    const bool actual_result =
        DictionaryPredictor::GetZeroQueryCandidatesForKey(
            request, test_entry.key, zero_query_dict, &actual_candidates);
    EXPECT_EQ(test_entry.expected_result, actual_result)
        << test_entry.DebugString();
    for (size_t j = 0; j < test_entry.expected_candidates.size(); ++j) {
      EXPECT_EQ(test_entry.expected_candidates[j], actual_candidates[j].first)
          << "Failed at " << j << " : " << test_entry.DebugString();
      EXPECT_EQ(test_entry.expected_types[j], actual_candidates[j].second)
          << "Failed at " << j << " : " << test_entry.DebugString();
    }
  }
}

namespace {
void SetSegmentForCommit(const string &candidate_value,
                         int candidate_source_info, Segments *segments) {
  segments->Clear();
  Segment *segment = segments->add_segment();
  segment->set_key("");
  segment->set_segment_type(Segment::FIXED_VALUE);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->key = candidate_value;
  candidate->content_key = candidate_value;
  candidate->value = candidate_value;
  candidate->content_value = candidate_value;
  candidate->source_info = candidate_source_info;
}
}  // namespace

TEST_F(DictionaryPredictorTest, UsageStats) {
  unique_ptr<MockDataAndPredictor> data_and_predictor(
      CreateDictionaryPredictorWithMockData());
  DictionaryPredictor *predictor =
      data_and_predictor->mutable_dictionary_predictor();

  Segments segments;
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNone", 0);
  SetSegmentForCommit(
      "★", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NONE, &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNone", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNumberSuffix", 0);
  SetSegmentForCommit(
      "個", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_NUMBER_SUFFIX,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeNumberSuffix", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoticon", 0);
  SetSegmentForCommit(
      "＼(^o^)／", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOTICON,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoticon", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoji", 0);
  SetSegmentForCommit("❕",
                      Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_EMOJI,
                      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeEmoji", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeBigram", 0);
  SetSegmentForCommit(
      "ヒルズ", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_BIGRAM,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeBigram", 1);

  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeSuffix", 0);
  SetSegmentForCommit(
      "が", Segment::Candidate::DICTIONARY_PREDICTOR_ZERO_QUERY_SUFFIX,
      &segments);
  predictor->Finish(*convreq_, &segments);
  EXPECT_COUNT_STATS("CommitDictionaryPredictorZeroQueryTypeSuffix", 1);
}

}  // namespace mozc
