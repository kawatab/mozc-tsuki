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

#ifndef MOZC_REWRITER_SYMBOL_REWRITER_H_
#define MOZC_REWRITER_SYMBOL_REWRITER_H_

#include <memory>
#include <string>

#include "data_manager/serialized_dictionary.h"
#include "rewriter/rewriter_interface.h"
// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"
#include "absl/strings/string_view.h"

namespace mozc {

class ConversionRequest;
class ConverterInterface;
class DataManagerInterface;
class Segment;
class Segments;

class SymbolRewriter : public RewriterInterface {
 public:
  explicit SymbolRewriter(const ConverterInterface *parent_converter,
                          const DataManagerInterface *data_manager);
  ~SymbolRewriter() override;

  int capability(const ConversionRequest &request) const override;

  bool Rewrite(const ConversionRequest &request,
               Segments *segments) const override;

 private:
  FRIEND_TEST(SymbolRewriterTest, TriggerRewriteEntireTest);
  FRIEND_TEST(SymbolRewriterTest, TriggerRewriteEachTest);
  FRIEND_TEST(SymbolRewriterTest, TriggerRewriteDescriptionTest);
  FRIEND_TEST(SymbolRewriterTest, SplitDescriptionTest);

  // Some characters may have different description for full/half width forms.
  // Here we just change the description in this function.
  static const std::string GetDescription(
      const std::string &value, absl::string_view description,
      absl::string_view additional_description);

  // return true key has no-hiragana
  static bool IsSymbol(const std::string &key);

  // Insert alternative form of space.
  static void ExpandSpace(Segment *segment);

  // Return true if two symbols are in same group.
  static bool InSameSymbolGroup(SerializedDictionary::const_iterator lhs,
                                SerializedDictionary::const_iterator rhs);

  // Insert Symbol into segment.
  static void InsertCandidates(size_t default_offset,
                               const SerializedDictionary::IterRange &range,
                               bool context_sensitive, Segment *segment);

  // Add symbol desc to exsisting candidates
  static void AddDescForCurrentCandidates(
      const SerializedDictionary::IterRange &range, Segment *segment);

  static size_t GetOffset(const ConversionRequest &request,
                          absl::string_view key);

  // Insert symbols using connected all segments.
  bool RewriteEntireCandidate(const ConversionRequest &request,
                              Segments *segments) const;

  // Insert symbols using single segment.
  bool RewriteEachCandidate(const ConversionRequest &request,
                            Segments *segments) const;

  const ConverterInterface *parent_converter_;
  std::unique_ptr<SerializedDictionary> dictionary_;
};

}  // namespace mozc

#endif  // MOZC_REWRITER_SYMBOL_REWRITER_H_
