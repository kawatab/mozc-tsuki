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

#include "rewriter/emoticon_rewriter.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/singleton.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/serialized_dictionary.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {
namespace {

class ValueCostCompare {
 public:
  bool operator() (SerializedDictionary::const_iterator a,
                   SerializedDictionary::const_iterator b) const {
    return a.cost() < b.cost();
  }
};

class IsEqualValue {
 public:
  bool operator() (const SerializedDictionary::const_iterator a,
                   const SerializedDictionary::const_iterator b) const {
    return a.value() == b.value();
  }
};

// Insert Emoticon into the |segment|
// Top |initial_insert_size| candidates are inserted from |initial_insert_pos|.
// Remained candidates are added to the buttom.
void InsertCandidates(SerializedDictionary::const_iterator begin,
                      SerializedDictionary::const_iterator end,
                      size_t initial_insert_pos,
                      size_t initial_insert_size,
                      bool is_no_learning,
                      Segment *segment) {
  if (segment->candidates_size() == 0) {
    LOG(WARNING) << "candiadtes_size is 0";
    return;
  }

  const Segment::Candidate &base_candidate = segment->candidate(0);
  size_t offset = std::min(initial_insert_pos, segment->candidates_size());

  // Sort values by cost just in case
  std::vector<SerializedDictionary::const_iterator> sorted_value;
  for (auto iter = begin; iter != end; ++iter) {
    sorted_value.push_back(iter);
  }

  std::sort(sorted_value.begin(), sorted_value.end(), ValueCostCompare());

  // after sorting the valeus by |cost|, adjacent candidates
  // will have the same value. It is almost OK to use std::unique to
  // remove dup entries, it is not a perfect way though.
  sorted_value.erase(
      std::unique(sorted_value.begin(), sorted_value.end(), IsEqualValue()),
      sorted_value.end());

  for (size_t i = 0; i < sorted_value.size(); ++i) {
    Segment::Candidate *c = nullptr;

    if (i < initial_insert_size) {
      c = segment->insert_candidate(offset);
      ++offset;
    } else {
      c = segment->push_back_candidate();
    }

    if (c == nullptr) {
      LOG(ERROR) << "cannot insert candidate at " << offset;
      continue;
    }

    c->Init();
    // TODO(taku): set an appropriate POS here.
    c->lid = sorted_value[i].lid();
    c->rid = sorted_value[i].rid();
    c->cost = base_candidate.cost;
    c->value.assign(sorted_value[i].value().data(),
                    sorted_value[i].value().size());
    c->content_value = c->value;
    c->key = base_candidate.key;
    c->content_key = base_candidate.content_key;
    // no full/half width normalizations
    c->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
    c->attributes |= Segment::Candidate::NO_VARIANTS_EXPANSION;
    c->attributes |= Segment::Candidate::CONTEXT_SENSITIVE;
    if (is_no_learning) {
      c->attributes |= Segment::Candidate::NO_LEARNING;
    }

    const char kBaseEmoticonDescription[] = "顔文字";

    if (sorted_value[i].description().empty()) {
      c->description = kBaseEmoticonDescription;
    } else {
      string description = kBaseEmoticonDescription;
      description.append(" ");
      description.append(sorted_value[i].description().data(),
                         sorted_value[i].description().size());
      c->description = description;
    }
  }
}

}  // namespace

bool EmoticonRewriter::RewriteCandidate(Segments *segments) const {
  bool modified = false;
  for (size_t i = 0; i < segments->conversion_segments_size(); ++i) {
    const string &key = segments->conversion_segment(i).key();
    if (key.empty()) {
      // This case happens for zero query suggestion.
      continue;
    }
    bool is_no_learning = false;
    SerializedDictionary::const_iterator begin;
    SerializedDictionary::const_iterator end = dic_.end();
    size_t initial_insert_size = 0;
    size_t initial_insert_pos = 0;

    // TODO(taku): Emoticon dictionary does not always include "facemark".
    // Displaying non-facemarks with "かおもじ" is not always correct.
    // We have to distinguish pure facemarks and other symbol marks.

    if (key == "かおもじ") {
      // When key is "かおもじ", default candidate size should be small enough.
      // It is safe to expand all candidates at this time.
      begin = dic_.begin();
      CHECK(begin != dic_.end());
      end = dic_.end();
      // set large value(100) so that all candidates are pushed to the bottom
      initial_insert_pos = 100;
      initial_insert_size = dic_.size();
    } else if (key == "かお") {
      // When key is "かお", expand all candidates in conservative way.
      begin = dic_.begin();
      CHECK(begin != dic_.end());
      // first 6 candidates are inserted at 4 th position.
      // Other candidates are pushed to the buttom.
      initial_insert_pos = 4;
      initial_insert_size = 6;
    } else if (key == "ふくわらい") {
      // Choose one emoticon randomly from the dictionary.
      // TODO(taku): want to make it "generate" more funny emoticon.
      begin = dic_.begin();
      CHECK(begin != dic_.end());
      uint32 n = 0;
      // use secure random not to predict the next emoticon.
      Util::GetRandomSequence(reinterpret_cast<char *>(&n), sizeof(n));
      begin += n % dic_.size();
      end = begin + 1;
      initial_insert_pos = 4;
      initial_insert_size = 1;
      is_no_learning = true;   // do not learn this candidate.
    } else {
      const auto range = dic_.equal_range(key);
      begin = range.first;
      end = range.second;
      if (begin != end) {
        initial_insert_pos = 6;
        initial_insert_size = std::distance(begin, end);
      }
    }

    if (begin == end) {
      continue;
    }

    InsertCandidates(begin, end,
                     initial_insert_pos,
                     initial_insert_size,
                     is_no_learning,
                     segments->mutable_conversion_segment(i));
    modified = true;
  }

  return modified;
}

std::unique_ptr<EmoticonRewriter> EmoticonRewriter::CreateFromDataManager(
    const DataManagerInterface &data_manager) {
  StringPiece token_array_data, string_array_data;
  data_manager.GetEmoticonRewriterData(&token_array_data, &string_array_data);
  return std::unique_ptr<EmoticonRewriter>(
      new EmoticonRewriter(token_array_data, string_array_data));
}

EmoticonRewriter::EmoticonRewriter(StringPiece token_array_data,
                                   StringPiece string_array_data)
    : dic_(token_array_data, string_array_data) {}

EmoticonRewriter::~EmoticonRewriter() = default;

int EmoticonRewriter::capability(const ConversionRequest &request) const {
  if (request.request().mixed_conversion()) {
    return RewriterInterface::ALL;
  }
  return RewriterInterface::CONVERSION;
}

bool EmoticonRewriter::Rewrite(const ConversionRequest &request,
                               Segments *segments) const {
  if (!request.config().use_emoticon_conversion()) {
    VLOG(2) << "no use_emoticon_conversion";
    return false;
  }
  return RewriteCandidate(segments);
}
}  // namespace mozc
