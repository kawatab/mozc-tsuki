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

#include "rewriter/symbol_rewriter.h"

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

// SymbolRewriter:
// When updating the rule
// 1. Export the spreadsheet into TEXT (TSV)
// 2. Copy the TSV to mozc/data/symbol/symbol.tsv
// 3. Run symbol_rewriter_dictionary_generator_main in this directory
// 4. Make sure symbol_rewriter_data.h is correct

namespace mozc {

namespace {
// Try to start inserting symbols from this position
const size_t kOffsetSize = 3;
// Number of symbols which are inserted to first part
const size_t kMaxInsertToMedium = 15;
}  // namespace

// Some characters may have different description for full/half width forms.
// Here we just change the description in this function.
// If the symbol has description and additional description,
// Return merged description.
// TODO(taku): allow us to define two descriptions in *.tsv file
// static function
const string SymbolRewriter::GetDescription(
    const string &value,
    StringPiece description,
    StringPiece additional_description) {
  if (description.empty()) {
    return "";
  }
  string result = description.as_string();
  // Merge description
  if (!additional_description.empty()) {
    result.append(1, '(');
    result.append(additional_description.data(), additional_description.size());
    result.append(1, ')');
  }
  return result;
}

// return true key has no-hiragana
// static function
bool SymbolRewriter::IsSymbol(const string &key) {
  for (ConstChar32Iterator iter(key); !iter.Done(); iter.Next()) {
    const char32 ucs4 = iter.Get();
    if (ucs4 >= 0x3041 && ucs4 <= 0x309F) {  // hiragana
      return false;
    }
  }
  return true;
}

// static function
void SymbolRewriter::ExpandSpace(Segment *segment) {
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).value == " ") {
      Segment::Candidate *c = segment->insert_candidate(i + 1);
      *c = segment->candidate(i);
      c->value = "　";  // Full-width space
      c->content_value = "　";  // Full-width space
      // Boundary is invalidated and unnecessary for space.
      c->inner_segment_boundary.clear();
      return;
    } else if (segment->candidate(i).value == "　") {  // Full-width space
      Segment::Candidate *c = segment->insert_candidate(i + 1);
      *c = segment->candidate(i);
      c->value = " ";
      c->content_value = " ";
      // Boundary is invalidated and unnecessary for space.
      c->inner_segment_boundary.clear();
      return;
    }
  }
}

// TODO(toshiyuki): Should we move this under Util module?
bool SymbolRewriter::IsPlatformDependent(
    SerializedDictionary::const_iterator iter) {
  if (iter.value().empty()) {
    return false;
  }
  const Util::CharacterSet cset = Util::GetCharacterSet(iter.value());
  return (cset >= Util::JISX0212);
}

// Return true if two symbols are in same group
// static function
bool SymbolRewriter::InSameSymbolGroup(
    SerializedDictionary::const_iterator lhs,
    SerializedDictionary::const_iterator rhs) {
  // "矢印記号", "矢印記号"
  // "ギリシャ(大文字)", "ギリシャ(小文字)"
  if (lhs.description().empty() || rhs.description().empty()) {
    return false;
  }
  const size_t cmp_len =
      std::max(lhs.description().size(), rhs.description().size());
  return std::strncmp(lhs.description().data(),
                      rhs.description().data(), cmp_len) == 0;
}

// Insert Symbol into segment.
// static function
void SymbolRewriter::InsertCandidates(
    const SerializedDictionary::IterRange &range,
    bool context_sensitive,
    Segment *segment) {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candiadtes_size is 0";
    return;
  }

  // work around for space.
  // space is not expanded in ExpandAlternative because it is not registered in
  // CharacterFormManager.
  // We do not want to make the form of spaces configurable, so we do not
  // register space to CharacterFormManager.
  ExpandSpace(segment);

  // If the original candidates given by ImmutableConveter already
  // include the target symbols, do assign description to these candidates.
  AddDescForCurrentCandidates(range, segment);

  const string &candidate_key = ((!segment->key().empty()) ?
                                 segment->key() :
                                 segment->candidate(0).key);
  size_t offset = 0;

  // If the key is "かおもじ", set the insert position at the bottom,
  // giving priority to emoticons inserted by EmoticonRewriter.
  if (candidate_key == "かおもじ") {
    offset = segment->candidates_size();
  } else {
    // Find the position wehere we start to insert the symbols
    // We want to skip the single-kanji we inserted by single-kanji rewriter.
    // We also skip transliterated key candidates.
    offset = std::min(kOffsetSize, segment->candidates_size());
    for (size_t i = offset; i < segment->candidates_size(); ++i) {
      const string &target_value = segment->candidate(i).value;
      if ((Util::CharsLen(target_value) == 1 &&
           Util::IsScriptType(target_value, Util::KANJI)) ||
          Util::IsScriptType(target_value, Util::HIRAGANA) ||
          Util::IsScriptType(target_value, Util::KATAKANA)) {
        ++offset;
      } else {
        break;
      }
    }
  }

  const size_t range_size = range.second - range.first;
  size_t inserted_count = 0;
  bool finish_first_part = false;
  const Segment::Candidate &base_candidate = segment->candidate(0);
  for (auto iter = range.first; iter != range.second; ++iter) {
    Segment::Candidate *candidate = segment->insert_candidate(offset);
    DCHECK(candidate);

    candidate->Init();
    candidate->lid = iter.lid();
    candidate->rid = iter.rid();
    candidate->cost = base_candidate.cost;
    candidate->structure_cost = base_candidate.structure_cost;
    candidate->value.assign(iter.value().data(), iter.value().size());
    candidate->content_value.assign(iter.value().data(), iter.value().size());
    candidate->key = candidate_key;
    candidate->content_key = candidate_key;

    if (context_sensitive) {
      candidate->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    }

    // The first two consist of two characters but the one of characters doesn't
    // have alternative character.
    if (candidate->value == "“”" || candidate->value == "‘’" ||
        candidate->value == "w" || candidate->value == "www") {
      candidate->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    }

    candidate->description = GetDescription(candidate->value,
                                            iter.description(),
                                            iter.additional_description());
    ++offset;
    ++inserted_count;

    // Insert to latter position
    // If number of rest symbols is small, insert current position.
    const auto next = iter + 1;
    if (next != range.second &&
        !finish_first_part &&
        inserted_count >= kMaxInsertToMedium &&
        range_size - inserted_count >= 5 &&
        // Do not divide symbols which seem to be in the same group
        // providing that they are not platform dependent characters.
        (!InSameSymbolGroup(iter, next) || IsPlatformDependent(next))) {
      offset = segment->candidates_size();
      finish_first_part = true;
    }
  }
}

// static
void SymbolRewriter::AddDescForCurrentCandidates(
    const SerializedDictionary::IterRange &range, Segment *segment) {
  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    Segment::Candidate *candidate = segment->mutable_candidate(i);
    string full_width_value, half_width_value;
    Util::HalfWidthToFullWidth(candidate->value, &full_width_value);
    Util::FullWidthToHalfWidth(candidate->value, &half_width_value);

    for (auto iter = range.first; iter != range.second; ++iter) {
      if (candidate->value == iter.value() ||
          full_width_value == iter.value() ||
          half_width_value == iter.value()) {
        candidate->description =
            GetDescription(candidate->value,
                           iter.description(),
                           iter.additional_description());
        break;
      }
    }
  }
}

bool SymbolRewriter::RewriteEachCandidate(Segments *segments) const {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    const string &key = segments->conversion_segment(i).key();
    const SerializedDictionary::IterRange range = dictionary_->equal_range(key);
    if (range.first == range.second) {
      continue;
    }

    // if key is symbol, no need to see the context
    const bool context_sensitive = !IsSymbol(key);

    InsertCandidates(range, context_sensitive,
                     segments->mutable_conversion_segment(i));

    modified = true;
  }

  return modified;
}

bool SymbolRewriter::RewriteEntireCandidate(const ConversionRequest &request,
                                            Segments *segments) const {
  string key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    key += segments->conversion_segment(i).key();
  }

  const SerializedDictionary::IterRange range = dictionary_->equal_range(key);
  if (range.first == range.second) {
    return false;
  }

  if (segments->conversion_segments_size() > 1) {
    if (segments->resized()) {
      // the given segments are resized by user
      // so don't modify anymore
      return false;
    }
    // need to resize
    const size_t all_length = Util::CharsLen(key);
    const size_t first_length =
        Util::CharsLen(segments->conversion_segment(0).key());
    const int diff = static_cast<int>(all_length - first_length);
    if (diff > 0) {
      parent_converter_->ResizeSegment(segments, request, 0, diff);
    }
  } else {
    InsertCandidates(range,
                     false,   // not context sensitive
                     segments->mutable_conversion_segment(0));
  }

  return true;
}

SymbolRewriter::SymbolRewriter(const ConverterInterface *parent_converter,
                               const DataManagerInterface *data_manager)
    : parent_converter_(parent_converter) {
  DCHECK(parent_converter_);
  StringPiece token_array_data, string_array_data;
  data_manager->GetSymbolRewriterData(&token_array_data, &string_array_data);
  DCHECK(SerializedDictionary::VerifyData(token_array_data, string_array_data));
  dictionary_.reset(new SerializedDictionary(token_array_data,
                                             string_array_data));
}

SymbolRewriter::~SymbolRewriter() {}

int SymbolRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool SymbolRewriter::Rewrite(const ConversionRequest &request,
                             Segments *segments) const {
  if (!request.config().use_symbol_conversion()) {
    VLOG(2) << "no use_symbol_conversion";
    return false;
  }

  // apply entire candidate first, as we want to
  // find character combinations first, e.g.,
  // "－＞" -> "→"
  return (RewriteEntireCandidate(request, segments) ||
          RewriteEachCandidate(segments));
}

}  // namespace mozc
