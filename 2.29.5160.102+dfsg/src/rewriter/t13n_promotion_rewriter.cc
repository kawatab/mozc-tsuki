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

#include "rewriter/t13n_promotion_rewriter.h"

#include <algorithm>
#include <string>

#include "base/util.h"
#include "composer/composer.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_util.h"
#include "transliteration/transliteration.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/string_view.h"

namespace mozc {

namespace {

// The insertion offset for T13N candidates.
// Only one of Latin T13n candidates (width, case variants for Latin character
// keys) and katakana T13n candidates (Katakana variants for other keys) will
// be promoted.
constexpr size_t kLatinT13nOffset = 3;
constexpr size_t kKatakanaT13nOffset = 5;

bool IsLatinInputMode(const ConversionRequest &request) {
  return (request.has_composer() &&
          (request.composer().GetInputMode() == transliteration::HALF_ASCII ||
           request.composer().GetInputMode() == transliteration::FULL_ASCII));
}

bool MaybeInsertLatinT13n(Segment *segment) {
  if (segment->meta_candidates_size() <=
      transliteration::FULL_ASCII_CAPITALIZED) {
    return false;
  }

  const size_t insert_pos =
      RewriterUtil::CalculateInsertPosition(*segment, kLatinT13nOffset);

  absl::flat_hash_set<absl::string_view> seen;
  for (size_t i = 0; i < insert_pos; ++i) {
    seen.insert(segment->candidate(i).value);
  }

  static constexpr transliteration::TransliterationType kLatinT13nTypes[] = {
      transliteration::HALF_ASCII,
      transliteration::FULL_ASCII,
      transliteration::HALF_ASCII_UPPER,
      transliteration::FULL_ASCII_UPPER,
      transliteration::HALF_ASCII_LOWER,
      transliteration::FULL_ASCII_LOWER,
      transliteration::HALF_ASCII_CAPITALIZED,
      transliteration::FULL_ASCII_CAPITALIZED,
  };

  size_t pos = insert_pos;
  for (const auto t13n_type : kLatinT13nTypes) {
    const Segment::Candidate &t13n_candidate =
        segment->meta_candidate(t13n_type);
    auto [it, inserted] = seen.insert(t13n_candidate.value);
    if (!inserted) {
      continue;
    }
    *(segment->insert_candidate(pos)) = t13n_candidate;
    ++pos;
  }
  return pos != insert_pos;
}

bool MaybePromoteKatakana(Segment *segment) {
  if (segment->meta_candidates_size() <= transliteration::FULL_KATAKANA) {
    return false;
  }

  const Segment::Candidate &katakana_candidate =
      segment->meta_candidate(transliteration::FULL_KATAKANA);
  const std::string &katakana_value = katakana_candidate.value;
  if (!Util::IsScriptType(katakana_value, Util::KATAKANA)) {
    return false;
  }

  for (size_t i = 0;
       i < std::min(segment->candidates_size(), kKatakanaT13nOffset); ++i) {
    if (segment->candidate(i).value == katakana_value) {
      // No need to promote or insert.
      return false;
    }
  }

  Segment::Candidate insert_candidate = katakana_candidate;
  size_t index = kKatakanaT13nOffset;
  for (; index < segment->candidates_size(); ++index) {
    if (segment->candidate(index).value == katakana_value) {
      break;
    }
  }

  const size_t insert_pos =
      RewriterUtil::CalculateInsertPosition(*segment, kKatakanaT13nOffset);
  if (index < segment->candidates_size()) {
    const Segment::Candidate insert_candidate = segment->candidate(index);
    *(segment->insert_candidate(insert_pos)) = insert_candidate;
  } else {
    *(segment->insert_candidate(insert_pos)) = katakana_candidate;
  }

  return true;
}

bool MaybePromoteT13n(const ConversionRequest &request, Segment *segment) {
  if (IsLatinInputMode(request) || Util::IsAscii(segment->key())) {
    return MaybeInsertLatinT13n(segment);
  }
  return MaybePromoteKatakana(segment);
}

}  // namespace

T13nPromotionRewriter::T13nPromotionRewriter() = default;

T13nPromotionRewriter::~T13nPromotionRewriter() = default;

int T13nPromotionRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {  // For mobile
    return RewriterInterface::ALL;
  } else {
    return RewriterInterface::NOT_AVAILABLE;
  }
}

bool T13nPromotionRewriter::Rewrite(const ConversionRequest &request,
                                    Segments *segments) const {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    modified |=
        MaybePromoteT13n(request, segments->mutable_conversion_segment(i));
  }
  return modified;
}

}  // namespace mozc
