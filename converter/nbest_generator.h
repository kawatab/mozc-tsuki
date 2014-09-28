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

#ifndef MOZC_CONVERTER_NBEST_GENERATOR_H_
#define MOZC_CONVERTER_NBEST_GENERATOR_H_

#include <string>
#include <vector>

#include "base/freelist.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "converter/candidate_filter.h"
#include "converter/segments.h"

namespace mozc {

class ConnectorInterface;
class Lattice;
class POSMatcher;
class SegmenterInterface;
class SuggestionFilter;
class SuppressionDictionary;
struct Node;

// TODO(toshiyuki): write unittest for NBestGenerator.
class NBestGenerator {
 public:
  enum BoundaryCheckMode {
    // Boundary check mode.
    // For the case like;
    //   Candidate edge:      |  candidate  |
    //   Nodes:        |Node A|Node B|Node C|Node D|

    // For normal converison.
    //  Candidate boundary is strictly same as inner boundary.
    // A-B: Should be the boundary
    // B-C: Should not be the boundary
    // C-D: Should be the boundary
    STRICT = 0,

    // For resegmented segment.
    //  Check mid point only.
    // A-B: Don't care
    // B-C: Should not be the boundary
    // C-D: Don't care
    ONLY_MID,

    // For Realtime conversion ("私の名前は中野です").
    //  Check only for candidate edge.
    // A-B: Should be the boundary
    // B-C: Don't care
    // C-D: Should be the boundary
    ONLY_EDGE,
  };

  // Try to enumurate N-best results between begin_node and end_node.
  NBestGenerator(const SuppressionDictionary *suppression_dictionary,
                 const SegmenterInterface *segmenter,
                 const ConnectorInterface *connector,
                 const POSMatcher *pos_matcher,
                 const Lattice *lattice,
                 const SuggestionFilter *suggestion_filter);
  ~NBestGenerator();

  // Reset the iterator status.
  void Reset(const Node *begin_node, const Node *end_node,
             const BoundaryCheckMode mode);

  // Iterator:
  // Can obtain N-best results by calling Next() in sequence.
  bool Next(const string &original_key,
            Segment::Candidate *candidate,
            Segments::RequestType request_type);

 private:
  enum BoundaryCheckResult {
    VALID = 0,
    VALID_WEAK_CONNECTED,  // Valid but should get penalty.
    INVALID,
  };

  typedef BoundaryCheckResult (NBestGenerator::*BoundaryChecker)(
      const Node *, const Node *, bool) const;

  struct QueueElement;
  struct QueueElementComparator;

  // This is just a priority_queue of const QueueElement*, but supports
  // more operations in addition to std::priority_queue.
  class Agenda {
   public:
    Agenda() {
    }
    ~Agenda() {
    }

    const QueueElement *Top() const {
      return priority_queue_.front();
    }
    bool IsEmpty() const {
      return priority_queue_.empty();
    }
    void Clear() {
      priority_queue_.clear();
    }
    void Reserve(int size) {
      priority_queue_.reserve(size);
    }

    void Push(const QueueElement *element);
    void Pop();

   private:
    vector<const QueueElement*> priority_queue_;

    DISALLOW_COPY_AND_ASSIGN(Agenda);
  };

  int InsertTopResult(const string &original_key,
                      Segment::Candidate *candidate,
                      Segments::RequestType request_type);

  void MakeCandidate(Segment::Candidate *candidate,
                     int32 cost, int32 structure_cost, int32 wcost,
                     const vector<const Node *> &nodes) const;

  // Helper functions for Next(). Checks node boundary conditions.
  BoundaryCheckResult CheckStrict(
      const Node *lnode, const Node *rnode, bool is_edge) const;
  BoundaryCheckResult CheckOnlyMid(
      const Node *lnode, const Node *rnode, bool is_edge) const;
  BoundaryCheckResult CheckOnlyEdge(
      const Node *lnode, const Node *rnode, bool is_edge) const;

  int GetTransitionCost(const Node *lnode, const Node *rnode) const;

  // Create queue element from freelist
  const QueueElement *CreateNewElement(const Node *node,
                                       const QueueElement *next,
                                       int32 fx,
                                       int32 gx,
                                       int32 structure_gx,
                                       int32 w_gx);

  // References to relevant modules.
  const SuppressionDictionary *suppression_dictionary_;
  const SegmenterInterface *segmenter_;
  const ConnectorInterface *connector_;
  const POSMatcher *pos_matcher_;
  const Lattice *lattice_;

  const Node *begin_node_;
  const Node *end_node_;

  Agenda agenda_;
  FreeList<QueueElement> freelist_;
  vector<const Node *> nodes_;
  scoped_ptr<converter::CandidateFilter> filter_;
  bool viterbi_result_checked_;
  BoundaryCheckMode check_mode_;

  BoundaryChecker boundary_checker_;

  DISALLOW_COPY_AND_ASSIGN(NBestGenerator);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_NBEST_GENERATOR_H_
