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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/assign.h"
#include "converter/segments.h"
#include "data_manager/serialized_dictionary.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/rewriter_util.h"
#include "absl/strings/string_view.h"

using mozc::dictionary::PosMatcher;

namespace mozc {

namespace {

bool IsEnableSingleKanjiPrediction(
    const ConversionRequest &conversion_request) {
  const commands::Request &request = conversion_request.request();
  return request.mixed_conversion() &&
         request.decoder_experiment_params().enable_single_kanji_prediction();
}

void InsertNounPrefix(const PosMatcher &pos_matcher, Segment *segment,
                      SerializedDictionary::iterator begin,
                      SerializedDictionary::iterator end) {
  DCHECK(begin != end);

  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return;
  }

  if (segment->segment_type() == Segment::FIXED_VALUE) {
    return;
  }

  const std::string &candidate_key =
      ((!segment->key().empty()) ? segment->key() : segment->candidate(0).key);
  for (auto iter = begin; iter != end; ++iter) {
    // Note:
    // The entry "cost" of noun_prefix_dictionary is "0" or "1".
    // Please refer to: mozc/rewriter/gen_single_kanji_noun_prefix_data.cc
    const int insert_pos = RewriterUtil::CalculateInsertPosition(
        *segment,
        static_cast<int>(iter.cost() + (segment->candidate(0).attributes &
                                        Segment::Candidate::CONTEXT_SENSITIVE)
                             ? 1
                             : 0));
    Segment::Candidate *c = segment->insert_candidate(insert_pos);
    c->lid = pos_matcher.GetNounPrefixId();
    c->rid = pos_matcher.GetNounPrefixId();
    c->cost = 5000;
    strings::Assign(c->content_value, iter.value());
    c->key = candidate_key;
    c->content_key = candidate_key;
    strings::Assign(c->value, iter.value());
    c->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }
}

}  // namespace

SingleKanjiRewriter::SingleKanjiRewriter(
    const DataManagerInterface &data_manager)
    : pos_matcher_(data_manager.GetPosMatcherData()),
      single_kanji_dictionary_(
          new dictionary::SingleKanjiDictionary(data_manager)) {}

SingleKanjiRewriter::~SingleKanjiRewriter() = default;

int SingleKanjiRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool SingleKanjiRewriter::Rewrite(const ConversionRequest &request,
                                  Segments *segments) const {
  if (!request.config().use_single_kanji_conversion()) {
    VLOG(2) << "no use_single_kanji_conversion";
    return false;
  }
  if (IsEnableSingleKanjiPrediction(request) &&
      request.request_type() != ConversionRequest::CONVERSION) {
    VLOG(2) << "single kanji prediction is enabled";
    return false;
  }

  bool modified = false;
  const size_t segments_size = segments->conversion_segments_size();
  const bool is_single_segment = (segments_size == 1);
  const bool use_svs = (request.request()
                            .decoder_experiment_params()
                            .variation_character_types() &
                        commands::DecoderExperimentParams::SVS_JAPANESE);
  for (size_t i = 0; i < segments_size; ++i) {
    AddDescriptionForExistingCandidates(
        segments->mutable_conversion_segment(i));

    const std::string &key = segments->conversion_segment(i).key();
    std::vector<std::string> kanji_list;
    if (!single_kanji_dictionary_->LookupKanjiEntries(key, use_svs,
                                                      &kanji_list)) {
      continue;
    }
    modified |=
        InsertCandidate(is_single_segment, pos_matcher_.GetGeneralSymbolId(),
                        kanji_list, segments->mutable_conversion_segment(i));
  }

  // Tweak for noun prefix.
  // TODO(team): Ideally, this issue can be fixed via the language model
  // and dictionary generation.
  for (size_t i = 0; i < segments_size; ++i) {
    if (segments->conversion_segment(i).candidates_size() == 0) {
      continue;
    }

    if (i + 1 < segments_size) {
      const Segment::Candidate &right_candidate =
          segments->conversion_segment(i + 1).candidate(0);
      // right segment must be a noun.
      if (!pos_matcher_.IsContentNoun(right_candidate.lid)) {
        continue;
      }
    } else if (segments_size != 1) {  // also apply if segments_size == 1.
      continue;
    }

    const std::string &key = segments->conversion_segment(i).key();
    const auto range = single_kanji_dictionary_->LookupNounPrefixEntries(key);
    if (range.first == range.second) {
      continue;
    }
    InsertNounPrefix(pos_matcher_, segments->mutable_conversion_segment(i),
                     range.first, range.second);
    // Ignore the next noun content word.
    ++i;
    modified = true;
  }

  return modified;
}

// Add single kanji variants description to existing candidates,
// because if we have candidates with same value, the lower ranked candidate
// will be removed.
void SingleKanjiRewriter::AddDescriptionForExistingCandidates(
    Segment *segment) const {
  DCHECK(segment);
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    Segment::Candidate *cand = segment->mutable_candidate(i);
    if (!cand->description.empty()) {
      continue;
    }
    single_kanji_dictionary_->GenerateDescription(cand->value,
                                                  &cand->description);
  }
}

// Insert SingleKanji into segment.
bool SingleKanjiRewriter::InsertCandidate(
    bool is_single_segment, uint16_t single_kanji_id,
    const std::vector<std::string> &kanji_list, Segment *segment) const {
  DCHECK(segment);
  DCHECK(!kanji_list.empty());
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candidates_size is 0";
    return false;
  }

  const std::string &candidate_key =
      ((!segment->key().empty()) ? segment->key() : segment->candidate(0).key);

  // Adding 8000 to the single kanji cost
  // Note that this cost does not make no effect.
  // Here we set the cost just in case.
  constexpr int kOffsetCost = 8000;

  // Append single-kanji
  for (size_t i = 0; i < kanji_list.size(); ++i) {
    Segment::Candidate *c = segment->push_back_candidate();
    FillCandidate(candidate_key, kanji_list[i], kOffsetCost + i,
                  single_kanji_id, c);
  }
  return true;
}

void SingleKanjiRewriter::FillCandidate(const absl::string_view key,
                                        const absl::string_view value,
                                        const int cost,
                                        const uint16_t single_kanji_id,
                                        Segment::Candidate *cand) const {
  cand->lid = single_kanji_id;
  cand->rid = single_kanji_id;
  cand->cost = cost;
  strings::Assign(cand->content_key, key);
  strings::Assign(cand->content_value, value);
  strings::Assign(cand->key, key);
  strings::Assign(cand->value, value);
  cand->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
  cand->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  single_kanji_dictionary_->GenerateDescription(value, &cand->description);
}
}  // namespace mozc
