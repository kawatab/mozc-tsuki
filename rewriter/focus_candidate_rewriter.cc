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

#include "rewriter/focus_candidate_rewriter.h"

#include <string>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/util.h"
#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/number_compound_util.h"

namespace mozc {
namespace {

enum {
  NUMBER = 1,
  SUFFIX = 2,
  CONNECTOR = 4
};

// TODO(taku): See POS and increase the coverage.
bool IsConnectorSegment(const Segment &segment) {
  return (segment.key() == "\xE3\x81\xA8" ||
          segment.key() == "\xE3\x82\x84");
}

// Finds value from the candidates list and move the canidate to the top.
bool RewriteCandidate(Segment *segment, const string &value) {
  for (int i = 0; i < segment->candidates_size(); ++i) {
    if (segment->candidate(i).content_value == value) {
      segment->move_candidate(i, 0);   // move to top
      return true;
    }
  }
  // Find value from meta candidates.
  for (int i = 0; i < segment->meta_candidates_size(); ++i) {
    if (segment->meta_candidate(i).content_value == value) {
      segment->move_candidate(-i - 1, 0);   // copy to top
      return true;
    }
  }
  return false;
}

// Returns true if the segment is valid.
bool IsValidSegment(const Segment &segment) {
  return (segment.segment_type() == Segment::FREE ||
          segment.segment_type() == Segment::FIXED_BOUNDARY ||
          segment.segment_type() == Segment::FIXED_VALUE);
}

bool IsNumberCandidate(const Segment::Candidate &candidate) {
  return (candidate.style != NumberUtil::NumberString::DEFAULT_STYLE ||
          Util::GetScriptType(candidate.value) == Util::NUMBER);
}

bool IsNumberSegment(const Segment &segment) {
  return (segment.candidates_size() > 0 &&
          IsNumberCandidate(segment.candidate(0)));
}

// Returns true if two candidates have the same number form.
bool IsSameNumberType(const Segment::Candidate &candidate1,
                      const Segment::Candidate &candidate2) {
  if (candidate1.style == candidate2.style) {
    if (candidate1.style == NumberUtil::NumberString::DEFAULT_STYLE) {
      if (IsNumberCandidate(candidate1) && IsNumberCandidate(candidate2) &&
          Util::GetFormType(candidate1.value) ==
          Util::GetFormType(candidate2.value)) {
        return true;
      }
    } else {
      return true;
    }
  }
  return false;
}

bool RewriteNumber(Segment *segment, const Segment::Candidate &candidate) {
  for (int i = 0; i < segment->candidates_size(); ++i) {
    if (IsSameNumberType(candidate, segment->candidate(i))) {
      segment->move_candidate(i, 0);   // move to top
      return true;
    }
  }

  // Find value from meta candidates.
  for (int i = 0; i < segment->meta_candidates_size(); ++i) {
    if (IsSameNumberType(candidate, segment->meta_candidate(i))) {
      segment->move_candidate(-i - 1, 0);   // copy to top
      return true;
    }
  }

  return false;
}

}  // namespace

FocusCandidateRewriter::FocusCandidateRewriter(
    const DataManagerInterface *data_manager) {
  data_manager->GetCounterSuffixSortedArray(&suffix_array_,
                                            &suffix_array_size_);
  pos_matcher_ = data_manager->GetPOSMatcher();
}

FocusCandidateRewriter::~FocusCandidateRewriter() {}

bool FocusCandidateRewriter::Focus(Segments *segments,
                                   size_t segment_index,
                                   int candidate_index) const {
  if (segments == NULL) {
    LOG(ERROR) << "Segments is NULL";
    return false;
  }

  if (segment_index >= segments->segments_size()) {
    LOG(WARNING) << "Segment index out of range";
    return false;
  }

  const Segment &seg = segments->segment(segment_index);

  // segment_type must be FREE or FIXED_BOUNDARY
  if (!IsValidSegment(seg)) {
    LOG(WARNING) << "Segment is not valid";
    return false;
  }

  // invalid candidate index
  if ((candidate_index < 0 &&
       -candidate_index - 1 >= static_cast<int>(seg.meta_candidates_size())) ||
      candidate_index >= static_cast<int>(seg.candidates_size())) {
    LOG(WARNING) << "Candidate index out of range: "
                 << candidate_index << " " << seg.candidates_size();
    return false;
  }

  // left to right
  {
    const string &left_value = seg.candidate(candidate_index).content_value;
    string right_value;

    if (Util::IsOpenBracket(left_value, &right_value)) {
      int num_nest = 1;
      for (size_t i = segment_index + 1; i < segments->segments_size(); ++i) {
        Segment *target_right_seg = segments->mutable_segment(i);
        if (target_right_seg == NULL ||
            target_right_seg->candidates_size() <= 0) {
          LOG(WARNING) << "target right seg is not valid";
          return false;
        }
        if (!IsValidSegment(*target_right_seg)) {
          continue;
        }
        const string &target_right_value =
            target_right_seg->candidate(0).content_value;
        string tmp;
        if (Util::IsOpenBracket(target_right_value, &tmp)) {
          ++num_nest;
        } else if (Util::IsCloseBracket(target_right_value, &tmp)) {
          --num_nest;
        }

        if (num_nest == 0 && RewriteCandidate(target_right_seg, right_value)) {
          return true;
        }
      }
      VLOG(1) << "could not find close bracket";
      return false;
    }
  }

  // right to left
  {
    const string &right_value = seg.candidate(candidate_index).content_value;
    string left_value;

    if (Util::IsCloseBracket(right_value, &left_value)) {
      int num_nest = 1;
      for (int i = segment_index - 1; i >= 0; --i) {
        Segment *target_left_seg = segments->mutable_segment(i);
        if (target_left_seg == NULL ||
            target_left_seg->candidates_size() <= 0) {
          LOG(WARNING) << "target left seg is not valid";
          return false;
        }
        if (!IsValidSegment(*target_left_seg)) {
          continue;
        }
        const string &target_left_value =
            target_left_seg->candidate(0).content_value;
        string tmp;
        if (Util::IsCloseBracket(target_left_value, &tmp)) {
          ++num_nest;
        } else if (Util::IsOpenBracket(target_left_value, &tmp)) {
          --num_nest;
        }
        if (num_nest == 0 && RewriteCandidate(target_left_seg, left_value)) {
          return true;
        }
      }
      VLOG(1) << "could not find open bracket";
      return false;
    }
  }

  {
    if (IsNumberCandidate(seg.candidate(candidate_index))) {
      bool modified = 0;
      int distance = 0;
      for (size_t i = segment_index + 1; i < segments->segments_size(); ++i) {
        Segment *target_right_seg = segments->mutable_segment(i);
        if (target_right_seg == NULL ||
            target_right_seg->candidates_size() <= 0) {
          LOG(WARNING) << "target right seg is not valid";
          return false;
        }
        if (!IsValidSegment(*target_right_seg)) {
          continue;
        }

        // Make sure the first candidate of the segment is number.
        if (IsNumberSegment(*target_right_seg) &&
            RewriteNumber(target_right_seg,
                          seg.candidate(candidate_index))) {
          modified = true;
          distance = 0;
        } else {
          ++distance;
        }
        // more than two segments between the target numbers
        if (distance >= 2) {
          break;
        }
      }
      return modified;
    }
  }

  {
    // <Number><Suffix><Connector>?<Number><Suffix><Connector>?
    if (segment_index > 0 &&
        IsNumberSegment(segments->segment(segment_index - 1)) &&
        seg.candidates_size() > 0 &&
        seg.candidate(0).content_key ==
        seg.candidate(candidate_index).content_key) {
      int next_stat = CONNECTOR | NUMBER;
      bool modified = false;
      for (size_t i = segment_index + 1; i < segments->segments_size(); ++i) {
        if (next_stat == (CONNECTOR | NUMBER)) {
          if (IsConnectorSegment(segments->segment(i))) {
            next_stat = NUMBER;
          } else if (IsNumberSegment(segments->segment(i))) {
            next_stat = SUFFIX;
          } else {
            break;
          }
        } else if (next_stat == NUMBER &&
                   IsNumberSegment(segments->segment(i))) {
          next_stat = SUFFIX;
        } else if (next_stat == SUFFIX &&
                   segments->segment(i).candidates_size() > 0 &&
                   segments->segment(i).candidate(0).content_key ==
                   seg.candidate(0).content_key) {
          if (!IsValidSegment(segments->segment(i))) {
            continue;
          }
          modified |= RewriteCandidate(
              segments->mutable_segment(i),
              seg.candidate(candidate_index).content_value);
          next_stat = CONNECTOR | NUMBER;
        } else {
          break;
        }
      }
      return modified;
    }
  }
  return RerankNumberCandidates(segments, segment_index, candidate_index);
}

bool FocusCandidateRewriter::RerankNumberCandidates(Segments *segments,
                                                    size_t segment_index,
                                                    int candidate_index) const {
  // Check if the focused candidate is a number compound.
  StringPiece number, suffix;
  uint32 number_script_type = 0;
  const Segment &seg = segments->segment(segment_index);
  if (!ParseNumberCandidate(seg.candidate(candidate_index), &number, &suffix,
                            &number_script_type)) {
    return false;
  }
  if (number.empty()) {
    return false;
  }

  // Try reranking top candidates of subsequent segments using the number
  // compound style of the focused candidate.
  bool modified = false;
  int distance = 0;
  for (size_t i = segment_index + 1; i < segments->segments_size(); ++i) {
    Segment *seg = segments->mutable_segment(i);
    const int index = FindMatchingCandidates(*seg, number_script_type, suffix);
    if (index == -1) {
      // If there's no appropriate candidate having the same style, we increment
      // the distance not to modify segments far from the focused one.
      if (++distance > 2) {
        break;
      }
      continue;
    }
    // Move the target candidate to the top.  We don't need to move if the
    // target is already at top (i.g., the case where index == 0).
    if (index > 0) {
      seg->move_candidate(index, 0);
      modified = true;
      distance = 0;
    }
  }
  return modified;
}

int FocusCandidateRewriter::FindMatchingCandidates(
    const Segment &seg, uint32 ref_script_type, StringPiece ref_suffix) const {
  // Only segments whose top candidate is a number compound are target of
  // reranking.
  const Segment::Candidate &cand = seg.candidate(0);
  StringPiece number, suffix;
  uint32 script_type = 0;
  if (!ParseNumberCandidate(cand, &number, &suffix, &script_type)) {
    return -1;
  }

  // Top candidate matches the style.
  if (script_type == ref_script_type && suffix == ref_suffix) {
    return 0;
  }

  // Check only top 10 candidates because, when the top candidate is a number
  // candidate, other number compounds likely to appear near the top candidate.
  const size_t max_size = min(seg.candidates_size(), static_cast<size_t>(10));
  for (size_t i = 1; i < max_size; ++i) {
    if (!ParseNumberCandidate(seg.candidate(i), &number, &suffix,
                              &script_type)) {
      continue;
    }
    if (script_type == ref_script_type && suffix == ref_suffix) {
      return i;
    }
  }
  return -1;
}

bool FocusCandidateRewriter::ParseNumberCandidate(
    const Segment::Candidate &cand, StringPiece* number,
    StringPiece* suffix, uint32 *script_type) const {
  // If the lengths of content value and value are different, particles may be
  // appended to value.  In such cases, we only accept parallel markers.
  // Otherwise, the following wrong rewrite will occur.
  // Example: "一階へは | 二回 | 行った -> 一階へは | 二階 | 行った"
  if (cand.content_value.size() != cand.value.size()) {
    if (!pos_matcher_->IsParallelMarker(cand.rid)) {
      return false;
    }
  }
  return number_compound_util::SplitStringIntoNumberAndCounterSuffix(
      suffix_array_, suffix_array_size_,
      cand.content_value, number, suffix, script_type);
}

}  // namespace mozc
