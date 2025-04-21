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

#include "composer/internal/typing_corrector.h"

#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/singleton.h"
#include "composer/internal/typing_model.h"
#include "composer/table.h"
#include "composer/type_corrected_query.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_data_manager.h"
#include "protocol/commands.pb.h"
#include "session/request_test_util.h"
#include "testing/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace composer {

using ::mozc::commands::Request;
using ::mozc::config::Config;
using ::mozc::config::ConfigHandler;

// Embedded cost for testing purpose.
class CostTableForTest {
 public:
  typedef std::map<absl::string_view, ProbableKeyEvents> CorrectionTable;

  CostTableForTest() {
    {
      ProbableKeyEvents *v = &table_["a"];
      AddProbableKeyEvent('a', 0.99, v);
      AddProbableKeyEvent('q', 0.003, v);
      AddProbableKeyEvent('w', 0.003, v);
      AddProbableKeyEvent('s', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["b"];
      AddProbableKeyEvent('b', 0.99, v);
      AddProbableKeyEvent('v', 0.0025, v);
      AddProbableKeyEvent('h', 0.0025, v);
      AddProbableKeyEvent('j', 0.0025, v);
      AddProbableKeyEvent('n', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["c"];
      AddProbableKeyEvent('c', 0.99, v);
      AddProbableKeyEvent('x', 0.0025, v);
      AddProbableKeyEvent('f', 0.0025, v);
      AddProbableKeyEvent('g', 0.0025, v);
      AddProbableKeyEvent('v', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["d"];
      AddProbableKeyEvent('d', 0.99, v);
      AddProbableKeyEvent('s', 0.002, v);
      AddProbableKeyEvent('e', 0.002, v);
      AddProbableKeyEvent('f', 0.002, v);
      AddProbableKeyEvent('x', 0.002, v);
      AddProbableKeyEvent('z', 0.002, v);
    }
    {
      ProbableKeyEvents *v = &table_["e"];
      AddProbableKeyEvent('e', 0.99, v);
      AddProbableKeyEvent('w', 0.003, v);
      AddProbableKeyEvent('d', 0.003, v);
      AddProbableKeyEvent('r', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["f"];
      AddProbableKeyEvent('f', 0.99, v);
      AddProbableKeyEvent('d', 0.002, v);
      AddProbableKeyEvent('r', 0.002, v);
      AddProbableKeyEvent('g', 0.002, v);
      AddProbableKeyEvent('c', 0.002, v);
      AddProbableKeyEvent('x', 0.002, v);
    }
    {
      ProbableKeyEvents *v = &table_["g"];
      AddProbableKeyEvent('g', 0.99, v);
      AddProbableKeyEvent('f', 0.002, v);
      AddProbableKeyEvent('t', 0.002, v);
      AddProbableKeyEvent('h', 0.002, v);
      AddProbableKeyEvent('v', 0.002, v);
      AddProbableKeyEvent('c', 0.002, v);
    }
    {
      ProbableKeyEvents *v = &table_["h"];
      AddProbableKeyEvent('h', 0.99, v);
      AddProbableKeyEvent('g', 0.002, v);
      AddProbableKeyEvent('y', 0.002, v);
      AddProbableKeyEvent('j', 0.002, v);
      AddProbableKeyEvent('b', 0.002, v);
      AddProbableKeyEvent('v', 0.002, v);
    }
    {
      ProbableKeyEvents *v = &table_["i"];
      AddProbableKeyEvent('i', 0.99, v);
      AddProbableKeyEvent('u', 0.003, v);
      AddProbableKeyEvent('k', 0.003, v);
      AddProbableKeyEvent('o', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["j"];
      AddProbableKeyEvent('j', 0.99, v);
      AddProbableKeyEvent('h', 0.002, v);
      AddProbableKeyEvent('k', 0.002, v);
      AddProbableKeyEvent('u', 0.002, v);
      AddProbableKeyEvent('n', 0.002, v);
      AddProbableKeyEvent('b', 0.002, v);
    }
    {
      ProbableKeyEvents *v = &table_["k"];
      AddProbableKeyEvent('k', 0.99, v);
      AddProbableKeyEvent('j', 0.002, v);
      AddProbableKeyEvent('i', 0.002, v);
      AddProbableKeyEvent('l', 0.002, v);
      AddProbableKeyEvent('m', 0.002, v);
      AddProbableKeyEvent('n', 0.002, v);
    }
    {
      ProbableKeyEvents *v = &table_["l"];
      AddProbableKeyEvent('l', 0.99, v);
      AddProbableKeyEvent('k', 0.0025, v);
      AddProbableKeyEvent('-', 0.0025, v);
      AddProbableKeyEvent('p', 0.0025, v);
      AddProbableKeyEvent('o', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["m"];
      AddProbableKeyEvent('m', 0.99, v);
      AddProbableKeyEvent('n', 0.003, v);
      AddProbableKeyEvent('k', 0.003, v);
      AddProbableKeyEvent('l', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["n"];
      AddProbableKeyEvent('n', 0.99, v);
      AddProbableKeyEvent('b', 0.0025, v);
      AddProbableKeyEvent('m', 0.0025, v);
      AddProbableKeyEvent('j', 0.0025, v);
      AddProbableKeyEvent('k', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["o"];
      AddProbableKeyEvent('o', 0.99, v);
      AddProbableKeyEvent('i', 0.0025, v);
      AddProbableKeyEvent('k', 0.0025, v);
      AddProbableKeyEvent('l', 0.0025, v);
      AddProbableKeyEvent('p', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["p"];
      AddProbableKeyEvent('p', 0.99, v);
      AddProbableKeyEvent('o', 0.003, v);
      AddProbableKeyEvent('l', 0.003, v);
      AddProbableKeyEvent('-', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["q"];
      AddProbableKeyEvent('q', 0.99, v);
      AddProbableKeyEvent('w', 0.003, v);
      AddProbableKeyEvent('a', 0.003, v);
      AddProbableKeyEvent('s', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["r"];
      AddProbableKeyEvent('r', 0.99, v);
      AddProbableKeyEvent('t', 0.003, v);
      AddProbableKeyEvent('f', 0.003, v);
      AddProbableKeyEvent('e', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["s"];
      AddProbableKeyEvent('s', 0.99, v);
      AddProbableKeyEvent('a', 0.0025, v);
      AddProbableKeyEvent('d', 0.0025, v);
      AddProbableKeyEvent('w', 0.0025, v);
      AddProbableKeyEvent('z', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["t"];
      AddProbableKeyEvent('t', 0.99, v);
      AddProbableKeyEvent('r', 0.003, v);
      AddProbableKeyEvent('y', 0.003, v);
      AddProbableKeyEvent('g', 0.003, v);
    }
    {
      ProbableKeyEvents *v = &table_["u"];
      AddProbableKeyEvent('u', 0.99, v);
      AddProbableKeyEvent('y', 0.003, v);
      AddProbableKeyEvent('i', 0.003, v);
      AddProbableKeyEvent('j', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["v"];
      AddProbableKeyEvent('v', 0.99, v);
      AddProbableKeyEvent('c', 0.0025, v);
      AddProbableKeyEvent('g', 0.0025, v);
      AddProbableKeyEvent('h', 0.0025, v);
      AddProbableKeyEvent('b', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["w"];
      AddProbableKeyEvent('w', 0.99, v);
      AddProbableKeyEvent('q', 0.0025, v);
      AddProbableKeyEvent('e', 0.0025, v);
      AddProbableKeyEvent('a', 0.0025, v);
      AddProbableKeyEvent('s', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["x"];
      AddProbableKeyEvent('x', 0.99, v);
      AddProbableKeyEvent('z', 0.0025, v);
      AddProbableKeyEvent('d', 0.0025, v);
      AddProbableKeyEvent('f', 0.0025, v);
      AddProbableKeyEvent('c', 0.0025, v);
    }
    {
      ProbableKeyEvents *v = &table_["y"];
      AddProbableKeyEvent('y', 0.99, v);
      AddProbableKeyEvent('t', 0.003, v);
      AddProbableKeyEvent('h', 0.003, v);
      AddProbableKeyEvent('u', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["z"];
      AddProbableKeyEvent('z', 0.99, v);
      AddProbableKeyEvent('s', 0.003, v);
      AddProbableKeyEvent('d', 0.003, v);
      AddProbableKeyEvent('x', 0.004, v);
    }
    {
      ProbableKeyEvents *v = &table_["-"];
      AddProbableKeyEvent('-', 0.99, v);
      AddProbableKeyEvent('p', 0.003, v);
      AddProbableKeyEvent('o', 0.003, v);
      AddProbableKeyEvent('l', 0.004, v);
    }
  }

  CostTableForTest(const CostTableForTest &) = delete;
  CostTableForTest &operator=(const CostTableForTest &) = delete;

  void InsertCharacter(TypingCorrector *corrector,
                       absl::string_view key) const {
    CompositionInput input;
    input.InitFromRaw({key.data(), key.size()}, /*is_new_input=*/false);
    input.set_probable_key_events(table_.find(key)->second);
    corrector->InsertCharacter(input);
  }

 private:
  CorrectionTable table_;

  void AddProbableKeyEvent(int key_code, double probability,
                           ProbableKeyEvents *probable_key_events) {
    ProbableKeyEvent *event = probable_key_events->Add();
    event->set_key_code(key_code);
    event->set_probability(probability);
  }
};

class TypingCorrectorTest : public ::testing::Test {
 protected:
  TypingCorrectorTest() = default;

  void SetUp() override {
    ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_typing_correction(true);
    request_ = std::make_unique<Request>();
    request_->set_special_romanji_table(
        commands::Request::QWERTY_MOBILE_TO_HIRAGANA);
    qwerty_table_.InitializeWithRequestAndConfig(*request_, config_,
                                                 mock_data_manager_);
    qwerty_table_.typing_model_ = TypingModel::CreateTypingModel(
        commands::Request::QWERTY_MOBILE_TO_HIRAGANA, mock_data_manager_);
  }

  void InsertOneByOne(const char *keys, TypingCorrector *corrector) {
    for (const char *key = keys; *key != '\0'; ++key) {
      Singleton<CostTableForTest>::get()->InsertCharacter(
          corrector, absl::string_view(key, 1));
    }
  }

  bool FindKey(const std::vector<TypeCorrectedQuery> &queries,
               const absl::string_view key) {
    for (size_t i = 0; i < queries.size(); ++i) {
      const std::set<std::string> &expanded = queries[i].expanded;
      if (expanded.empty() && queries[i].base == key) {
        return true;
      }
      for (std::set<std::string>::const_iterator itr = expanded.begin();
           itr != expanded.end(); ++itr) {
        if (queries[i].base + *itr == key) {
          return true;
        }
      }
    }
    return false;
  }

  void ExpectTypingCorrectorEqual(const TypingCorrector &l,
                                  const TypingCorrector &r) {
    EXPECT_EQ(l.available_, r.available_);
    EXPECT_EQ(l.table_, r.table_);
    EXPECT_EQ(l.config_, r.config_);
    EXPECT_EQ(l.max_correction_query_candidates_,
              r.max_correction_query_candidates_);
    EXPECT_EQ(l.max_correction_query_results_, r.max_correction_query_results_);
    EXPECT_EQ(l.top_n_.size(), r.top_n_.size());
    for (size_t i = 0; i < l.top_n_.size(); ++i) {
      EXPECT_EQ(l.top_n_[i], l.top_n_[i]);
    }
  }

  const testing::MockDataManager mock_data_manager_;
  std::unique_ptr<Request> request_;
  Config config_;
  Table qwerty_table_;
};

TEST_F(TypingCorrectorTest, TypingCorrection) {
  constexpr int kCorrectedQueryCandidates = 1000;
  constexpr int kCorrectedQueryResults = 1000;
  TypingCorrector corrector(request_.get(), &qwerty_table_,
                            kCorrectedQueryCandidates, kCorrectedQueryResults);
  corrector.SetConfig(&config_);
  ASSERT_TRUE(corrector.IsAvailable());

  struct {
    const char *keys;
    const char *correction;
    const char *exact_composition;
  } kTestCases[] = {
      {"phayou", "おはよう", "ｐはよう"},
      {"orukaresama", "おつかれさま", "おるかれさま"},
      {"gu-huru", "ぐーぐる", "ぐーふる"},
      {"bihongo", "にほんご", "びほんご"},
      {"yajiniku", "やきにく", "やじにく"},
      {"so-natsu", "どーなつ", "そーなつ"},
      // "おはよう" can be generated from raw key so
      // it shouldn't be in correction candidates.
      {"ohayou", nullptr, "おはよう"},
      // "syamoji" -> nullptr, "しゃもじ"
      // A query which can be composed from raw input
      // shouldn't be in correction candidates.
      // This is more complex pattern than "おはよう" test case.
      // "おはよう" case can be processed correctly by comparing
      // raw input and corrected query.
      // But this case "syamoji" (raw input) and "shamoji" (corrected input)
      // are different but their query is identical ("しゃもじ").
      // Thus we have to also check not only raw/corrected input but also
      // raw/corrected queries.
      {"syamozi", nullptr, "しゃもじ"},
      // "kaish" -> nullptr, "かいしゃ"
      // Pending input is expanded into possible queries for
      // kana-modifier-insensitive-conversion (a.k.a かつこう変換).
      // In this case "kaish" is expanded into "かいしゃ", "かいしゅ"
      // and so on.
      // Typing corrected input "kaisy" is also expanded into
      // "かいしゃ", "かいしゅ" and so on but they are duplicate
      // of expanded queries from "kaish".
      // Thus they shouldn't be in corrected candidates.
      {"kaish", nullptr, "かいしゃ"},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    SCOPED_TRACE(std::string("key: ") + kTestCases[i].keys);
    InsertOneByOne(kTestCases[i].keys, &corrector);
    std::vector<TypeCorrectedQuery> queries;
    corrector.GetQueriesForPrediction(&queries);
    // Number of queries can be equal to kCorrectedQueries.
    EXPECT_GE(kCorrectedQueryResults, queries.size());
    for (std::vector<TypeCorrectedQuery>::iterator it = queries.begin();
         it != queries.end(); ++it) {
      // Empty TypeCorrectedQuery is unexpected.
      EXPECT_FALSE(it->base.empty() && it->expanded.empty());
    }
    if (kTestCases[i].correction) {
      EXPECT_TRUE(FindKey(queries, kTestCases[i].correction))
          << kTestCases[i].correction << " isn't contained";
    }
    EXPECT_FALSE(FindKey(queries, kTestCases[i].exact_composition))
        << kTestCases[i].exact_composition << " is contained unexpectedly";
    corrector.Reset();
  }
}

TEST_F(TypingCorrectorTest, SkipFirstProbKeys) {
  constexpr int kCorrectedQueryCandidates = 1000;
  constexpr int kCorrectedQueryResults = 1000;
  TypingCorrector corrector(request_.get(), &qwerty_table_,
                            kCorrectedQueryCandidates, kCorrectedQueryResults);
  corrector.SetConfig(&config_);
  ASSERT_TRUE(corrector.IsAvailable());

  struct {
    const absl::string_view keys;
    const absl::string_view correction;
    const bool expected_default;
    const bool expected_skip_first_prob_keys;
  } kTestCases[] = {
      {"phayou", "おはよう", true, false},
      {"orukaresama", "おつかれさま", true, true},
      {"gu-huru", "ぐーぐる", true, true},
      {"bihongo", "にほんご", true, false},
      {"yajiniku", "やきにく", true, true},
      {"so-natsu", "どーなつ", true, false},
      // No correction
      {"ohayou", "おはよう", false, false},
      {"syamozi", "しゃもじ", false, false},
      {"kaish", "かいしゃ", false, false},
  };

  auto contains_correction = [this](absl::string_view keys,
                                    absl::string_view correction,
                                    TypingCorrector *corrector) {
    SCOPED_TRACE(absl::StrCat("key: ", keys));
    InsertOneByOne(keys.data(), corrector);
    std::vector<TypeCorrectedQuery> queries;
    corrector->GetQueriesForPrediction(&queries);
    return FindKey(queries, correction);
  };

  Request default_request, mobile_request;
  commands::RequestForUnitTest::FillMobileRequest(&mobile_request);
  for (const auto &test_case : kTestCases) {
    corrector.Reset();
    corrector.SetRequest(&default_request);
    EXPECT_EQ(
        contains_correction(test_case.keys, test_case.correction, &corrector),
        test_case.expected_default);

    corrector.Reset();
    corrector.SetRequest(&mobile_request);
    EXPECT_EQ(
        contains_correction(test_case.keys, test_case.correction, &corrector),
        test_case.expected_skip_first_prob_keys);
  }
}

TEST_F(TypingCorrectorTest, Invalidate) {
  const CostTableForTest *table = Singleton<CostTableForTest>::get();

  TypingCorrector corrector(request_.get(), &qwerty_table_, 30, 30);
  corrector.SetConfig(&config_);

  EXPECT_TRUE(corrector.IsAvailable());
  table->InsertCharacter(&corrector, "p");
  table->InsertCharacter(&corrector, "h");
  table->InsertCharacter(&corrector, "a");

  corrector.Invalidate();
  EXPECT_FALSE(corrector.IsAvailable());

  table->InsertCharacter(&corrector, "y");
  table->InsertCharacter(&corrector, "o");
  table->InsertCharacter(&corrector, "u");

  std::vector<TypeCorrectedQuery> queries;
  corrector.GetQueriesForPrediction(&queries);
  EXPECT_TRUE(queries.empty());
}

TEST_F(TypingCorrectorTest, Copy) {
  TypingCorrector corrector(request_.get(), &qwerty_table_, 30, 30);
  corrector.SetConfig(&config_);
  InsertOneByOne("phayou", &corrector);

  const TypingCorrector corrector2(corrector);
  ExpectTypingCorrectorEqual(corrector, corrector2);

  TypingCorrector corrector3(request_.get(), nullptr, 1000, 1000);
  corrector3 = corrector;
  ExpectTypingCorrectorEqual(corrector, corrector3);
}

TEST_F(TypingCorrectorTest, SupportNonAscii) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  config.set_use_typing_correction(true);
  commands::RequestForUnitTest::FillMobileRequest(request_.get());
  request_->set_special_romanji_table(commands::Request::FLICK_TO_HIRAGANA);

  Table table;
  testing::MockDataManager data_manager;
  table.InitializeWithRequestAndConfig(*request_, config, data_manager);

  TypingCorrector corrector(request_.get(), &table, 30, 30);
  corrector.SetConfig(&config_);

  EXPECT_TRUE(corrector.IsAvailable());

  ProbableKeyEvents events;
  const struct ProbableKeyData {
    const std::string str;
    const double prob;
  } key_data[] = {
      {"め", 0.98},
      {"む", 0.15},
      {"も", 0.01},
  };
  for (const ProbableKeyData &data : key_data) {
    ProbableKeyEvent *event = events.Add();
    event->set_key_code(Util::Utf8ToUcs4(data.str));
    event->set_probability(data.prob);
  }
  CompositionInput input;
  input.InitFromRaw("め", /*is_new_input=*/false);
  input.set_probable_key_events(std::move(events));
  corrector.InsertCharacter(input);
  std::vector<TypeCorrectedQuery> queries;
  // No model cost should be looked up.
  corrector.GetQueriesForPrediction(&queries);
  EXPECT_EQ(queries.size(), 0);
}

TEST_F(TypingCorrectorTest, Cost) {
  Table table;

  // Creates a typing model which always returns cost 0.
  constexpr absl::string_view chars = "ab^";
  // TypingCorrector looks up tri-gram.
  // The maximum index will be (len(`chars`) + 1)^3 - 1.
  const uint8_t costs[4 * 4 * 4] = {0};
  const int32_t mapping_table[] = {0};
  table.SetTypingModelForTesting(std::make_unique<TypingModel>(
      chars.data(), chars.size(), costs, std::size(costs), mapping_table));

  auto create_input = []() -> CompositionInput {
    CompositionInput input;
    input.InitFromRaw({"a", 1}, true);
    ProbableKeyEvents probable_key_events;
    ProbableKeyEvent *event = probable_key_events.Add();
    event->set_key_code('a');
    event->set_probability(0.75);
    event = probable_key_events.Add();
    event->set_key_code('b');
    event->set_probability(0.25);
    input.set_probable_key_events(probable_key_events);
    return input;
  };

  {
    TypingCorrector corrector(request_.get(), &table, 30, 30);
    corrector.SetConfig(&config_);
    corrector.InsertCharacter(create_input());

    std::vector<TypeCorrectedQuery> queries;
    corrector.GetQueriesForPrediction(&queries);
    ASSERT_EQ(queries.size(), 1);
    EXPECT_EQ(queries[0].base, "b");
    EXPECT_NEAR(queries[0].cost, -500 * log(0.25), 2);
  }

  {
    TypingCorrector corrector(request_.get(), &table, 30, 30);
    Request request;
    request.mutable_decoder_experiment_params()
        ->set_use_typing_correction_diff_cost(true);
    corrector.SetRequest(&request);
    corrector.SetConfig(&config_);
    corrector.InsertCharacter(create_input());

    std::vector<TypeCorrectedQuery> queries;
    corrector.GetQueriesForPrediction(&queries);
    ASSERT_EQ(queries.size(), 1);
    EXPECT_EQ(queries[0].base, "b");
    EXPECT_NEAR(queries[0].cost, -500 * log(0.25 / 0.75), 2);
  }
}

TEST_F(TypingCorrectorTest, Asis) {
  Table table;
  request_->set_special_romanji_table(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA);
  table.InitializeWithRequestAndConfig(*request_, config_, mock_data_manager_);
  table.SetTypingModelForTesting(TypingModel::CreateTypingModel(
      commands::Request::TWELVE_KEYS_TO_HIRAGANA, mock_data_manager_));

  TypingCorrector corrector(request_.get(), &table, 30, 30);
  corrector.SetConfig(&config_);
  ASSERT_TRUE(corrector.IsAvailable());
  {
    CompositionInput input;
    input.InitFromRaw({"4", 1}, true);  // "た"
    corrector.InsertCharacter(input);
  }
  {
    CompositionInput input;
    input.InitFromRaw({"5", 1}, true);  // "な"
    ProbableKeyEvents probable_key_events;
    ProbableKeyEvent *event = probable_key_events.Add();
    event->set_key_code('5');  // "な"
    event->set_probability(0.75);
    event = probable_key_events.Add();
    event->set_key_code('2');  // "か"
    event->set_probability(0.25);
    input.set_probable_key_events(probable_key_events);
    corrector.InsertCharacter(input);
  }
  {
    CompositionInput input;
    input.InitFromRaw({"2", 1}, true);  // "か"
    corrector.InsertCharacter(input);
  }
  {
    CompositionInput input;
    input.InitFromRaw({"*", 1}, true);  // modifier key
    corrector.InsertCharacter(input);
  }

  std::vector<TypeCorrectedQuery> queries;
  corrector.GetQueriesForPrediction(&queries);
  ASSERT_EQ(queries.size(), 1);
  // raw: "422*" -> たぎ
  ASSERT_EQ(queries[0].base, "た");
  auto contains = [](std::set<std::string> str_set, std::string str) {
    return str_set.find(str) != str_set.end();
  };
  ASSERT_TRUE(contains(queries[0].expanded, "ぎ"));
  ASSERT_TRUE(contains(queries[0].expanded, "き"));
  ASSERT_EQ(queries[0].asis, "たぎ");
}

}  // namespace composer
}  // namespace mozc
