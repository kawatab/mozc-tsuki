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

#include "rewriter/english_variants_rewriter.h"

#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/util.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "absl/container/flat_hash_set.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"

namespace mozc {

EnglishVariantsRewriter::EnglishVariantsRewriter() {}

EnglishVariantsRewriter::~EnglishVariantsRewriter() {}

bool EnglishVariantsRewriter::ExpandEnglishVariants(
    const std::string &input, std::vector<std::string> *variants) const {
  DCHECK(variants);

  if (input.empty()) {
    return false;
  }

  // multi-word
  if (absl::StrContains(input, " ")) {
    return false;
  }

  std::string lower = input;
  std::string upper = input;
  std::string capitalized = input;
  Util::LowerString(&lower);
  Util::UpperString(&upper);
  Util::CapitalizeString(&capitalized);

  if (lower == upper) {
    // given word is non-ascii.
    return false;
  }

  variants->clear();
  // If |input| is non-standard expression, like "iMac", only
  // expand lowercase.
  if (input != lower && input != upper && input != capitalized) {
    variants->push_back(lower);
    return true;
  }

  if (input != lower) {
    variants->push_back(lower);
  }
  if (input != capitalized) {
    variants->push_back(capitalized);
  }
  if (input != upper) {
    variants->push_back(upper);
  }

  return true;
}

bool EnglishVariantsRewriter::IsT13NCandidate(
    Segment::Candidate *candidate) const {
  return (Util::IsEnglishTransliteration(candidate->content_value) &&
          Util::GetScriptType(candidate->content_key) == Util::HIRAGANA);
}

bool EnglishVariantsRewriter::IsEnglishCandidate(
    Segment::Candidate *candidate) const {
  return (Util::IsEnglishTransliteration(candidate->content_value) &&
          Util::GetScriptType(candidate->content_key) == Util::ALPHABET);
}

bool EnglishVariantsRewriter::ExpandEnglishVariantsWithSegment(
    Segment *seg) const {
  CHECK(seg);

  bool modified = false;
  absl::flat_hash_set<std::string> expanded_t13n_candidates;
  absl::flat_hash_set<std::string> original_candidates;

  for (size_t i = 0; i < seg->candidates_size(); ++i) {
    original_candidates.insert(seg->candidate(i).value);
  }

  for (int i = seg->candidates_size() - 1; i >= 0; --i) {
    Segment::Candidate *original_candidate = seg->mutable_candidate(i);
    DCHECK(original_candidate);

    // http://b/issue?id=5137299
    // If the entry is coming from user dictionary,
    // expand English variants.
    if (original_candidate->attributes &
            Segment::Candidate::NO_VARIANTS_EXPANSION &&
        !(original_candidate->attributes &
          Segment::Candidate::USER_DICTIONARY)) {
      continue;
    }

    if (IsT13NCandidate(original_candidate)) {
      if (expanded_t13n_candidates.find(original_candidate->value) !=
          expanded_t13n_candidates.end()) {
        original_candidate->attributes |=
            Segment::Candidate::NO_VARIANTS_EXPANSION;
        continue;
      }

      // Expand T13N candidate variants
      modified = true;
      original_candidate->attributes |=
          Segment::Candidate::NO_VARIANTS_EXPANSION;
      std::vector<std::string> variants;
      if (ExpandEnglishVariants(original_candidate->content_value, &variants)) {
        CHECK(!variants.empty());
        for (auto it = variants.rbegin(); it != variants.rend(); ++it) {
          const std::string new_value =
              absl::StrCat(*it, original_candidate->functional_value());
          expanded_t13n_candidates.insert(new_value);
          if (original_candidates.find(new_value) !=
              original_candidates.end()) {
            continue;
          }
          Segment::Candidate *new_candidate = seg->insert_candidate(i + 1);
          DCHECK(new_candidate);
          new_candidate->Init();
          new_candidate->value = std::move(new_value);
          new_candidate->key = original_candidate->key;
          new_candidate->content_value = std::move(*it);
          new_candidate->content_key = original_candidate->content_key;
          new_candidate->cost = original_candidate->cost;
          new_candidate->wcost = original_candidate->wcost;
          new_candidate->structure_cost = original_candidate->structure_cost;
          new_candidate->lid = original_candidate->lid;
          new_candidate->rid = original_candidate->rid;
          new_candidate->attributes |=
              Segment::Candidate::NO_VARIANTS_EXPANSION;
        }
      }
    } else if (IsEnglishCandidate(original_candidate)) {
      // Fix variants for English candidate
      modified = true;
      original_candidate->attributes |=
          Segment::Candidate::NO_VARIANTS_EXPANSION;
    }
  }

  return modified;
}

int EnglishVariantsRewriter::capability(
    const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool EnglishVariantsRewriter::Rewrite(const ConversionRequest &request,
                                      Segments *segments) const {
  bool modified = false;
  for (size_t i = segments->history_segments_size();
       i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    DCHECK(seg);
    modified |= ExpandEnglishVariantsWithSegment(seg);
  }

  return modified;
}
}  // namespace mozc
