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

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include "base/port.h"
#include "base/string_piece.h"
#include "composer/internal/composition.h"
#include "composer/internal/composition_input.h"
#include "composer/internal/typing_model.h"
#include "composer/table.h"
#include "composer/type_corrected_query.h"
#include "config/config.pb.h"
#include "config/config_handler.h"

DEFINE_bool(enable_typing_correction, false,
            "Force enabling typing correction feature "
            "regardless of GET_CONFIG(use_typing_correction).");

namespace mozc {
namespace composer {

namespace {

// Looks up model cost for current key given previous keys.
int LookupModelCost(const string &prev, const string &current,
                    const TypingModel &typing_model) {
  if (current.size() != 1) {
    return TypingModel::kInfinity;
  }
  char trigram[4] = { '^', '^', current[0], '\0' };
  if (prev.size() == 1) {
    trigram[1] = prev[0];
  } else if (prev.size() >= 2) {
    trigram[0] = prev[prev.size() - 2];
    trigram[1] = prev[prev.size() - 1];
  }

  const int cost = typing_model.GetCost(trigram);
  return cost == TypingModel::kNoData ? TypingModel::kInfinity : cost;
}

inline int Cost(double prob) {
  return static_cast<int>(-500.0 * log(prob));
}

}  // namespace


struct TypingCorrector::KeyAndPenaltyLess {
  bool operator()(const KeyAndPenalty &l, const KeyAndPenalty &r) const {
    return l.second < r.second;
  }
};

TypingCorrector::TypingCorrector(const Table *table,
                                 size_t max_correction_query_candidates,
                                 size_t max_correction_query_results)
    : table_(table),
      max_correction_query_candidates_(max_correction_query_candidates),
      max_correction_query_results_(max_correction_query_results) {
  Reset();
}

TypingCorrector::~TypingCorrector() {}

void TypingCorrector::InsertCharacter(
    const StringPiece key,
    const ProbableKeyEvents &probable_key_events) {
  key.AppendToString(&raw_key_);
  if (!IsAvailable() || probable_key_events.size() == 0) {
    // If this corrector is not available or no ProbableKeyEvent is available,
    // just append |key| to each corrections.
    for (size_t i = 0; i < top_n_.size(); ++i) {
      key.AppendToString(&top_n_[i].first);
    }
    return;
  }

  // Approximation of dynamic programming to find N least cost key sequences.
  // At each insertion, generate all the possible paths from previous N least
  // key sequences, and keep only new N least key sequences.
  vector<KeyAndPenalty> tmp;
  tmp.reserve(top_n_.size() * probable_key_events.size());
  for (size_t i = 0; i < top_n_.size(); ++i) {
    for (size_t j = 0; j < probable_key_events.size(); ++j) {
      const ProbableKeyEvent& event = probable_key_events.Get(j);
      const string key_as_string(1, event.key_code());
      const int new_cost = top_n_[i].second + Cost(event.probability())
          + LookupModelCost(top_n_[i].first, key_as_string,
                            *table_->typing_model());
      if (new_cost < TypingModel::kInfinity) {
        tmp.push_back(make_pair(top_n_[i].first + key_as_string, new_cost));
      }
    }
  }
  const size_t cutoff_size = min(max_correction_query_candidates_, tmp.size());
  partial_sort(tmp.begin(), tmp.begin() + cutoff_size, tmp.end(),
               KeyAndPenaltyLess());
  tmp.resize(cutoff_size);
  top_n_.swap(tmp);
}

void TypingCorrector::Reset() {
  raw_key_.clear();
  top_n_.clear();
  top_n_.push_back(KeyAndPenalty("", 0));
  available_ = true;
}

void TypingCorrector::Invalidate() {
  available_ = false;
}

bool TypingCorrector::IsAvailable() const {
  return (GET_CONFIG(use_typing_correction) ||
          FLAGS_enable_typing_correction) &&
         available_ && table_ && table_->typing_model();
}

void TypingCorrector::CopyFrom(const TypingCorrector &src) {
  available_ = src.available_;
  table_ = src.table_;
  max_correction_query_candidates_ = src.max_correction_query_candidates_;
  max_correction_query_results_ = src.max_correction_query_results_;
  top_n_ = src.top_n_;
}

void TypingCorrector::SetTable(const Table *table) {
  table_ = table;

  if (!raw_key_.empty()) {
    // If table is switched during the type-correcting, quit the typing
    // correction.
    available_ = false;
  }
}


void TypingCorrector::GetQueriesForPrediction(
    vector<TypeCorrectedQuery> *queries) const {
  queries->clear();
  if (!IsAvailable() || table_ == NULL || raw_key_.empty()) {
    return;
  }
  // These objects are for cache. Used and reset repeatedly.
  Composition c(table_);
  CompositionInput input;
  // We shouldn't return such queries which can be created from
  // raw input.
  // For example, "しゃもじ" shouldn't be in the returned queries
  // when the raw input is "shamoji" of QWERTY keyboard.
  // This behavior needs special implementation because
  // "syamoji" can be a typing corrected input from "shamoji",
  // and both input can create "しゃもじ".
  // So "shamoji" creates typing corrected input "syamoji", and
  // "syamoji" creates typing corrected query "しゃもじ", which
  // can be created from "shamoji".
  // 2nd exmple is "かいしゃ" from "kaish".
  // The raw input "kaish" and typing corrected input "kaisy"
  // create identical queries "かいしゃ", "かいしゅ" and "かいしょ".
  // So here is the same situation as 1st example.

  // Calculate all the queries which the raw input can create.
  // If ambiguity in the input (== no expansion is performed),
  // a query is created.
  // e.g. "shamoji" -> "しゃもじ"
  // If there is ambiguity, queries are created.
  // e.g. "kaish" -> "かいしゃ", "かいしゅ" and "かいしょ".
  set<string> raw_queries;
  {
    input.set_raw(raw_key_);
    input.set_is_new_input(true);
    c.InsertInput(0, input);
    string raw_base;
    set<string> raw_expanded;
    c.GetExpandedStrings(&raw_base, &raw_expanded);
    if (raw_expanded.empty()) {
      raw_queries.insert(raw_base);
    } else {
      for (set<string>::iterator it = raw_expanded.begin();
           it != raw_expanded.end();
           ++it) {
        raw_queries.insert(raw_base + *it);
      }
    }
  }

  // Filter all the typing correction queries.
  // If no queries are filtered, the number of retured queries
  // is top_n_.size().
  // So here we pregenerate top_n_.size() of initialized instances.
  queries->resize(top_n_.size());
  size_t result_count = 0;
  for (size_t i = 0;
       i < top_n_.size() && result_count < max_correction_query_results_;
       ++i) {
    const KeyAndPenalty &correction = top_n_[i];
    if (correction.first == raw_key_) {
      // If typing correction input is identical to raw input,
      // filter it because its queries are surely identical to
      // raw queries.
      continue;
    }
    TypeCorrectedQuery *query = &queries->at(result_count);
    // Fill TypeCorrectedQuery's base and expanded field
    // by using cached objects.
    input.Clear();
    input.set_raw(correction.first);
    input.set_is_new_input(true);
    c.Erase();
    c.InsertInput(0, input);
    c.GetExpandedStrings(&query->base, &query->expanded);
    if (query->expanded.empty()) {
      // This typing correction input has no ambiguity.
      // e.g. "syamoji" -> "しゃもじ".
      // So here we can check only TypeCorrectedQuery's base field.
      DCHECK(!query->base.empty());
      // If base is included in raw_queries, filter the query.
      // This is ["shamoji" and "syamoji]" case.
      if (raw_queries.find(query->base) != raw_queries.end()) {
        continue;
      }
    } else {
      // This typing correction input has ambiguity.
      // e.g. "kaish" -> "かいしゃ", "かいしゅ" and "かいしょ".
      // So we have to check expanded queries.
      for (set<string>::iterator it = query->expanded.begin();
           it != query->expanded.end();) {
        if (raw_queries.find(query->base + *it) != raw_queries.end()) {
          query->expanded.erase(it++);
        } else {
          ++it;
        }
      }
      if (query->expanded.empty()) {
        // If all the queries are in raw_queries,
        // this typing correction input shouldn't be returned.
        continue;
      }
    }
    query->cost = correction.second;
    ++result_count;
  }
  // If some queries are filtered, there are unused queries
  // at the tail of queries. Trim them.
  queries->resize(min(result_count, max_correction_query_results_));
}

}  // namespace composer
}  // namespace mozc
