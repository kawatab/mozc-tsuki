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

#ifndef MOZC_CONVERTER_NODE_LIST_BUILDER_H_
#define MOZC_CONVERTER_NODE_LIST_BUILDER_H_

#include <cstdint>

#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "converter/node.h"
#include "converter/node_allocator.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_token.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"

namespace mozc {

// The cost is 500 * log(30): 30 times in freq.
static const int32_t kKanaModifierInsensitivePenalty = 1700;

struct SpatialCostParams {
  int penalty = kKanaModifierInsensitivePenalty;
  int min_char_length = 0;
  int GetPenalty(absl::string_view key) const {
    return (min_char_length > 0 && Util::CharsLen(key) < min_char_length)
               ? kKanaModifierInsensitivePenalty
               : penalty;
  }
};

// Propagates the spatial_cost_params only when enable_new_spatial_scoring is
// enabled.
inline SpatialCostParams GetSpatialCostParams(
    const ConversionRequest &request) {
  const auto &experiment_params = request.request().decoder_experiment_params();
  SpatialCostParams result;
  if (experiment_params.enable_new_spatial_scoring()) {
    result.penalty = experiment_params.spatial_cost_penalty();
    result.min_char_length =
        experiment_params.spatial_cost_penalty_min_char_length();
  }
  return result;
}

// Provides basic functionality for building a list of nodes.
// This class is defined inline because it contributes to the performance of
// dictionary lookup.
class BaseNodeListBuilder : public dictionary::DictionaryInterface::Callback {
 public:
  BaseNodeListBuilder(mozc::NodeAllocator *allocator, int limit,
                      const SpatialCostParams &spatial_cost_param)
      : allocator_(allocator),
        limit_(limit),
        penalty_(0),
        spatial_cost_params_(spatial_cost_param),
        result_(nullptr) {
    DCHECK(allocator_) << "Allocator must not be nullptr";
  }

  BaseNodeListBuilder(const BaseNodeListBuilder &) = delete;
  BaseNodeListBuilder &operator=(const BaseNodeListBuilder &) = delete;

  // Determines a penalty for tokens of this (key, actual_key) pair.
  ResultType OnActualKey(absl::string_view key, absl::string_view actual_key,
                         int num_expanded) override {
    penalty_ = num_expanded > 0 ? spatial_cost_params_.GetPenalty(key) : 0;
    return TRAVERSE_CONTINUE;
  }

  // Creates a new node and prepends it to the current list.
  ResultType OnToken(absl::string_view key, absl::string_view actual_key,
                     const dictionary::Token &token) override {
    Node *new_node = NewNodeFromToken(token);
    PrependNode(new_node);
    return (limit_ <= 0) ? TRAVERSE_DONE : TRAVERSE_CONTINUE;
  }

  int limit() const { return limit_; }
  int penalty() const { return penalty_; }
  Node *result() const { return result_; }
  NodeAllocator *allocator() { return allocator_; }

  Node *NewNodeFromToken(const dictionary::Token &token) {
    Node *new_node = allocator_->NewNode();
    new_node->InitFromToken(token);
    new_node->wcost += penalty_;
    return new_node;
  }

  void PrependNode(Node *node) {
    node->bnext = result_;
    result_ = node;
    --limit_;
  }

 protected:
  NodeAllocator *allocator_;
  int limit_;
  int penalty_;
  const SpatialCostParams spatial_cost_params_;
  Node *result_;
};

// Implements key filtering rule for LookupPrefix().
// This class is also defined inline.
class NodeListBuilderForLookupPrefix : public BaseNodeListBuilder {
 public:
  NodeListBuilderForLookupPrefix(mozc::NodeAllocator *allocator, int limit,
                                 size_t min_key_length,
                                 const SpatialCostParams &spatial_cost_params)
      : BaseNodeListBuilder(allocator, limit, spatial_cost_params),
        min_key_length_(min_key_length) {}

  NodeListBuilderForLookupPrefix(const NodeListBuilderForLookupPrefix &) =
      delete;
  NodeListBuilderForLookupPrefix &operator=(
      const NodeListBuilderForLookupPrefix &) = delete;

  ResultType OnKey(absl::string_view key) override {
    return key.size() < min_key_length_ ? TRAVERSE_NEXT_KEY : TRAVERSE_CONTINUE;
  }

 protected:
  const size_t min_key_length_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_NODE_LIST_BUILDER_H_
