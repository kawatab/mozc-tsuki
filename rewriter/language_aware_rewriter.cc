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

#include "rewriter/language_aware_rewriter.h"

#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "composer/composer.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "session/commands.pb.h"
#include "usage_stats/usage_stats.h"

namespace mozc {

LanguageAwareRewriter::LanguageAwareRewriter(
    const POSMatcher &pos_matcher,
    const DictionaryInterface *dictionary)
    : unknown_id_(pos_matcher.GetUnknownId()),
      dictionary_(dictionary) {}

LanguageAwareRewriter::~LanguageAwareRewriter() {}

namespace {

bool IsEnabled(const mozc::commands::Request &request) {
  // The current default value of language_aware_input is
  // NO_LANGUAGE_AWARE_INPUT and only unittests set LANGUAGE_AWARE_SUGGESTION
  // at this moment.  Thus, FillRawText is not performed in the productions
  // yet.
  if (request.language_aware_input() ==
      mozc::commands::Request::NO_LANGUAGE_AWARE_INPUT) {
    return false;
  } else if (request.language_aware_input() ==
             mozc::commands::Request::LANGUAGE_AWARE_SUGGESTION) {
    return true;
  }
  DCHECK_EQ(mozc::commands::Request::DEFAULT_LANGUAGE_AWARE_BEHAVIOR,
            request.language_aware_input());

  if (!GET_CONFIG(use_spelling_correction)) {
    return false;
  }

#ifdef OS_ANDROID
  return false;
#else  // OS_ANDROID
  return true;
#endif  // OS_ANDROID
}

}  // namespace

int LanguageAwareRewriter::capability(
    const ConversionRequest &request) const {
  // Language aware input is performed only on suggestion or prediction.
  if (!IsEnabled(request.request())) {
    return RewriterInterface::NOT_AVAILABLE;
  }

  return (RewriterInterface::SUGGESTION | RewriterInterface::PREDICTION);
}

namespace {
bool IsRawQuery(const composer::Composer &composer,
                const DictionaryInterface *dictionary,
                int *rank) {
  string raw_text;
  composer.GetRawString(&raw_text);

  // Check if the length of text is less than or equal to three.
  // For example, "cat" is not treated as a raw query so far to avoid
  // false negative cases.
  if (raw_text.size() <= 3) {
    return false;
  }

  // If the composition string is same with the raw_text, there is no
  // need to add the candidate to suggestions.
  string composition;
  composer.GetStringForPreedit(&composition);
  if (composition == raw_text) {
    return false;
  }

  // If alphabet characters are in the middle of the composition, it is
  // probably a raw query.  For example, "えぁｍｐぇ" (example) contains
  // "m" and "p" in the middle.  So it is treated as a raw query.  On the
  // other hand, "くえｒｙ" (query) contains alphabet characters, but they
  // are at the end of the string, so it cannot be determined here.
  //
  // Note, GetQueryForPrediction omits the trailing alphabet characters of
  // the composition string and returns it.
  string key;
  composer.GetQueryForPrediction(&key);
  if (Util::ContainsScriptType(key, Util::ALPHABET)) {
    *rank = 0;
    return true;
  }

  // If the input text is stored in the dictionary, it is perhaps a raw query.
  // For example, the input characters of "れもヴぇ" (remove) is in the
  // dictionary, so it is treated as a raw text.  This logic is a little
  // aggressive because "たけ" (take), "ほうせ" (house) and so forth are also
  // treated as raw texts.
  if (dictionary->HasValue(raw_text)) {
    *rank = 2;
    return true;
  }

  return false;
}

// Get T13n candidate ids from existing candidates.
void GetAlphabetIds(const Segment &segment, uint16 *lid, uint16 *rid) {
  DCHECK(lid);
  DCHECK(rid);

  for (int i = 0; i < segment.candidates_size(); ++i) {
    const Segment::Candidate &candidate = segment.candidate(i);
    const Util::ScriptType type = Util::GetScriptType(candidate.value);
    if (type == Util::ALPHABET) {
      *lid = candidate.lid;
      *rid = candidate.rid;
      return;
    }
  }
}
}  // namespace

// Note: This function seemed slow, but the benchmark tests
// resulted that it was only less than 0.1% point penalty.
// = session_handler_benchmark_test
// BM_PerformanceForRandomKeyEvents: 891944807 -> 892740748 (1.00089)
// = converter_benchmark_test
// BM_DesktopAnthyCorpusConversion 25062440090 -> 25101542382 (1.002)
// BM_DesktopStationPredictionCorpusPrediction 8695341697 -> 8672187681 (0.997)
// BM_DesktopStationPredictionCorpusSuggestion 6149502840 -> 6152393270 (1.000)
bool LanguageAwareRewriter::FillRawText(
    const ConversionRequest &request, Segments *segments) const {
  if (segments->conversion_segments_size() != 1 || !request.has_composer()) {
    return false;
  }

  int rank = 0;
  if (!IsRawQuery(request.composer(), dictionary_, &rank)) {
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);

  string raw_string;
  request.composer().GetRawString(&raw_string);

  uint16 lid = unknown_id_;
  uint16 rid = unknown_id_;
  GetAlphabetIds(*segment, &lid, &rid);

  // Create a candidate.
  if (rank > segment->candidates_size()) {
    rank = segment->candidates_size();
  }
  Segment::Candidate *candidate = segment->insert_candidate(rank);
  candidate->Init();
  candidate->value = raw_string;
  candidate->key = raw_string;
  candidate->content_value = raw_string;
  candidate->content_key = raw_string;
  candidate->lid = lid;
  candidate->rid = rid;

  candidate->attributes |= (Segment::Candidate::NO_VARIANTS_EXPANSION |
                            Segment::Candidate::NO_EXTRA_DESCRIPTION);
  candidate->prefix = "\xE2\x86\x92 ";  // "→ "
  candidate->description =
    // "もしかして"
    "\xE3\x82\x82\xE3\x81\x97\xE3\x81\x8B\xE3\x81\x97\xE3\x81\xA6";

  // Set usage stats
  usage_stats::UsageStats::IncrementCount("LanguageAwareSuggestionTriggered");

  return true;
}

bool LanguageAwareRewriter::Rewrite(
    const ConversionRequest &request, Segments *segments) const {
  if (!IsEnabled(request.request())) {
    return false;
  }
  return FillRawText(request, segments);
}

namespace {
bool IsLangaugeAwareInputCandidate(const composer::Composer &composer,
                                   const Segment::Candidate &candidate) {
  // Check candidate.prefix to filter if the candidate is probably generated
  // from LanguangeAwareInput or not.
  //
  // "→ "
  if (candidate.prefix != "\xE2\x86\x92 ") {
    return false;
  }

  string raw_string;
  composer.GetRawString(&raw_string);
  if (raw_string != candidate.value) {
    return false;
  }
  return true;
}
}  // namespace

void LanguageAwareRewriter::Finish(const ConversionRequest &request,
                                     Segments *segments) {
  if (request.request().language_aware_input() !=
      mozc::commands::Request::LANGUAGE_AWARE_SUGGESTION) {
    return;
  }

  if (segments->conversion_segments_size() != 1 || !request.has_composer()) {
    return;
  }

  // Update usage stats
  const Segment &segment = segments->conversion_segment(0);
  // Ignores segments which are not converted or not committed.
  if (segment.candidates_size() == 0 ||
      segment.segment_type() != Segment::FIXED_VALUE) {
    return;
  }

  if (IsLangaugeAwareInputCandidate(request.composer(),
                                    segment.candidate(0))) {
    usage_stats::UsageStats::IncrementCount("LanguageAwareSuggestionCommitted");
  }
}

}  // namespace mozc
