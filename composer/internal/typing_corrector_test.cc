// Copyright 2010-2014, Google Inc.
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

#include <string>
#include <vector>
#include "base/singleton.h"
#include "config/config_handler.h"
#include "composer/table.h"
#include "composer/type_corrected_query.h"
#include "composer/internal/typing_model.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace composer {

namespace {
#include "composer/internal/typing_model_qwerty_mobile-hiragana.h"
}

using mozc::config::Config;
using mozc::config::ConfigHandler;

// Embedded cost for testing purpose.
class CostTableForTest {
 public:
  typedef map<StringPiece, ProbableKeyEvents> CorrectionTable;

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

  void InsertCharacter(TypingCorrector *corrector,
                       const StringPiece& key) const {
    corrector->InsertCharacter(key, table_.find(key)->second);
  }

 private:
  CorrectionTable table_;

  void AddProbableKeyEvent(int key_code, double probability,
                           ProbableKeyEvents *probable_key_events) {
    ProbableKeyEvent *event = probable_key_events->Add();
    event->set_key_code(key_code);
    event->set_probability(probability);
  }

  DISALLOW_COPY_AND_ASSIGN(CostTableForTest);
};

class TypingCorrectorTest : public ::testing::Test {
 protected:
  TypingCorrectorTest() :
      qwerty_typing_model_(
          kKeyCharacters_QwertyMobileHiragana,
          kKeyCharactersSize_QwertyMobileHiragana,
          kCostTable_QwertyMobileHiragana,
          kCostTableSize_QwertyMobileHiragana,
          kCostMappingTable_QwertyMobileHiragana) {
  }

  virtual void SetUp() {
    ConfigHandler::GetConfig(&config_backup_);
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.set_use_typing_correction(true);
    ConfigHandler::SetConfig(config);
    commands::Request request;
    request.set_special_romanji_table(
        commands::Request::QWERTY_MOBILE_TO_HIRAGANA);
    qwerty_table_.InitializeWithRequestAndConfig(request, config);
    qwerty_table_.typing_model_ = &qwerty_typing_model_;
  }

  virtual void TearDown() {
    ConfigHandler::SetConfig(config_backup_);
  }

  void InsertOneByOne(const char *keys, TypingCorrector *corrector) {
    for (const char *key = keys; *key != '\0'; ++key) {
      Singleton<CostTableForTest>::get()->InsertCharacter(corrector,
                                                          StringPiece(key, 1));
    }
  }

  bool FindKey(const vector<TypeCorrectedQuery> &queries,
               const string &key) {
    for (size_t i = 0; i < queries.size(); ++i) {
      const set<string> &expanded = queries[i].expanded;
      if (expanded.empty() && queries[i].base == key) {
        return true;
      }
      for (set<string>::const_iterator itr = expanded.begin();
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
    EXPECT_EQ(l.max_correction_query_candidates_,
              r.max_correction_query_candidates_);
    EXPECT_EQ(l.max_correction_query_results_,
              r.max_correction_query_results_);
    EXPECT_EQ(l.top_n_.size(), r.top_n_.size());
    for (size_t i = 0; i < l.top_n_.size(); ++i) {
      EXPECT_EQ(l.top_n_[i], l.top_n_[i]);
    }
  }

  Config config_backup_;
  Table qwerty_table_;
  TypingModel qwerty_typing_model_;
};

TEST_F(TypingCorrectorTest, TypingCorrection) {
  const int kCorrectedQueryCandidates = 1000;
  const int kCorrectedQueryResults = 1000;
  TypingCorrector corrector(&qwerty_table_,
                            kCorrectedQueryCandidates,
                            kCorrectedQueryResults);
  ASSERT_TRUE(corrector.IsAvailable());

  struct {
    const char *keys;
    const char *correction;
    const char *exact_composition;
  } kTestCases[] = {
    // "phayou" -> "おはよう", "ｐはよう"
    { "phayou",
      "\xE3\x81\x8A\xE3\x81\xAF\xE3\x82\x88\xE3\x81\x86",
      "\xEF\xBD\x90\xE3\x81\xAF\xE3\x82\x88\xE3\x81\x86"},
    // "orukaresama" -> "おつかれさま", "おるかれさま"
    { "orukaresama",
      "\xE3\x81\x8A\xE3\x81\xA4\xE3\x81\x8B"
          "\xE3\x82\x8C\xE3\x81\x95\xE3\x81\xBE",
      "\xE3\x81\x8A\xE3\x82\x8B\xE3\x81\x8B"
          "\xE3\x82\x8C\xE3\x81\x95\xE3\x81\xBE"},
    // "gu-huru" -> "ぐーぐる", "ぐーふる"
    { "gu-huru",
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B",
      "\xE3\x81\x90\xE3\x83\xBC\xE3\x81\xB5\xE3\x82\x8B"},
    // "bihongo" -> "にほんご", "びほんご"
    { "bihongo",
      "\xE3\x81\xAB\xE3\x81\xBB\xE3\x82\x93\xE3\x81\x94",
      "\xE3\x81\xB3\xE3\x81\xBB\xE3\x82\x93\xE3\x81\x94"},
    // "yajiniku" -> "やきにく", "やじにく"
    { "yajiniku",
      "\xE3\x82\x84\xE3\x81\x8D\xE3\x81\xAB\xE3\x81\x8F",
      "\xE3\x82\x84\xE3\x81\x98\xE3\x81\xAB\xE3\x81\x8F"},
    // "so-natsu" -> "どーなつ", "そーなつ"
    { "so-natsu",
      "\xE3\x81\xA9\xE3\x83\xBC\xE3\x81\xAA\xE3\x81\xA4",
      "\xE3\x81\x9D\xE3\x83\xBC\xE3\x81\xAA\xE3\x81\xA4"},
    // "ohayou" -> NULL, "おはよう"
    // "おはよう" can be generated from raw key so
    // it shouldn't be in correction candidates.
    { "ohayou",
      NULL,
      "\xE3\x81\x8A\xE3\x81\xAF\xE3\x82\x88\xE3\x81\x86"},
    // "syamoji" -> NULL, "しゃもじ"
    // A query which can be composed from raw input
    // shouldn't be in correction candidates.
    // This is more complex pattern than "おはよう" test case.
    // "おはよう" case can be processed correctly by comparing
    // raw input and corrected query.
    // But this case "syamoji" (raw input) and "shamoji" (corrected input)
    // are different but their query is identical ("しゃもじ").
    // Thus we have to also check not only raw/corrected input but also
    // raw/corrected queries.
    { "syamozi",
      NULL,
      "\xE3\x81\x97\xE3\x82\x83\xE3\x82\x82\xE3\x81\x98"},
    // "kaish" -> NULL, "かいしゃ"
    // Pending input is expanded into possible queries for
    // kana-modifier-insensitive-conversion (a.k.a かつこう変換).
    // In this case "kaish" is expanded into "かいしゃ", "かいしゅ"
    // and so on.
    // Typing corrected input "kaisy" is also expanded into
    // "かいしゃ", "かいしゅ" and so on but they are duplicate
    // of expanded queries from "kaish".
    // Thus they shouldn't be in corrected candidates.
    { "kaish",
      NULL,
      "\xE3\x81\x8B\xE3\x81\x84\xE3\x81\x97\xE3\x82\x83"},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    SCOPED_TRACE(string("key: ") + kTestCases[i].keys);
    InsertOneByOne(kTestCases[i].keys, &corrector);
    vector<TypeCorrectedQuery> queries;
    corrector.GetQueriesForPrediction(&queries);
    // Number of queries can be equal to kCorrectedQueries.
    EXPECT_GE(kCorrectedQueryResults, queries.size());
    for (vector<TypeCorrectedQuery>::iterator it = queries.begin();
         it != queries.end();
         ++it) {
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

TEST_F(TypingCorrectorTest, Invalidate) {
  const CostTableForTest *table = Singleton<CostTableForTest>::get();

  TypingCorrector corrector(&qwerty_table_, 30, 30);

  EXPECT_TRUE(corrector.IsAvailable());
  table->InsertCharacter(&corrector, "p");
  table->InsertCharacter(&corrector, "h");
  table->InsertCharacter(&corrector, "a");

  corrector.Invalidate();
  EXPECT_FALSE(corrector.IsAvailable());

  table->InsertCharacter(&corrector, "y");
  table->InsertCharacter(&corrector, "o");
  table->InsertCharacter(&corrector, "u");

  vector<TypeCorrectedQuery> queries;
  corrector.GetQueriesForPrediction(&queries);
  EXPECT_TRUE(queries.empty());
}

TEST_F(TypingCorrectorTest, CopyFrom) {
  TypingCorrector corrector(&qwerty_table_, 30, 30);
  InsertOneByOne("phayou", &corrector);

  TypingCorrector corrector2(NULL, 1000, 1000);
  corrector2.CopyFrom(corrector);

  ExpectTypingCorrectorEqual(corrector, corrector2);
}

}  // namespace composer
}  // namespace mozc
