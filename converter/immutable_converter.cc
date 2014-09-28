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

#include "converter/immutable_converter.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/connector_interface.h"
#include "converter/conversion_request.h"
#include "converter/key_corrector.h"
#include "converter/lattice.h"
#include "converter/nbest_generator.h"
#include "converter/node.h"
#include "converter/node_list_builder.h"
#include "converter/segmenter_interface.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/suggestion_filter.h"
#include "session/commands.pb.h"

DECLARE_bool(disable_lattice_cache);
DEFINE_bool(disable_predictive_realtime_conversion,
            false,
            "disable predictive realtime conversion");

namespace mozc {
namespace {

const size_t kMaxSegmentsSize                   = 256;
const size_t kMaxCharLength                     = 1024;
const size_t kMaxCharLengthForReverseConversion = 600;  // 200 chars in UTF8
const int    kMaxCost                           = 32767;
const int    kMinCost                           = -32767;
const int    kDefaultNumberCost                 = 3000;

class KeyCorrectedNodeListBuilder : public BaseNodeListBuilder {
 public:
  KeyCorrectedNodeListBuilder(size_t pos,
                              StringPiece original_lookup_key,
                              const KeyCorrector *key_corrector,
                              NodeAllocatorInterface *allocator)
      : BaseNodeListBuilder(allocator, allocator->max_nodes_size()),
        pos_(pos),
        original_lookup_key_(original_lookup_key),
        key_corrector_(key_corrector),
        tail_(NULL) {}

  virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                             const Token &token) {
    const size_t offset =
        key_corrector_->GetOriginalOffset(pos_, token.key.size());
    if (!KeyCorrector::IsValidPosition(offset) || offset == 0) {
      return TRAVERSE_NEXT_KEY;
    }
    Node *node = NewNodeFromToken(token);
    node->key.assign(original_lookup_key_.data() + pos_, offset);
    node->wcost += KeyCorrector::GetCorrectedCostPenalty(node->key);

    // Push back |node| to the end.
    if (result_ == NULL) {
      result_ = node;
    } else {
      DCHECK(tail_ != NULL);
      tail_->bnext = node;
    }
    tail_ = node;
    return TRAVERSE_CONTINUE;
  }

  Node *tail() const { return tail_; }

 private:
  const size_t pos_;
  const StringPiece original_lookup_key_;
  const KeyCorrector *key_corrector_;
  Node *tail_;
};

void InsertCorrectedNodes(size_t pos, const string &key,
                          const ConversionRequest &request,
                          const KeyCorrector *key_corrector,
                          const DictionaryInterface *dictionary,
                          Lattice *lattice) {
  if (key_corrector == NULL) {
    return;
  }
  size_t length = 0;
  const char *str = key_corrector->GetCorrectedPrefix(pos, &length);
  if (str == NULL || length == 0) {
    return;
  }
  KeyCorrectedNodeListBuilder builder(pos, key, key_corrector,
                                      lattice->node_allocator());
  dictionary->LookupPrefix(
      StringPiece(str, length),
      request.IsKanaModifierInsensitiveConversion(),
      &builder);
  if (builder.tail() != NULL) {
    builder.tail()->bnext = NULL;
  }
  if (builder.result() != NULL) {
    lattice->Insert(pos, builder.result());
  }
}

bool IsNumber(const char c) {
  return c >= '0' && c <= '9';
}

bool ContainsWhiteSpacesOnly(const StringPiece s) {
  for (ConstChar32Iterator iter(s); !iter.Done(); iter.Next()) {
    switch (iter.Get()) {
      case 0x09:    // TAB
      case 0x20:    // Half-width space
      case 0x3000:  // Full-width space
        break;
      default:
        return false;
    }
  }
  return true;
}

void DecomposeNumberAndSuffix(const string &input,
                              string *number, string *suffix) {
  const char *begin = input.data();
  const char *end = input.data() + input.size();
  size_t pos = 0;
  while (begin < end) {
    if (IsNumber(*begin)) {
      ++pos;
      ++begin;
      continue;
    }
    break;
  }
  number->assign(input, 0, pos);
  suffix->assign(input, pos, input.size() - pos);
}

void DecomposePrefixAndNumber(const string &input,
                              string *prefix, string *number) {
  const char *begin = input.data();
  const char *end = input.data() + input.size() - 1;
  size_t pos = input.size();
  while (begin <= end) {
    if (IsNumber(*end)) {
      --pos;
      --end;
      continue;
    }
    break;
  }
  prefix->assign(input, 0, pos);
  number->assign(input, pos, input.size() - pos);
}

void NormalizeHistorySegments(Segments *segments) {
  for (size_t i = 0; i < segments->history_segments_size(); ++i) {
    Segment *segment = segments->mutable_history_segment(i);
    if (segment == NULL || segment->candidates_size() == 0) {
      continue;
    }

    string key;
    Segment::Candidate *c = segment->mutable_candidate(0);
    const string value = c->value;
    const string content_value = c->content_value;
    const string content_key = c->content_key;
    Util::FullWidthAsciiToHalfWidthAscii(segment->key(), &key);
    Util::FullWidthAsciiToHalfWidthAscii(value, &c->value);
    Util::FullWidthAsciiToHalfWidthAscii(content_value, &c->content_value);
    Util::FullWidthAsciiToHalfWidthAscii(content_key, &c->content_key);
    c->key = key;
    segment->set_key(key);

    // Ad-hoc rewrite for Numbers
    // Since number candidate is generative, i.e, any number can be
    // written by users, we normalize the value here. normalzied number
    // is used for the ranking tweaking based on history
    if (key.size() > 1 &&
        key == c->value &&
        key == c->content_value &&
        key == c->key &&
        key == c->content_key &&
        Util::GetScriptType(key) == Util::NUMBER &&
        IsNumber(key[key.size() - 1])) {
      key = key[key.size() - 1];  // use the last digit only
      segment->set_key(key);
      c->value = key;
      c->content_value = key;
      c->content_key = key;
    }
  }
}

Lattice *GetLattice(Segments *segments, bool is_prediction) {
  Lattice *lattice = segments->mutable_cached_lattice();
  if (lattice == NULL) {
    return NULL;
  }

  const size_t history_segments_size = segments->history_segments_size();

  string history_key = "";
  for (size_t i = 0; i < history_segments_size; ++i) {
    history_key.append(segments->segment(i).key());
  }
  string conversion_key = "";
  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
    conversion_key.append(segments->segment(i).key());
  }

  const size_t lattice_history_end_pos = lattice->history_end_pos();

  if (!is_prediction ||
      FLAGS_disable_lattice_cache ||
      Util::CharsLen(conversion_key) <= 1 ||
      lattice_history_end_pos != history_key.size()) {
    // Do not cache if conversion is not prediction, or disable_lattice_cache
    // flag is used.  In addition, if a user input the key right after the
    // finish of conversion, reset the lattice to erase old nodes.
    // Even if the lattice key is not changed, we should reset the lattice
    // when the history size is changed.
    // When we submit the candidate partially, the entire key will not changed,
    // but the history position will be changed.
    lattice->Clear();
  }

  return lattice;
}

}  // namespace

ImmutableConverterImpl::ImmutableConverterImpl(
    const DictionaryInterface *dictionary,
    const DictionaryInterface *suffix_dictionary,
    const SuppressionDictionary *suppression_dictionary,
    const ConnectorInterface *connector,
    const SegmenterInterface *segmenter,
    const POSMatcher *pos_matcher,
    const PosGroup *pos_group,
    const SuggestionFilter *suggestion_filter)
    : dictionary_(dictionary),
      suffix_dictionary_(suffix_dictionary),
      suppression_dictionary_(suppression_dictionary),
      connector_(connector),
      segmenter_(segmenter),
      pos_matcher_(pos_matcher),
      pos_group_(pos_group),
      suggestion_filter_(suggestion_filter),
      first_name_id_(pos_matcher_->GetFirstNameId()),
      last_name_id_(pos_matcher_->GetLastNameId()),
      number_id_(pos_matcher_->GetNumberId()),
      unknown_id_(pos_matcher_->GetUnknownId()),
      last_to_first_name_transition_cost_(
          connector_->GetTransitionCost(last_name_id_, first_name_id_)) {
  DCHECK(dictionary_);
  DCHECK(suffix_dictionary_);
  DCHECK(suppression_dictionary_);
  DCHECK(connector_);
  DCHECK(segmenter_);
  DCHECK(pos_matcher_);
  DCHECK(pos_group_);
  DCHECK(suggestion_filter_);
}

void ImmutableConverterImpl::ExpandCandidates(
    const string &original_key, NBestGenerator *nbest, Segment *segment,
    Segments::RequestType request_type, size_t expand_size) const {
  DCHECK(nbest);
  DCHECK(segment);
  CHECK_GT(expand_size, 0);

  while (segment->candidates_size() < expand_size) {
    Segment::Candidate *candidate = segment->push_back_candidate();
    DCHECK(candidate);
    candidate->Init();

    // if NBestGenerator::Next() returns NULL,
    // no more entries are generated.
    if (!nbest->Next(original_key, candidate, request_type)) {
      segment->pop_back_candidate();
      break;
    }
  }
}

void ImmutableConverterImpl::InsertDummyCandidates(Segment *segment,
                                                   size_t expand_size) const {
  const Segment::Candidate *top_candidate =
      segment->candidates_size() == 0 ? NULL :
      segment->mutable_candidate(0);
  const Segment::Candidate *last_candidate =
      segment->candidates_size() == 0 ? NULL :
      segment->mutable_candidate(segment->candidates_size() - 1);

  // Insert a dummy candiate whose content_value is katakana.
  // If functional_key() is empty, no need to make a dummy candidate.
  if (segment->candidates_size() > 0 &&
      segment->candidates_size() < expand_size &&
      !segment->candidate(0).functional_key().empty() &&
      Util::GetScriptType(segment->candidate(0).content_key) ==
      Util::HIRAGANA) {
    // Use last_candidate as a refernce of cost.
    // Use top_candidate as a refarence of lid/rid and key/value.
    DCHECK(top_candidate);
    DCHECK(last_candidate);
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);

    string katakana_value;
    Util::HiraganaToKatakana(segment->candidate(0).content_key,
                             &katakana_value);

    new_candidate->CopyFrom(*top_candidate);
    new_candidate->value = katakana_value + top_candidate->functional_value();
    new_candidate->content_value = katakana_value;
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->attributes = 0;
    // We cannot copy inner_segment_boundary; see b/8109381.
    new_candidate->inner_segment_boundary.clear();
    DCHECK(new_candidate->IsValid());
    last_candidate = new_candidate;
  }

  // Insert a dummy hiragana candidate.
  if (segment->candidates_size() == 0 ||
      (segment->candidates_size() < expand_size &&
       Util::GetScriptType(segment->key()) == Util::HIRAGANA)) {
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);

    if (last_candidate != NULL) {
      new_candidate->CopyFrom(*last_candidate);
      // We cannot copy inner_segment_boundary; see b/8109381.
      new_candidate->inner_segment_boundary.clear();
    } else {
      new_candidate->Init();
    }
    new_candidate->key = segment->key();
    new_candidate->value = segment->key();
    new_candidate->content_key = segment->key();
    new_candidate->content_value = segment->key();
    if (last_candidate != NULL) {
      new_candidate->cost = last_candidate->cost + 1;
      new_candidate->wcost = last_candidate->wcost + 1;
      new_candidate->structure_cost = last_candidate->structure_cost + 1;
    }
    new_candidate->attributes = 0;
    last_candidate = new_candidate;
    // One character hiragana/katakana will cause side effect.
    // Type "し" and choose "シ". After that, "しました" will become "シました".
    if (Util::CharsLen(new_candidate->key) <= 1) {
      new_candidate->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    }
    DCHECK(new_candidate->IsValid());
  }

  // Insert a dummy katakana candidate.
  string katakana_value;
  Util::HiraganaToKatakana(segment->key(), &katakana_value);
  if (segment->candidates_size() > 0 &&
      segment->candidates_size() < expand_size &&
      Util::GetScriptType(katakana_value) == Util::KATAKANA) {
    Segment::Candidate *new_candidate = segment->add_candidate();
    DCHECK(new_candidate);
    DCHECK(last_candidate);
    new_candidate->Init();
    new_candidate->key = segment->key();
    new_candidate->value = katakana_value;
    new_candidate->content_key = segment->key();
    new_candidate->content_value = katakana_value;
    new_candidate->cost = last_candidate->cost + 1;
    new_candidate->wcost = last_candidate->wcost + 1;
    new_candidate->structure_cost = last_candidate->structure_cost + 1;
    new_candidate->lid = last_candidate->lid;
    new_candidate->rid = last_candidate->rid;
    if (Util::CharsLen(new_candidate->key) <= 1) {
      new_candidate->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    }
    DCHECK(new_candidate->IsValid());
  }

  DCHECK_GT(segment->candidates_size(), 0);
}

void ImmutableConverterImpl::ApplyResegmentRules(
    size_t pos, Lattice *lattice) const {
  if (ResegmentArabicNumberAndSuffix(pos, lattice)) {
    VLOG(1) << "ResegmentArabicNumberAndSuffix returned true";
    return;
  }

  if (ResegmentPrefixAndArabicNumber(pos, lattice)) {
    VLOG(1) << "ResegmentArabicNumberAndSuffix returned true";
    return;
  }

  if (ResegmentPersonalName(pos, lattice)) {
    VLOG(1) << "ResegmentPersonalName returned true";
    return;
  }
}

// Currently, only arabic_number + suffix patterns are resegmented
// TODO(taku): consider kanji number into consideration
bool ImmutableConverterImpl::ResegmentArabicNumberAndSuffix(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == NULL) {
    VLOG(1) << "bnode is NULL";
    return false;
  }

  bool modified = false;

  for (const Node *compound_node = bnode;
       compound_node != NULL; compound_node = compound_node->bnext) {
    if (!compound_node->value.empty() && !compound_node->key.empty() &&
        pos_matcher_->IsNumber(compound_node->lid) &&
        !pos_matcher_->IsNumber(compound_node->rid) &&
        IsNumber(compound_node->value[0]) && IsNumber(compound_node->key[0])) {
      string number_value, number_key;
      string suffix_value, suffix_key;
      DecomposeNumberAndSuffix(compound_node->value,
                               &number_value, &suffix_value);
      DecomposeNumberAndSuffix(compound_node->key,
                               &number_key, &suffix_key);

      if (suffix_value.empty() || suffix_key.empty()) {
        continue;
      }

      // not compatibile
      if (number_value != number_key) {
        LOG(WARNING) << "Incompatible key/value number pair";
        continue;
      }

      // do -1 so that resegmented nodes are boosted
      // over compound node.
      const int32 wcost = max(compound_node->wcost / 2 - 1, 0);

      Node *number_node = lattice->NewNode();
      CHECK(number_node);
      number_node->key = number_key;
      number_node->value = number_value;
      number_node->lid = compound_node->lid;
      number_node->rid = 0;   // 0 to 0 transition cost is 0
      number_node->wcost = wcost;
      number_node->node_type = Node::NOR_NODE;
      number_node->bnext = NULL;

      // insert number into the lattice
      lattice->Insert(pos, number_node);

      Node *suffix_node = lattice->NewNode();
      CHECK(suffix_node);
      suffix_node->key = suffix_key;
      suffix_node->value = suffix_value;
      suffix_node->lid = 0;
      suffix_node->rid = compound_node->rid;
      suffix_node->wcost = wcost;
      suffix_node->node_type = Node::NOR_NODE;
      suffix_node->bnext = NULL;

      suffix_node->constrained_prev = number_node;

      // insert suffix into the lattice
      lattice->Insert(pos + number_node->key.size(), suffix_node);
      VLOG(1) << "Resegmented: " << compound_node->value << " "
              << number_node->value << " " << suffix_node->value;

      modified = true;
    }
  }

  return modified;
}

bool ImmutableConverterImpl::ResegmentPrefixAndArabicNumber(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == NULL) {
    VLOG(1) << "bnode is NULL";
    return false;
  }

  bool modified = false;

  for (const Node *compound_node = bnode;
       compound_node != NULL; compound_node = compound_node->bnext) {
    // Unlike ResegmentArabicNumberAndSuffix, we don't
    // check POS as words ending with Arabic numbers are pretty rare.
    if (compound_node->value.size() > 1 && compound_node->key.size() > 1 &&
        !IsNumber(compound_node->value[0]) &&
        !IsNumber(compound_node->key[0]) &&
        IsNumber(compound_node->value[compound_node->value.size() - 1]) &&
        IsNumber(compound_node->key[compound_node->key.size() - 1])) {
      string number_value, number_key;
      string prefix_value, prefix_key;
      DecomposePrefixAndNumber(compound_node->value,
                               &prefix_value, &number_value);
      DecomposePrefixAndNumber(compound_node->key,
                               &prefix_key, &number_key);

      if (prefix_value.empty() || prefix_key.empty()) {
        continue;
      }

      // not compatibile
      if (number_value != number_key) {
        LOG(WARNING) << "Incompatible key/value number pair";
        continue;
      }

      // do -1 so that resegmented nodes are boosted
      // over compound node.
      const int32 wcost = max(compound_node->wcost / 2 - 1, 0);

      Node *prefix_node = lattice->NewNode();
      CHECK(prefix_node);
      prefix_node->key = prefix_key;
      prefix_node->value = prefix_value;
      prefix_node->lid = compound_node->lid;
      prefix_node->rid = 0;   // 0 to 0 transition cost is 0
      prefix_node->wcost = wcost;
      prefix_node->node_type = Node::NOR_NODE;
      prefix_node->bnext = NULL;

      // insert number into the lattice
      lattice->Insert(pos, prefix_node);

      Node *number_node = lattice->NewNode();
      CHECK(number_node);
      number_node->key = number_key;
      number_node->value = number_value;
      number_node->lid = 0;
      number_node->rid = compound_node->rid;
      number_node->wcost = wcost;
      number_node->node_type = Node::NOR_NODE;
      number_node->bnext = NULL;

      number_node->constrained_prev = prefix_node;

      // insert number into the lattice
      lattice->Insert(pos + prefix_node->key.size(), number_node);
      VLOG(1) << "Resegmented: " << compound_node->value << " "
              << prefix_node->value << " " << number_node->value;

      modified = true;
    }
  }

  return modified;
}

bool ImmutableConverterImpl::ResegmentPersonalName(
    size_t pos, Lattice *lattice) const {
  const Node *bnode = lattice->begin_nodes(pos);
  if (bnode == NULL) {
    VLOG(1) << "bnode is NULL";
    return false;
  }

  bool modified = false;

  // find a combination of last_name and first_name, e.g. "田中麗奈".
  for (const Node *compound_node = bnode;
       compound_node != NULL; compound_node = compound_node->bnext) {
    // left word is last name and right word is first name
    if (compound_node->lid != last_name_id_ ||
        compound_node->rid != first_name_id_) {
      continue;
    }

    const size_t len = Util::CharsLen(compound_node->value);

    // Don't resegment one-word last_name/first_name like 林健,
    // as it would deliver side effect.
    if (len <= 2) {
      continue;
    }

    // Don't resegment if the value is katakana
    if (Util::GetScriptType(compound_node->value) == Util::KATAKANA) {
      continue;
    }

    // Do constrained Viterbi search inside the compound "田中麗奈".
    // Constraints:
    // 1. Concats of last_name and first_name should be "田中麗奈"
    // 2. consisting of two words (last_name and first_name)
    // 3. Segment-boundary exist between the two words.
    // 4.a Either (a) POS of lnode is last_name or (b) POS of rnode is fist_name
    //     (len >= 4)
    // 4.b Both (a) POS of lnode is last_name and (b) POS of rnode is fist_name
    //     (len == 3)
    const Node *best_last_name_node = NULL;
    const Node *best_first_name_node = NULL;
    int best_cost = 0x7FFFFFFF;
    for (const Node *lnode = bnode; lnode != NULL; lnode = lnode->bnext) {
      // lnode(last_name) is a prefix of compound, Constraint 1.
      if (compound_node->value.size() > lnode->value.size() &&
          compound_node->key.size() > lnode->key.size() &&
          Util::StartsWith(compound_node->value, lnode->value)) {
        // rnode(first_name) is a suffix of compound, Constraint 1.
        for (const Node *rnode = lattice->begin_nodes(pos + lnode->key.size());
             rnode != NULL; rnode = rnode->bnext) {
          if ((lnode->value.size() + rnode->value.size())
              == compound_node->value.size() &&
              (lnode->value + rnode->value) == compound_node->value &&
              segmenter_->IsBoundary(lnode, rnode, false)) {   // Constraint 3.
            const int32 cost = lnode->wcost + GetCost(lnode, rnode);
            if (cost < best_cost) {   // choose the smallest ones
              best_last_name_node = lnode;
              best_first_name_node = rnode;
              best_cost = cost;
            }
          }
        }
      }
    }

    // No valid first/last names are found
    if (best_first_name_node == NULL || best_last_name_node == NULL) {
      continue;
    }

    // Constraint 4.a
    if (len >= 4 &&
        (best_last_name_node->lid != last_name_id_ &&
         best_first_name_node->rid != first_name_id_)) {
      continue;
    }

    // Constraint 4.b
    if (len == 3 &&
        (best_last_name_node->lid != last_name_id_ ||
         best_first_name_node->rid != first_name_id_)) {
      continue;
    }

    // Insert LastName and FirstName as independent nodes.
    // Duplications will be removed in nbest enumerations.
    // word costs are calculated from compound node by assuming that
    // transition cost is 0.
    //
    // last_name_cost + transition_cost + first_name_cost == compound_cost
    // last_name_cost == first_name_cost
    // i.e,
    // last_name_cost = first_name_cost =
    // (compound_cost - transition_cost) / 2;
    const int32 wcost = (compound_node->wcost -
                         last_to_first_name_transition_cost_) / 2;

    Node *last_name_node = lattice->NewNode();
    CHECK(last_name_node);
    last_name_node->key = best_last_name_node->key;
    last_name_node->value = best_last_name_node->value;
    last_name_node->lid = compound_node->lid;
    last_name_node->rid = last_name_id_;
    last_name_node->wcost = wcost;
    last_name_node->node_type = Node::NOR_NODE;
    last_name_node->bnext = NULL;

    // insert last_name into the lattice
    lattice->Insert(pos, last_name_node);

    Node *first_name_node = lattice->NewNode();
    CHECK(first_name_node);
    first_name_node->key = best_first_name_node->key;
    first_name_node->value = best_first_name_node->value;
    first_name_node->lid = first_name_id_;
    first_name_node->rid = compound_node->rid;
    first_name_node->wcost = wcost;
    first_name_node->node_type = Node::NOR_NODE;
    first_name_node->bnext = NULL;

    first_name_node->constrained_prev = last_name_node;

    // insert first_name into the lattice
    lattice->Insert(pos + last_name_node->key.size(), first_name_node);

    VLOG(2) << "Resegmented: " << compound_node->value << " "
            << last_name_node->value << " " << first_name_node->value;

    modified = true;
  }

  return modified;
}

namespace {

class NodeListBuilderWithCacheEnabled : public NodeListBuilderForLookupPrefix {
 public:
  NodeListBuilderWithCacheEnabled(NodeAllocatorInterface *allocator,
                                  size_t min_key_length)
      : NodeListBuilderForLookupPrefix(allocator,
                                       allocator->max_nodes_size(),
                                       min_key_length) {
    DCHECK(allocator);
  }

  virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                             const Token &token) {
    Node *node = NewNodeFromToken(token);
    node->attributes |= Node::ENABLE_CACHE;
    node->raw_wcost = node->wcost;
    PrependNode(node);
    return (limit_ <= 0) ? TRAVERSE_DONE : TRAVERSE_CONTINUE;
  }
};

}  // namespace

Node *ImmutableConverterImpl::Lookup(const int begin_pos,
                                     const int end_pos,
                                     const ConversionRequest &request,
                                     bool is_reverse,
                                     bool is_prediction,
                                     Lattice *lattice) const {
  CHECK_LE(begin_pos, end_pos);
  const char *begin = lattice->key().data() + begin_pos;
  const char *end = lattice->key().data() + end_pos;
  const size_t len = end_pos - begin_pos;

  lattice->node_allocator()->set_max_nodes_size(8192);
  Node *result_node = NULL;
  if (is_reverse) {
    BaseNodeListBuilder builder(
        lattice->node_allocator(),
        lattice->node_allocator()->max_nodes_size());
    dictionary_->LookupReverse(
        StringPiece(begin, len), lattice->node_allocator(), &builder);
    result_node = builder.result();
  } else {
    if (is_prediction && !FLAGS_disable_lattice_cache) {
      NodeListBuilderWithCacheEnabled builder(
          lattice->node_allocator(),
          lattice->cache_info(begin_pos) + 1);
      dictionary_->LookupPrefix(
          StringPiece(begin, len),
          request.IsKanaModifierInsensitiveConversion(),
          &builder);
      result_node = builder.result();
      lattice->SetCacheInfo(begin_pos, len);
    } else {
      // When cache feature is not used, look up normally
      BaseNodeListBuilder builder(
          lattice->node_allocator(),
          lattice->node_allocator()->max_nodes_size());
      dictionary_->LookupPrefix(
          StringPiece(begin, len),
          request.IsKanaModifierInsensitiveConversion(),
          &builder);
      result_node = builder.result();
    }
  }
  return AddCharacterTypeBasedNodes(begin, end, lattice, result_node);
}

Node *ImmutableConverterImpl::AddCharacterTypeBasedNodes(
    const char *begin, const char *end, Lattice *lattice, Node *nodes) const {

  size_t mblen = 0;
  const char32 ucs4 = Util::UTF8ToUCS4(begin, end, &mblen);

  const Util::ScriptType first_script_type = Util::GetScriptType(ucs4);
  const Util::FormType first_form_type = Util::GetFormType(ucs4);

  // Add 1 character node. It can be either UnknownId or NumberId.
  {
    Node *new_node = lattice->NewNode();
    CHECK(new_node);
    if (first_script_type == Util::NUMBER) {
      new_node->lid = number_id_;
      new_node->rid = number_id_;
    } else {
      new_node->lid = unknown_id_;
      new_node->rid = unknown_id_;
    }

    new_node->wcost = kMaxCost;
    new_node->value.assign(begin, mblen);
    new_node->key.assign(begin, mblen);
    new_node->node_type = Node::NOR_NODE;
    new_node->bnext = nodes;
    nodes = new_node;
  }  // scope out |new_node|

  if (first_script_type == Util::NUMBER) {
    nodes->wcost = kDefaultNumberCost;
    return nodes;
  }

  if (first_script_type != Util::ALPHABET &&
      first_script_type != Util::KATAKANA) {
    return nodes;
  }

  // group by same char type
  int num_char = 1;
  const char *p = begin + mblen;
  while (p < end) {
    const char32 next_ucs4 = Util::UTF8ToUCS4(p, end, &mblen);
    if (first_script_type != Util::GetScriptType(next_ucs4) ||
        first_form_type != Util::GetFormType(next_ucs4)) {
      break;
    }
    p += mblen;
    ++num_char;
  }

  if (num_char > 1) {
    mblen = static_cast<uint32>(p - begin);
    Node *new_node = lattice->NewNode();
    CHECK(new_node);
    if (first_script_type == Util::NUMBER) {
      new_node->lid = number_id_;
      new_node->rid = number_id_;
    } else {
      new_node->lid = unknown_id_;
      new_node->rid = unknown_id_;
    }
    new_node->wcost = kMaxCost / 2;
    new_node->value.assign(begin, mblen);
    new_node->key.assign(begin, mblen);
    new_node->node_type = Node::NOR_NODE;
    new_node->bnext = nodes;
    nodes = new_node;
  }

  return nodes;
}

namespace {

// Reasonably big cost. Cannot use INT_MAX because a new cost will be
// calculated based on kVeryBigCost.
const int kVeryBigCost = (INT_MAX >> 2);

// Runs viterbi algorithm at position |pos|. The left_boundary/right_boundary
// are the next boundary looked from pos. (If pos is on the boundary,
// left_boundary should be the previous one, and right_boundary should be
// the next).
inline void ViterbiInternal(
    const ConnectorInterface &connector, size_t pos, size_t right_boundary,
    Lattice *lattice) {
  for (Node *rnode = lattice->begin_nodes(pos);
       rnode != NULL; rnode = rnode->bnext) {
    if (rnode->end_pos > right_boundary) {
      // Invalid rnode.
      rnode->prev = NULL;
      continue;
    }

    if (rnode->constrained_prev != NULL) {
      // Constrained node.
      if (rnode->constrained_prev->prev == NULL) {
        rnode->prev = NULL;
      } else {
        rnode->prev = rnode->constrained_prev;
        rnode->cost =
            rnode->prev->cost +
            rnode->wcost +
            connector.GetTransitionCost(rnode->prev->rid, rnode->lid);
      }
      continue;
    }

    // Find a valid node which connects to the rnode with minimum cost.
    int best_cost = kVeryBigCost;
    Node *best_node = NULL;
    for (Node *lnode = lattice->end_nodes(pos);
         lnode != NULL; lnode = lnode->enext) {
      if (lnode->prev == NULL) {
        // Invalid lnode.
        continue;
      }

      int cost =
          lnode->cost + connector.GetTransitionCost(lnode->rid, rnode->lid);
      if (cost < best_cost) {
        best_cost = cost;
        best_node = lnode;
      }
    }

    rnode->prev = best_node;
    rnode->cost = best_cost + rnode->wcost;
  }
}
}  // namespace

bool ImmutableConverterImpl::Viterbi(
    const Segments &segments, Lattice *lattice) const {
  const string &key = lattice->key();

  // Process BOS.
  {
    Node *bos_node = lattice->bos_nodes();
    // Ensure only one bos node is available.
    DCHECK(bos_node != NULL);
    DCHECK(bos_node->enext == NULL);

    const size_t right_boundary = segments.segment(0).key().size();
    for (Node *rnode = lattice->begin_nodes(0);
         rnode != NULL; rnode = rnode->bnext) {
      if (rnode->end_pos > right_boundary) {
        // Invalid rnode.
        continue;
      }

      // Ensure no constraint.
      DCHECK(rnode->constrained_prev == NULL);

      rnode->prev = bos_node;
      rnode->cost =
          bos_node->cost +
          connector_->GetTransitionCost(bos_node->rid, rnode->lid) +
          rnode->wcost;
    }
  }

  size_t left_boundary = 0;
  const size_t segments_size = segments.segments_size();

  // Specialization for the first segment.
  // Don't run on the left boundary (the connection with BOS node),
  // beacuse it is already run above.
  {
    const size_t right_boundary =
        left_boundary + segments.segment(0).key().size();
    for (size_t pos = left_boundary + 1; pos < right_boundary; ++pos) {
      ViterbiInternal(*connector_, pos, right_boundary, lattice);
    }
    left_boundary = right_boundary;
  }

  // The condition to break is in the loop.
  for (size_t i = 1; i < segments_size; ++i) {
    // Run Viterbi for each position the segment.
    const size_t right_boundary =
        left_boundary + segments.segment(i).key().size();
    for (size_t pos = left_boundary; pos < right_boundary; ++pos) {
      ViterbiInternal(*connector_, pos, right_boundary, lattice);
    }
    left_boundary = right_boundary;
  }

  // Process EOS.
  {
    Node *eos_node = lattice->eos_nodes();

    // Ensure only one eos node.
    DCHECK(eos_node != NULL);
    DCHECK(eos_node->bnext == NULL);

    // No constrained prev.
    DCHECK(eos_node->constrained_prev == NULL);

    left_boundary =
        key.size() - segments.segment(segments_size - 1).key().size();
    // Find a valid node which connects to the rnode with minimum cost.
    int best_cost = kVeryBigCost;
    Node *best_node = NULL;
    for (Node *lnode = lattice->end_nodes(key.size());
         lnode != NULL; lnode = lnode->enext) {
      if (lnode->prev == NULL) {
        // Invalid lnode.
        continue;
      }

      int cost =
          lnode->cost +
          connector_->GetTransitionCost(lnode->rid, eos_node->lid);
      if (cost < best_cost) {
        best_cost = cost;
        best_node = lnode;
      }
    }

    eos_node->prev = best_node;
    eos_node->cost = best_cost + eos_node->wcost;
  }

  // Traverse the node from end to begin.
  Node *node = lattice->eos_nodes();
  CHECK(node->bnext == NULL);
  Node *prev = NULL;
  while (node->prev != NULL) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (lattice->bos_nodes() != prev) {
    LOG(WARNING) << "cannot make lattice";
    return false;
  }

  return true;
}

// faster Viterbi algorithm for prediction
//
// Run simple Viterbi algorithm with contracting the same lid and rid.
// Because the original Viterbi has speciall nodes, we should take it
// consideration.
// 1. CONNECTED nodes: are normal nodes.
// 2. WEAK_CONNECTED nodes: don't occur in prediction, so we do not have to
//    consider about them.
// 3. NOT_CONNECTED nodes: occur when they are between history nodes and
//    normal nodes.
// For NOT_CONNECTED nodes, we should run Viterbi for history nodes first,
// and do it for normal nodes second. The function "PredictionViterbiSub"
// runs Viterbi for positions between calc_begin_pos and calc_end_pos,
// inclusive.
//
// We cannot apply this function in suggestion because in suggestion there are
// WEAK_CONNECTED nodes and this function is not designed for them.
//
// TODO(toshiyuki): We may be able to use faster viterbi for
// conversion/suggestion if we use richer info as contraction group.

bool ImmutableConverterImpl::PredictionViterbi(
    const Segments &segments, Lattice *lattice) const {
  const size_t key_length = lattice->key().size();
  const size_t history_segments_size = segments.history_segments_size();
  size_t history_length = 0;
  for (size_t i = 0; i < history_segments_size; ++i) {
    history_length += segments.segment(i).key().size();
  }
  PredictionViterbiInternal(0, history_length, lattice);
  PredictionViterbiInternal(history_length, key_length, lattice);

  Node *node = lattice->eos_nodes();
  CHECK(node->bnext == NULL);
  Node *prev = NULL;
  while (node->prev != NULL) {
    prev = node->prev;
    prev->next = node;
    node = prev;
  }

  if (lattice->bos_nodes() != prev) {
    LOG(WARNING) << "cannot make lattice";
    return false;
  }

  return true;
}

void ImmutableConverterImpl::PredictionViterbiInternal(
    int calc_begin_pos, int calc_end_pos, Lattice *lattice) const {
  CHECK_LE(calc_begin_pos, calc_end_pos);

  // Mapping from lnode's rid to (cost, Node) of best way/cost, and vice versa.
  // Note that, the average number of lid/rid variation is less than 30 in
  // most cases. So, in order to avoid too many allocations for internal
  // nodes of std::map, we use vector of key-value pairs.
  typedef vector<pair<int, pair<int, Node*> > > BestMap;
  typedef OrderBy<FirstKey, Less> OrderByFirst;
  BestMap lbest, rbest;
  lbest.reserve(128);
  rbest.reserve(128);

  const pair<int, Node*> kInvalidValue(INT_MAX, static_cast<Node*>(NULL));

  for (size_t pos = calc_begin_pos; pos <= calc_end_pos; ++pos) {
    lbest.clear();
    for (Node *lnode = lattice->end_nodes(pos);
         lnode != NULL; lnode = lnode->enext) {
      const int rid = lnode->rid;
      BestMap::value_type key(rid, kInvalidValue);
      BestMap::iterator iter =
          lower_bound(lbest.begin(), lbest.end(), key, OrderByFirst());
      if (iter == lbest.end() || iter->first != rid) {
        lbest.insert(
            iter, BestMap::value_type(rid, make_pair(lnode->cost, lnode)));
      } else if (lnode->cost < iter->second.first) {
        iter->second.first = lnode->cost;
        iter->second.second = lnode;
      }
    }

    if (lbest.empty()) {
      continue;
    }

    rbest.clear();
    Node *rnode_begin = lattice->begin_nodes(pos);
    for (Node *rnode = rnode_begin; rnode != NULL; rnode = rnode->bnext) {
      if (rnode->end_pos > calc_end_pos) {
        continue;
      }
      BestMap::value_type key(rnode->lid, kInvalidValue);
      BestMap::iterator iter =
          lower_bound(rbest.begin(), rbest.end(), key, OrderByFirst());
      if (iter == rbest.end() || iter->first != rnode->lid) {
        rbest.insert(iter, key);
      }
    }

    if (rbest.empty()) {
      continue;
    }

    for (BestMap::iterator liter = lbest.begin();
         liter != lbest.end(); ++liter) {
      for (BestMap::iterator riter = rbest.begin();
           riter != rbest.end(); ++riter) {
        const int cost = liter->second.first +
            connector_->GetTransitionCost(liter->first, riter->first);
        if (cost < riter->second.first) {
          riter->second.first = cost;
          riter->second.second = liter->second.second;
        }
      }
    }

    for (Node *rnode = rnode_begin; rnode != NULL; rnode = rnode->bnext) {
      if (rnode->end_pos > calc_end_pos) {
        continue;
      }
      BestMap::value_type key(rnode->lid, kInvalidValue);
      BestMap::iterator iter =
          lower_bound(rbest.begin(), rbest.end(), key, OrderByFirst());
      if (iter == rbest.end() || iter->first != rnode->lid ||
          iter->second.second == NULL) {
        continue;
      }

      rnode->cost = iter->second.first + rnode->wcost;
      rnode->prev = iter->second.second;
    }
  }
}

namespace {

// Adds penalty for predictive nodes when building a node list.
class NodeListBuilderForPredictiveNodes : public BaseNodeListBuilder {
 public:
  NodeListBuilderForPredictiveNodes(NodeAllocatorInterface *allocator,
                                    int limit, const POSMatcher *pos_matcher)
      : BaseNodeListBuilder(allocator, limit), pos_matcher_(pos_matcher) {}

  virtual ~NodeListBuilderForPredictiveNodes() {}

  virtual ResultType OnToken(StringPiece key, StringPiece actual_key,
                             const Token &token) {
    Node *node = NewNodeFromToken(token);
    const int kPredictiveNodeDefaultPenalty = 900;  // ~= -500 * log(1/6)
    int additional_cost = kPredictiveNodeDefaultPenalty;

    // Bonus for suffix word.
    if (pos_matcher_->IsSuffixWord(node->rid) &&
        pos_matcher_->IsSuffixWord(node->lid)) {
      const int kSuffixWordBonus = 700;
      additional_cost -= kSuffixWordBonus;
    }

    // Penalty for unique noun word.
    if (pos_matcher_->IsUniqueNoun(node->rid) ||
        pos_matcher_->IsUniqueNoun(node->lid)) {
      const int kUniqueNounPenalty = 500;
      additional_cost += kUniqueNounPenalty;
    }

    // Penalty for number.
    if (pos_matcher_->IsNumber(node->rid) ||
        pos_matcher_->IsNumber(node->lid)) {
      const int kNumberPenalty = 4000;
      additional_cost += kNumberPenalty;
    }

    node->wcost += additional_cost;
    PrependNode(node);
    return (limit_ <= 0) ? TRAVERSE_DONE : TRAVERSE_CONTINUE;
  }

 private:
  const POSMatcher *pos_matcher_;
};

}  // namespace

// Add predictive nodes from conversion key.
void ImmutableConverterImpl::MakeLatticeNodesForPredictiveNodes(
    const Segments &segments, const ConversionRequest &request,
    Lattice *lattice) const {
  const string &key = lattice->key();
  string conversion_key;
  for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
    conversion_key += segments.conversion_segment(i).key();
  }
  DCHECK_NE(string::npos, key.find(conversion_key));
  vector<string> conversion_key_chars;
  Util::SplitStringToUtf8Chars(conversion_key, &conversion_key_chars);

  // do nothing if the conversion key is short
  const size_t kKeyMinLength = 7;
  if (conversion_key_chars.size() < kKeyMinLength) {
    return;
  }

  // Predictive search from suffix dictionary.
  // (search words with between 1 and 6 characters)
  {
    const size_t kMaxSuffixLookupKey = 6;
    const size_t max_sufffix_len =
        min(kMaxSuffixLookupKey, conversion_key_chars.size());
    size_t pos = key.size();

    for (size_t suffix_len = 1; suffix_len <= max_sufffix_len; ++suffix_len) {
      pos -= conversion_key_chars[
          conversion_key_chars.size() - suffix_len].size();
      DCHECK_GE(key.size(), pos);
      NodeListBuilderForPredictiveNodes builder(
          lattice->node_allocator(),
          lattice->node_allocator()->max_nodes_size(),
          pos_matcher_);
      suffix_dictionary_->LookupPredictive(
          StringPiece(key.data() + pos, key.size() - pos),
          request.IsKanaModifierInsensitiveConversion(), &builder);
      if (builder.result() != NULL) {
        lattice->Insert(pos, builder.result());
      }
    }
  }

  // Predictive search from system dictionary.
  // (search words with between 5 and 8 characters)
  {
    const size_t kMinSystemLookupKey = 5;
    const size_t kMaxSystemLookupKey = 8;
    const size_t max_suffix_len =
        min(kMaxSystemLookupKey, conversion_key_chars.size());
    size_t pos = key.size();
    for (size_t suffix_len = 1; suffix_len <= max_suffix_len; ++suffix_len) {
      pos -= conversion_key_chars[
          conversion_key_chars.size() - suffix_len].size();
      DCHECK_GE(key.size(), pos);

      if (suffix_len < kMinSystemLookupKey) {
        // Just update |pos|.
        continue;
      }

      NodeListBuilderForPredictiveNodes builder(
          lattice->node_allocator(),
          lattice->node_allocator()->max_nodes_size(),
          pos_matcher_);
      dictionary_->LookupPredictive(
          StringPiece(key.data() + pos, key.size() - pos),
          request.IsKanaModifierInsensitiveConversion(), &builder);
      if (builder.result() != NULL) {
        lattice->Insert(pos, builder.result());
      }
    }
  }
}

bool ImmutableConverterImpl::MakeLattice(
    const ConversionRequest &request,
    Segments *segments, Lattice *lattice) const {
  if (segments == NULL) {
    LOG(ERROR) << "Segments is NULL";
    return false;
  }

  if (lattice == NULL) {
    LOG(ERROR) << "Lattice is NULL";
    return false;
  }

  if (segments->segments_size() >= kMaxSegmentsSize) {
    LOG(WARNING) << "too many segments";
    return false;
  }

  NormalizeHistorySegments(segments);

  const bool is_reverse =
      (segments->request_type() == Segments::REVERSE_CONVERSION);

  const bool is_prediction =
      (segments->request_type() == Segments::SUGGESTION ||
       segments->request_type() == Segments::PREDICTION);

  // In suggestion mode, ImmutableConverter will not accept multiple-segments.
  // The result always consists of one segment.
  if ((is_reverse || is_prediction) &&
      (segments->conversion_segments_size() != 1 ||
       segments->conversion_segment(0).segment_type() != Segment::FREE)) {
    LOG(WARNING) << "ImmutableConverter doesn't support constrained requests";
    return false;
  }

  // Make the conversion key.
  string conversion_key;
  const size_t history_segments_size = segments->history_segments_size();
  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
    DCHECK(!segments->segment(i).key().empty());
    conversion_key.append(segments->segment(i).key());
  }
  const size_t max_char_len =
      is_reverse ? kMaxCharLengthForReverseConversion : kMaxCharLength;
  if (conversion_key.empty() || conversion_key.size() >= max_char_len) {
    LOG(WARNING) << "Conversion key is empty or too long: " << conversion_key;
    return false;
  }

  // Make the history key.
  string history_key;
  for (size_t i = 0; i < history_segments_size; ++i) {
    DCHECK(!segments->segment(i).key().empty());
    history_key.append(segments->segment(i).key());
  }
  // Check if the total length (length of history_key + conversion_key) doesn't
  // exceed the maximum key length. If it exceeds the limit, we simply clears
  // such useless history segments, which is acceptable because such cases
  // rarely happen in normal use cases.
  if (history_key.size() + conversion_key.size() >= max_char_len) {
    LOG(WARNING) << "Clear history segments due to the limit of key length.";
    segments->clear_history_segments();
    history_key.clear();
  }

  const string key = history_key + conversion_key;
  lattice->UpdateKey(key);
  lattice->ResetNodeCost();

  if (is_reverse) {
    // Reverse lookup for each prefix string in key is slow with current
    // implementation, so run it for them at once and cache the result.
    dictionary_->PopulateReverseLookupCache(key, lattice->node_allocator());
  }

  bool is_valid_lattice = true;
  // Perform the main part of lattice construction.
  if (!MakeLatticeNodesForHistorySegments(*segments, request, lattice) ||
      lattice->end_nodes(history_key.size()) == NULL) {
    is_valid_lattice = false;
  }

  // Can not apply key corrector to invalid lattice.
  if (is_valid_lattice) {
    MakeLatticeNodesForConversionSegments(
        *segments, request, history_key, lattice);
  }

  if (is_reverse) {
    // No reverse look up will happen afterwards.
    dictionary_->ClearReverseLookupCache(lattice->node_allocator());
  }

  // Predictive real time conversion
  if (is_prediction && !FLAGS_disable_predictive_realtime_conversion) {
    MakeLatticeNodesForPredictiveNodes(*segments, request, lattice);
  }

  if (!is_valid_lattice) {
    // Safely bail out, since reverse look up cache was released already.
    return false;
  }

  if (lattice->end_nodes(key.size()) == NULL) {
    LOG(WARNING) << "cannot build lattice from input";
    return false;
  }

  ApplyPrefixSuffixPenalty(conversion_key, lattice);

  // Re-segment personal-names, numbers ...etc
  const bool is_conversion =
      (segments->request_type() == Segments::CONVERSION);
  if (is_conversion) {
    Resegment(*segments, history_key, conversion_key, lattice);
  }

  return true;
}

bool ImmutableConverterImpl::MakeLatticeNodesForHistorySegments(
    const Segments &segments, const ConversionRequest &request,
    Lattice *lattice) const {
  const bool is_reverse =
     (segments.request_type() == Segments::REVERSE_CONVERSION);
  const size_t history_segments_size = segments.history_segments_size();
  const string &key = lattice->key();

  size_t segments_pos = 0;
  uint16 last_rid = 0;

  for (size_t s = 0; s < history_segments_size; ++s) {
    const Segment &segment = segments.segment(s);
    if (segment.segment_type() != Segment::HISTORY &&
        segment.segment_type() != Segment::SUBMITTED) {
      LOG(WARNING) << "inconsistent history";
      return false;
    }
    if (segment.key().empty()) {
      LOG(WARNING) << "invalid history: key is empty";
      return false;
    }
    const Segment::Candidate &candidate = segment.candidate(0);

    // Add a virtual nodes corresponding to HISTORY segments.
    Node *rnode = lattice->NewNode();
    CHECK(rnode);
    rnode->lid = candidate.lid;
    rnode->rid = candidate.rid;
    rnode->wcost = 0;
    rnode->value = candidate.value;
    rnode->key = segment.key();
    rnode->node_type = Node::HIS_NODE;
    rnode->bnext = NULL;
    lattice->Insert(segments_pos, rnode);

    // For the last history segment,  we also insert a new node having
    // EOS part-of-speech. Viterbi algorithm will find the
    // best path from rnode(context) and rnode2(EOS).
    if (s + 1 == history_segments_size && candidate.rid != 0) {
      Node *rnode2 = lattice->NewNode();
      CHECK(rnode2);
      rnode2->lid = candidate.lid;
      rnode2->rid = 0;   // 0 is BOS/EOS

      // This cost was originally set to 1500.
      // It turned out this penalty was so strong that it caused some
      // undesirable conversions like "の-なまえ" -> "の-な前" etc., so we
      // changed this to 0.
      // Reducing the cost promotes context-unaware conversions, and this may
      // have some unexpected side effects.
      // TODO(team): Figure out a better way to set the cost using
      // boundary.def-like approach.
      rnode2->wcost = 0;
      rnode2->value = candidate.value;
      rnode2->key = segment.key();
      rnode2->node_type = Node::HIS_NODE;
      rnode2->bnext = NULL;
      lattice->Insert(segments_pos, rnode2);
    }

    // Dictionary lookup for the candidates which are
    // overlapping between history and conversion.
    // Check only the last history segment at this moment.
    //
    // Example: history "おいかわ(及川)", conversion: "たくや"
    // Here, try to find "おいかわたくや(及川卓也)" from dictionary
    // and insert "卓也" as a new word node with a modified cost
    if (s + 1 == history_segments_size) {
      const bool is_prediction =
          (segments.request_type() == Segments::SUGGESTION ||
           segments.request_type() == Segments::PREDICTION);
      const Node *node = Lookup(segments_pos, key.size(), request,
                                is_reverse, is_prediction, lattice);
      for (const Node *compound_node = node; compound_node != NULL;
           compound_node = compound_node->bnext) {
        // No overlapps
        if (compound_node->key.size() <= rnode->key.size() ||
            compound_node->value.size() <= rnode->value.size() ||
            !Util::StartsWith(compound_node->key, rnode->key) ||
            !Util::StartsWith(compound_node->value, rnode->value)) {
          // not a prefix
          continue;
        }

        // Must be in the same POS group.
        // http://b/issue?id=2977618
        if (pos_group_->GetPosGroup(candidate.lid)
            != pos_group_->GetPosGroup(compound_node->lid)) {
          continue;
        }

        // make new virtual node
        Node *new_node = lattice->NewNode();
        CHECK(new_node);

        // get the suffix part ("たくや/卓也")
        new_node->key.assign(compound_node->key, rnode->key.size(),
                             compound_node->key.size() - rnode->key.size());
        new_node->value.assign(
            compound_node->value, rnode->value.size(),
            compound_node->value.size() - rnode->value.size());

        // rid/lid are derived from the compound.
        // lid is just an approximation
        new_node->rid = compound_node->rid;
        new_node->lid = compound_node->lid;
        new_node->bnext = NULL;
        new_node->node_type = Node::NOR_NODE;
        new_node->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;

        // New cost recalcuration:
        //
        // compound_node->wcost * (candidate len / compound_node len)
        // - trans(candidate.rid, new_node.lid)
        new_node->wcost =
            compound_node->wcost *
            candidate.value.size() / compound_node->value.size()
            - connector_->GetTransitionCost(candidate.rid, new_node->lid);

        VLOG(2) << " compound_node->lid=" << compound_node->lid
                << " compound_node->rid=" << compound_node->rid
                << " compound_node->wcost=" << compound_node->wcost;
        VLOG(2) << " last_rid=" << last_rid
                << " candidate.lid=" << candidate.lid
                << " candidate.rid=" << candidate.rid
                << " candidate.cost=" << candidate.cost
                << " candidate.wcost=" << candidate.wcost;
        VLOG(2) << " new_node->wcost=" << new_node->wcost;

        new_node->constrained_prev = rnode;

        // Added as new node
        lattice->Insert(segments_pos + rnode->key.size(), new_node);

        VLOG(2) << "Added: " << new_node->key << " " << new_node->value;
      }
    }

    // update segment pos
    segments_pos += rnode->key.size();
    last_rid = rnode->rid;
  }
  lattice->set_history_end_pos(segments_pos);
  return true;
}

void ImmutableConverterImpl::MakeLatticeNodesForConversionSegments(
    const Segments &segments, const ConversionRequest &request,
    const string &history_key, Lattice *lattice) const {
  const string &key = lattice->key();
  const bool is_conversion =
      (segments.request_type() == Segments::CONVERSION);
  // Do not use KeyCorrector if user changes the boundary.
  // http://b/issue?id=2804996
  scoped_ptr<KeyCorrector> key_corrector;
  if (is_conversion && !segments.resized()) {
    KeyCorrector::InputMode mode = KeyCorrector::ROMAN;
    if (GET_CONFIG(preedit_method) != config::Config::ROMAN) {
      mode = KeyCorrector::KANA;
    }
    key_corrector.reset(new KeyCorrector(key, mode, history_key.size()));
  }

  const bool is_reverse =
      (segments.request_type() == Segments::REVERSE_CONVERSION);
  const bool is_prediction =
      (segments.request_type() == Segments::SUGGESTION ||
       segments.request_type() == Segments::PREDICTION);
  for (size_t pos = history_key.size(); pos < key.size(); ++pos) {
    if (lattice->end_nodes(pos) != NULL) {
      Node *rnode =
          Lookup(pos, key.size(), request, is_reverse, is_prediction, lattice);
      // If history key is NOT empty and user input seems to starts with
      // a particle ("はにで..."), mark the node as STARTS_WITH_PARTICLE.
      // We change the segment boundary if STARTS_WITH_PARTICLE attribute
      // is assigned.
      if (!history_key.empty() && pos == history_key.size()) {
        for (Node *node = rnode; node != NULL; node = node->bnext) {
          if (pos_matcher_->IsAcceptableParticleAtBeginOfSegment(node->lid) &&
              node->lid == node->rid) {  // not a compound.
            node->attributes |= Node::STARTS_WITH_PARTICLE;
          }
        }
      }
      CHECK(rnode != NULL);
      lattice->Insert(pos, rnode);
      InsertCorrectedNodes(
          pos, key, request, key_corrector.get(), dictionary_, lattice);
    }
  }
}

void ImmutableConverterImpl::ApplyPrefixSuffixPenalty(
    const string &conversion_key,
    Lattice *lattice) const {
  const string &key = lattice->key();
  DCHECK_LE(conversion_key.size(), key.size());
  for (Node *node = lattice->begin_nodes(key.size() -
                                         conversion_key.size());
       node != NULL; node = node->bnext) {
    // TODO(taku):
    // We might be able to tweak the penalty according to
    // the size of history segments.
    // If history-segments is non-empty, we can make the
    // penalty smaller so that history context is more likely
    // selected.
    node->wcost += segmenter_->GetPrefixPenalty(node->lid);
  }

  for (Node *node = lattice->end_nodes(key.size());
       node != NULL; node = node->enext) {
    node->wcost += segmenter_->GetSuffixPenalty(node->rid);
  }
}

void ImmutableConverterImpl::Resegment(
    const Segments &segments,
    const string &history_key, const string &conversion_key,
    Lattice *lattice) const {
  for (size_t pos = history_key.size();
       pos < history_key.size() + conversion_key.size(); ++pos) {
    ApplyResegmentRules(pos, lattice);
  }

  // Enable constrained node.
  size_t segments_pos = 0;
  for (size_t s = 0; s < segments.segments_size(); ++s) {
    const Segment &segment = segments.segment(s);
    if (segment.segment_type() == Segment::FIXED_VALUE) {
      const Segment::Candidate &candidate = segment.candidate(0);
      Node *rnode = lattice->NewNode();
      CHECK(rnode);
      rnode->lid       = candidate.lid;
      rnode->rid       = candidate.rid;
      rnode->wcost     = kMinCost;
      rnode->value     = candidate.value;
      rnode->key       = segment.key();
      rnode->node_type = Node::CON_NODE;
      rnode->bnext     = NULL;
      lattice->Insert(segments_pos, rnode);
    }
    segments_pos += segment.key().size();
  }
}

// Single segment conversion results should be set to |segments|.
void ImmutableConverterImpl::InsertFirstSegmentToCandidates(
    Segments *segments,
    const Lattice &lattice,
    const vector<uint16> &group,
    size_t max_candidates_size) const {
  const size_t only_first_segment_candidate_pos =
      segments->conversion_segment(0).candidates_size();
  InsertCandidates(segments, lattice, group,
                   max_candidates_size,
                   ONLY_FIRST_SEGMENT);
  // Note that inserted candidates might consume the entire key.
  // e.g. key: "なのは", value: "ナノは"
  // Erase them later.
  if (segments->conversion_segment(0).candidates_size() <=
      only_first_segment_candidate_pos) {
    return;
  }

  // Set new costs for only first segment candidates
  // Basically, only first segment candidates cost is smaller
  // than that of single segment conversion results.
  // For example, the cost of "私の" is smaller than "私の名前は".
  // To merge these two categories of results, we will add the
  // cost penalty based on the cost diff.
  const Segment &first_segment = segments->conversion_segment(0);
  const int base_cost_diff =
      max(0,
          (first_segment.candidate(0).cost -
           first_segment.candidate(only_first_segment_candidate_pos).cost));
  const int base_wcost_diff =
      max(0,
          (first_segment.candidate(0).wcost -
           first_segment.candidate(only_first_segment_candidate_pos).wcost));
  for (size_t i = only_first_segment_candidate_pos;
       i < first_segment.candidates_size();) {
    static const int kOnlyFirstSegmentOffset = 300;
    Segment::Candidate *candidate =
        segments->mutable_conversion_segment(0)->mutable_candidate(i);
    // If the size of candidate's key is greater than or
    // equal to 1st segment's key,
    // it means that the result consumes the entire key.
    // Such results are not appropriate for PARTIALLY_KEY_CONSUMED so erase it.
    if (candidate->key.size() >= first_segment.key().size()) {
      segments->mutable_conversion_segment(0)->erase_candidate(i);
      continue;
    }
    candidate->cost += (base_cost_diff + kOnlyFirstSegmentOffset);
    candidate->wcost += (base_wcost_diff + kOnlyFirstSegmentOffset);
    DCHECK(!(candidate->attributes &
             Segment::Candidate::PARTIALLY_KEY_CONSUMED));
    candidate->attributes |= Segment::Candidate::PARTIALLY_KEY_CONSUMED;
    candidate->consumed_key_size = Util::CharsLen(candidate->key);
    ++i;
  }
}

bool ImmutableConverterImpl::IsSegmentEndNode(
    const Segments &segments, const Node *node,
    const vector<uint16> &group, bool is_single_segment) const {
  DCHECK(node->next);
  if (node->next->node_type == Node::EOS_NODE) {
    return true;
  }

  // In reverse conversion, group consecutive white spaces into one segment.
  // For example, "ほん むりょう" -> "ほん", " ", "むりょう".
  if (segments.request_type() == Segments::REVERSE_CONVERSION) {
    const bool this_node_is_ws = ContainsWhiteSpacesOnly(node->key);
    const bool next_node_is_ws = ContainsWhiteSpacesOnly(node->next->key);
    if (this_node_is_ws) {
      return !next_node_is_ws;
    }
    if (next_node_is_ws) {
      return true;
    }
    // If this and next nodes are both non-white spaces, fall back to the
    // subsequent logic.
  }

  const Segment &old_segment = segments.segment(group[node->begin_pos]);
  // |node| and |node->next| should be in same segment due to FIXED_BOUNDAY
  // |node->next| is NOT a boundary. Very strong constraint.
  if (group[node->begin_pos] == group[node->next->begin_pos] &&
      old_segment.segment_type() == Segment::FIXED_BOUNDARY) {
    return false;
  }

  // |node->next| is a boundary. Very strong constraint.
  if (group[node->begin_pos] != group[node->next->begin_pos]) {
    return true;
  }

  // CON_NODE is generated for FIXED_VALUE candidate.
  if (node->node_type == Node::CON_NODE) {
    return true;
  }

  // Grammatically segmented.
  if (segmenter_->IsBoundary(node, node->next, is_single_segment)) {
    return true;
  }

  return false;
}

Segment *ImmutableConverterImpl::GetInsertTargetSegment(
    const Lattice &lattice,
    const vector<uint16> &group,
    InsertCandidatesType type,
    size_t begin_pos,
    const Node *node,
    Segments *segments) const {
  if (type != MULTI_SEGMENTS) {
    DCHECK(type == SINGLE_SEGMENT || type == ONLY_FIRST_SEGMENT);
    // Realtime conversion that produces only one segment.
    return segments->mutable_segment(segments->segments_size() - 1);
  }

  // 'Normal' conversion. Add new segment and initialize it.
  Segment *segment = segments->add_segment();
  DCHECK(segment);
  segment->clear_candidates();
  segment->set_key(
      lattice.key().substr(begin_pos, node->end_pos - begin_pos));
  const Segment &old_segment = segments->segment(group[node->begin_pos]);
  segment->set_segment_type(old_segment.segment_type());
  return segment;
}

void ImmutableConverterImpl::InsertCandidates(
    Segments *segments,
    const Lattice &lattice,
    const vector<uint16> &group,
    size_t max_candidates_size,
    InsertCandidatesType type) const {
  // skip HIS_NODE(s)
  Node *prev = lattice.bos_nodes();
  for (Node *node = lattice.bos_nodes()->next;
       node->next != NULL && node->node_type == Node::HIS_NODE;
       node = node->next) {
    prev = node;
  }

  const size_t expand_size =
      max(static_cast<size_t>(1),
          min(static_cast<size_t>(512), max_candidates_size));

  const bool is_single_segment = (type == SINGLE_SEGMENT);
  NBestGenerator nbest_generator(
      suppression_dictionary_, segmenter_, connector_, pos_matcher_,
      &lattice, suggestion_filter_);

  string original_key;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    original_key.append(segments->conversion_segment(i).key());
  }

  size_t begin_pos = string::npos;
  for (Node *node = prev->next; node->next != NULL; node = node->next) {
    if (begin_pos == string::npos) {
      begin_pos = node->begin_pos;
    }

    if (!IsSegmentEndNode(*segments, node, group, is_single_segment)) {
      continue;
    }

    Segment *segment = GetInsertTargetSegment(
        lattice, group, type, begin_pos, node, segments);
    CHECK(segment);

    NBestGenerator::BoundaryCheckMode mode = NBestGenerator::STRICT;
    if (type == SINGLE_SEGMENT) {
      // For realtime conversion.
      mode = NBestGenerator::ONLY_EDGE;
    } else if (segment->segment_type() == Segment::FIXED_BOUNDARY) {
      // Boundary is specified. Skip boundary check in nbest generator.
      mode = NBestGenerator::ONLY_MID;
    }
    nbest_generator.Reset(prev, node->next, mode);

    ExpandCandidates(original_key, &nbest_generator, segment,
                     segments->request_type(), expand_size);

    if (type == MULTI_SEGMENTS || type == SINGLE_SEGMENT) {
      InsertDummyCandidates(segment, expand_size);
    }

    if (node->node_type == Node::CON_NODE) {
      segment->set_segment_type(Segment::FIXED_VALUE);
    }

    if (type == ONLY_FIRST_SEGMENT) {
      break;
    }
    begin_pos = string::npos;
    prev = node;
  }
}

bool ImmutableConverterImpl::MakeSegments(const ConversionRequest &request,
                                          const Lattice &lattice,
                                          const vector<uint16> &group,
                                          Segments *segments) const {
  if (segments == NULL) {
    LOG(WARNING) << "Segments is NULL";
    return false;
  }

  const Segments::RequestType type = segments->request_type();
  const bool is_prediction = (type == Segments::PREDICTION ||
                              type == Segments::SUGGESTION ||
                              type == Segments::PARTIAL_PREDICTION ||
                              type == Segments::PARTIAL_SUGGESTION);

  if (is_prediction) {
    const size_t max_candidates_size =
        segments->max_prediction_candidates_size();

    if (request.create_partial_candidates()) {
      // TODO(toshiyuki): It may be better to change this value
      // according to the key length.
      static const size_t kOnlyFirstSegmentCandidateSize = 3;
      const size_t single_segment_candidates_size =
          ((max_candidates_size > kOnlyFirstSegmentCandidateSize) ?
           max_candidates_size - kOnlyFirstSegmentCandidateSize : 1);
      InsertCandidates(segments, lattice, group,
                       single_segment_candidates_size, SINGLE_SEGMENT);

      // Even if single_segment_candidates_size + kOnlyFirstSegmentCandidateSize
      // is greater than max_candidates_size, we cannot skip
      // InsertFirstSegmentToCandidates().
      // For example:
      //   the sum: 11
      //   max_candidates_size: 10
      //   current candidate size: 8
      // In this case, the sum > |max_candidates_size|, but we should not
      // skip calling InsertFirstSegmentToCandidates, as we want to add
      // two candidates.
      const size_t only_first_segment_candidates_size =
          min(max_candidates_size,
              single_segment_candidates_size + kOnlyFirstSegmentCandidateSize);
      InsertFirstSegmentToCandidates(
          segments, lattice, group, only_first_segment_candidates_size);
    } else {
      InsertCandidates(
          segments, lattice, group, max_candidates_size, SINGLE_SEGMENT);
    }
  } else {
    DCHECK(!request.create_partial_candidates());
    // Currently, we assume that REVERSE_CONVERSION only
    // requires 1 result.
    // TODO(taku): support to set the size on REVESER_CONVERSION mode.
    const size_t max_candidates_size =
        ((type == Segments::REVERSE_CONVERSION) ?
         1 : segments->max_conversion_candidates_size());

    // InsertCandidates inserts new segments after the existing
    // conversion segments. So we have to erase old conversion segments.
    // We have to keep old segments for calling InsertCandidates because
    // we need segment constraints like FIXED_BOUNDARY.
    // TODO(toshiyuki): We want more beautiful structure.
    const size_t old_conversion_segments_size =
        segments->conversion_segments_size();
    InsertCandidates(
        segments, lattice, group, max_candidates_size, MULTI_SEGMENTS);
    if (old_conversion_segments_size > 0) {
      segments->erase_segments(segments->history_segments_size(),
                               old_conversion_segments_size);
    }
  }
  return true;
}

void ImmutableConverterImpl::MakeGroup(
    const Segments &segments, vector<uint16> *group) const {
  group->clear();
  for (size_t i = 0; i < segments.segments_size(); ++i) {
    for (size_t j = 0; j < segments.segment(i).key().size(); ++j) {
      group->push_back(static_cast<uint16>(i));
    }
  }
  group->push_back(static_cast<uint16>(segments.segments_size()));
}

bool ImmutableConverterImpl::ConvertForRequest(
    const ConversionRequest &request, Segments *segments) const {
  const bool is_prediction =
      (segments->request_type() == Segments::PREDICTION ||
       segments->request_type() == Segments::SUGGESTION);

  Lattice *lattice = GetLattice(segments, is_prediction);

  if (!MakeLattice(request, segments, lattice)) {
    LOG(WARNING) << "could not make lattice";
    return false;
  }

  vector<uint16> group;
  MakeGroup(*segments, &group);

  if (is_prediction) {
    if (!PredictionViterbi(*segments, lattice)) {
      LOG(WARNING) << "prediction_viterbi failed";
      return false;
    }
  } else {
    if (!Viterbi(*segments, lattice)) {
      LOG(WARNING) << "viterbi failed";
      return false;
    }
  }

  VLOG(2) << lattice->DebugString();
  if (!MakeSegments(request, *lattice, group, segments)) {
    LOG(WARNING) << "make segments failed";
    return false;
  }

  return true;
}

}  // namespace mozc
