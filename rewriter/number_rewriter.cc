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

#include "rewriter/number_rewriter.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/number_compound_util.h"
#include "session/commands.pb.h"

namespace mozc {
namespace {

// Rewrite type
enum RewriteType {
  NO_REWRITE = 0,
  ARABIC_FIRST,  // arabic candidates first ordering
  KANJI_FIRST,  // kanji candidates first ordering
};

struct RewriteCandidateInfo {
  RewriteType type;
  int position;
  Segment::Candidate candidate;
};

// Returns rewrite type for the given segment and base candidate information.
// base_candidate_pos: the index of the base candidate.
// *arabic_candidate: arabic candidate using numeric style conversion.
// POS information, cost, etc will be copied from base candidate.
RewriteType GetRewriteTypeAndBase(
    const CounterSuffixEntry *suffix_array,
    size_t suffix_array_size,
    const Segment &seg,
    int base_candidate_pos,
    const POSMatcher &pos_matcher,
    Segment::Candidate *arabic_candidate) {
  DCHECK(arabic_candidate);

  const Segment::Candidate &c = seg.candidate(base_candidate_pos);
  if (!number_compound_util::IsNumber(suffix_array, suffix_array_size,
                                      pos_matcher, c)) {
    return NO_REWRITE;
  }

  if (Util::GetScriptType(c.content_value) == Util::NUMBER) {
    arabic_candidate->CopyFrom(c);
    arabic_candidate->inner_segment_boundary.clear();
    DCHECK(arabic_candidate->IsValid());
    return ARABIC_FIRST;
  }

  string half_width_new_content_value;
  Util::FullWidthToHalfWidth(c.content_key, &half_width_new_content_value);
  // Try to get normalized kanji_number and arabic_number.
  // If it failed, do nothing.
  // Retain suffix for later use.
  string number_suffix, kanji_number, arabic_number;
  if (!NumberUtil::NormalizeNumbersWithSuffix(c.content_value,
                                              true,  // trim_reading_zeros
                                              &kanji_number,
                                              &arabic_number,
                                              &number_suffix) ||
      arabic_number == half_width_new_content_value) {
    return NO_REWRITE;
  }
  const string suffix(c.value, c.content_value.size(),
                      c.value.size() - c.content_value.size());
  arabic_candidate->Init();
  arabic_candidate->value = arabic_number + number_suffix + suffix;
  arabic_candidate->content_value = arabic_number + number_suffix;
  arabic_candidate->key = c.key;
  arabic_candidate->content_key = c.content_key;
  arabic_candidate->cost = c.cost;
  arabic_candidate->structure_cost = c.structure_cost;
  arabic_candidate->lid = c.lid;
  arabic_candidate->rid = c.rid;
  DCHECK(arabic_candidate->IsValid());
  return KANJI_FIRST;
}

void GetRewriteCandidateInfos(
    const CounterSuffixEntry *suffix_array, size_t suffix_array_size,
    const Segment &seg,
    const POSMatcher &pos_matcher,
    vector<RewriteCandidateInfo> *rewrite_candidate_info) {
  DCHECK(rewrite_candidate_info);
  RewriteCandidateInfo info;

  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const RewriteType type = GetRewriteTypeAndBase(
        suffix_array, suffix_array_size,
        seg, i, pos_matcher, &info.candidate);
    if (type == NO_REWRITE) {
      continue;
    }
    info.type = type;
    info.position = i;
    rewrite_candidate_info->push_back(info);
  }
}

// If top candidate is Kanji numeric, we want to expand at least
// 5 candidates apart from base candidate.
// http://b/issue?id=2872048
const int kArabicNumericOffset = 5;

void PushBackCandidate(const string &value, const string &desc,
                       NumberUtil::NumberString::Style style,
                       vector<Segment::Candidate> *results) {
  bool found = false;
  for (vector<Segment::Candidate>::const_iterator it = results->begin();
       it != results->end(); ++it) {
    if (it->value == value) {
      found = true;
      break;
    }
  }
  if (!found) {
    Segment::Candidate cand;
    cand.value = value;
    cand.description = desc;
    cand.style = style;
    results->push_back(cand);
  }
}

void SetCandidatesInfo(const Segment::Candidate &arabic_cand,
                       vector<Segment::Candidate> *candidates) {
  const string suffix(
      arabic_cand.value, arabic_cand.content_value.size(),
      arabic_cand.value.size() - arabic_cand.content_value.size());

  for (vector<Segment::Candidate>::iterator it = candidates->begin();
       it != candidates->end(); ++it) {
    it->content_value.assign(it->value);
    it->value.append(suffix);
  }
}

class CheckValueOperator {
 public:
  explicit CheckValueOperator(const string &v) : find_value_(v) {}
  bool operator() (const Segment::Candidate &cand) const {
    return (cand.value == find_value_);
  }

 private:
  const string &find_value_;
};

// If we have the candidates to be inserted before the base candidate,
// delete them.
// TODO(toshiyuki): Delete candidates between base pos and insert pos
// if necessary.
void EraseExistingCandidates(
    const vector<Segment::Candidate> &results,
    int base_candidate_pos,
    Segment *seg,
    vector<RewriteCandidateInfo> *rewrite_candidate_info_list) {
  DCHECK(seg);
  // Remember base candidate value
  for (int pos = base_candidate_pos - 1; pos >= 0; --pos) {
    // Simple liner search. |results| size is small. (at most 10 or so)
    const vector<Segment::Candidate>::const_iterator iter =
        find_if(results.begin(), results.end(),
                CheckValueOperator(seg->candidate(pos).value));
    if (iter == results.end()) {
      continue;
    }

    seg->erase_candidate(pos);

    // Adjust position in rewrite_candidate_info.
    for (size_t i = 0; i < rewrite_candidate_info_list->size(); ++i) {
      if ((*rewrite_candidate_info_list)[i].position > pos) {
        --(*rewrite_candidate_info_list)[i].position;
      }
    }
  }
}

// This is a utility function for InsertCandidate and UpdateCandidate.
// Do not use this function directly.
void MergeCandidateInfoInternal(const Segment::Candidate &base_cand,
                                const Segment::Candidate &result_cand,
                                Segment::Candidate *cand) {
  DCHECK(cand);
  cand->lid = base_cand.lid;
  cand->rid = base_cand.rid;
  cand->cost = base_cand.cost;
  cand->value = result_cand.value;
  cand->content_value = result_cand.content_value;
  cand->key = base_cand.key;
  cand->content_key = base_cand.content_key;
  cand->style = result_cand.style;
  cand->description = result_cand.description;
  // Don't want to have FULL_WIDTH form for Hex/Oct/BIN..etc.
  if (cand->style == NumberUtil::NumberString::NUMBER_HEX ||
      cand->style == NumberUtil::NumberString::NUMBER_OCT ||
      cand->style == NumberUtil::NumberString::NUMBER_BIN) {
    cand->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
  }
}

void InsertCandidate(Segment *segment,
                     int32 insert_position,
                     const Segment::Candidate &base_cand,
                     const Segment::Candidate &result_cand) {
  DCHECK(segment);
  Segment::Candidate *c = segment->insert_candidate(insert_position);
  c->Init();
  MergeCandidateInfoInternal(base_cand, result_cand, c);
}

void UpdateCandidate(Segment *segment,
                     int32 update_position,
                     const Segment::Candidate &base_cand,
                     const Segment::Candidate &result_cand) {
  DCHECK(segment);
  Segment::Candidate *c = segment->mutable_candidate(update_position);
  // Do not call |c->Init()| for an existing candidate.
  // There are two major reasons.
  // 1) Future design change may introduce another field into
  //    Segment::Candidate. In such situation, simply calling |c->Init()|
  //    for an existing candidate may result in unexpeced data loss.
  // 2) In order to preserve existing attribute information such as
  //    Segment::Candidate::USER_DICTIONARY bit in |c|, we cannot not call
  //    |c->Init()|. Note that neither |base_cand| nor |result[0]| has
  //    valid value in its |attributes|.
  MergeCandidateInfoInternal(base_cand, result_cand, c);
}

void InsertConvertedCandidates(const vector<Segment::Candidate> &results,
                               const Segment::Candidate &base_cand,
                               int base_candidate_pos,
                               int insert_pos, Segment *seg) {
  if (results.empty()) {
    return;
  }
  if (base_candidate_pos >= seg->candidates_size()) {
    LOG(WARNING) << "Invalid base candidate pos";
    return;
  }
  // First, insert top candidate
  // If we find the base candidate is equal to the converted
  // special form candidates, we will rewrite it.
  // Otherwise, we will insert top candidate just below the base.
  // Sometimes original base candidate is different from converted candidate
  // For example, "千万" v.s. "一千万", or "一二三" v.s. "百二十三".
  // We don't want to rewrite "千万" to "一千万".
  {
    const string &base_value = seg->candidate(base_candidate_pos).value;
    vector<Segment::Candidate>::const_iterator itr =
        find_if(results.begin(), results.end(), CheckValueOperator(base_value));
    if (itr != results.end() &&
        itr->style != NumberUtil::NumberString::NUMBER_KANJI &&
        itr->style != NumberUtil::NumberString::NUMBER_KANJI_ARABIC) {
      // Update exsisting base candidate
      UpdateCandidate(seg, base_candidate_pos, base_cand, results[0]);
    } else {
      // Insert candidate just below the base candidate
      InsertCandidate(seg, base_candidate_pos + 1, base_cand, results[0]);
      ++insert_pos;
    }
  }

  // Insert others
  for (size_t i = 1; i < results.size(); ++i) {
    InsertCandidate(seg, insert_pos++, base_cand, results[i]);
  }
}

int GetInsertPos(int base_pos, const Segment &segment, RewriteType type) {
  if (type == ARABIC_FIRST) {
    // +2 for arabic half_width full_width expansion
    return min(base_pos + 2, static_cast<int>(segment.candidates_size()));
  } else {
    return min(base_pos + kArabicNumericOffset,
               static_cast<int>(segment.candidates_size()));
  }
}

void InsertHalfArabic(const string &half_arabic,
                      vector<NumberUtil::NumberString> *output) {
  output->push_back(
      NumberUtil::NumberString(half_arabic, "",
                               NumberUtil::NumberString::DEFAULT_STYLE));
}

void GetNumbers(RewriteType type, bool exec_radix_conversion,
                const string &arabic_content_value,
                vector<NumberUtil::NumberString> *output) {
  DCHECK(output);
  if (type == ARABIC_FIRST) {
    InsertHalfArabic(arabic_content_value, output);
    NumberUtil::ArabicToWideArabic(arabic_content_value, output);
    NumberUtil::ArabicToSeparatedArabic(arabic_content_value, output);
    NumberUtil::ArabicToKanji(arabic_content_value, output);
    NumberUtil::ArabicToOtherForms(arabic_content_value, output);
  } else if (type == KANJI_FIRST) {
    NumberUtil::ArabicToKanji(arabic_content_value, output);
    InsertHalfArabic(arabic_content_value, output);
    NumberUtil::ArabicToWideArabic(arabic_content_value, output);
    NumberUtil::ArabicToSeparatedArabic(arabic_content_value, output);
    NumberUtil::ArabicToOtherForms(arabic_content_value, output);
  }

  if (exec_radix_conversion) {
    NumberUtil::ArabicToOtherRadixes(arabic_content_value, output);
  }
}

bool RewriteOneSegment(
    const CounterSuffixEntry *suffix_array, size_t suffix_array_size,
    const POSMatcher &pos_matcher, bool exec_radix_conversion, Segment *seg) {
  DCHECK(seg);
  bool modified = false;
  vector<RewriteCandidateInfo> rewrite_candidate_infos;
  GetRewriteCandidateInfos(suffix_array, suffix_array_size,
                           *seg, pos_matcher, &rewrite_candidate_infos);

  for (int i = rewrite_candidate_infos.size() - 1; i >= 0; --i) {
    const RewriteCandidateInfo &info = rewrite_candidate_infos[i];
    if (info.candidate.content_value.size() > info.candidate.value.size()) {
      LOG(ERROR) << "Invalid content_value/value: ";
      break;
    }

    string arabic_content_value;
    Util::FullWidthToHalfWidth(
        info.candidate.content_value, &arabic_content_value);
    if (Util::GetScriptType(arabic_content_value) != Util::NUMBER) {
      if (Util::GetFirstScriptType(arabic_content_value) == Util::NUMBER) {
        // Rewrite for number suffix
        const int insert_pos = min(info.position + 1,
                                   static_cast<int>(seg->candidates_size()));
        InsertCandidate(seg, insert_pos, info.candidate, info.candidate);
        modified = true;
        continue;
      }
      LOG(ERROR) << "arabic_content_value is not number: "
                 << arabic_content_value;
      break;
    }
    vector<NumberUtil::NumberString> output;
    GetNumbers(info.type, exec_radix_conversion, arabic_content_value, &output);
    vector<Segment::Candidate> converted_numbers;
    for (int j = 0; j < output.size(); ++j) {
      PushBackCandidate(output[j].value, output[j].description,
                        output[j].style, &converted_numbers);
    }
    SetCandidatesInfo(info.candidate, &converted_numbers);

    // Caution!!!: This invocation will update the data inside of the
    // rewrite_candidate_infos. Thus, |info| also can be updated as well
    // regardless of whether it's const reference-ness.
    EraseExistingCandidates(converted_numbers, info.position,
                            seg, &rewrite_candidate_infos);
    int insert_pos = GetInsertPos(info.position, *seg, info.type);
    DCHECK_LT(info.position, insert_pos);
    InsertConvertedCandidates(
        converted_numbers, info.candidate, info.position, insert_pos, seg);
    modified = true;
  }
  return modified;
}
}  // namespace

NumberRewriter::NumberRewriter(const DataManagerInterface *data_manager)
    : pos_matcher_(data_manager->GetPOSMatcher()) {
  data_manager->GetCounterSuffixSortedArray(&suffix_array_,
                                            &suffix_array_size_);
}

NumberRewriter::~NumberRewriter() {}

int NumberRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool NumberRewriter::Rewrite(const ConversionRequest &request,
                             Segments *segments) const {
  DCHECK(segments);
  if (!GET_CONFIG(use_number_conversion)) {
    VLOG(2) << "no use_number_conversion";
    return false;
  }

  bool modified = false;
  // Radix conversion is done only for conversion mode.
  // Showing radix candidates is annoying for an user.
  const bool exec_radix_conversion =
      (segments->conversion_segments_size() == 1 &&
       segments->request_type() == Segments::CONVERSION);

  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    Segment *seg = segments->mutable_conversion_segment(i);
    modified |= RewriteOneSegment(suffix_array_, suffix_array_size_,
                                  *pos_matcher_, exec_radix_conversion, seg);
  }

  return modified;
}

}  // namespace mozc
