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

#ifndef MOZC_CONVERTER_CONVERTER_H_
#define MOZC_CONVERTER_CONVERTER_H_

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "converter/converter_interface.h"
#include "converter/immutable_converter_interface.h"
#include "converter/segments.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "request/conversion_request.h"
#include "rewriter/rewriter_interface.h"
#include "testing/gunit_prod.h"  // for FRIEND_TEST()
#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc {

class ConverterImpl final : public ConverterInterface {
 public:
  ConverterImpl() = default;

  // Lazily initializes the internal members. Must be called before the use.
  void Init(const dictionary::PosMatcher *pos_matcher,
            const dictionary::SuppressionDictionary *suppression_dictionary,
            std::unique_ptr<prediction::PredictorInterface> predictor,
            std::unique_ptr<RewriterInterface> rewriter,
            ImmutableConverterInterface *immutable_converter);

  ABSL_MUST_USE_RESULT
  bool StartConversionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override;
  ABSL_MUST_USE_RESULT
  bool StartConversion(Segments *segments,
                       absl::string_view key) const override;
  ABSL_MUST_USE_RESULT
  bool StartReverseConversion(Segments *segments,
                              absl::string_view key) const override;
  ABSL_MUST_USE_RESULT
  bool StartPredictionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override;
  ABSL_MUST_USE_RESULT
  bool StartPrediction(Segments *segments,
                       absl::string_view key) const override;
  ABSL_MUST_USE_RESULT
  bool StartSuggestionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override;
  ABSL_MUST_USE_RESULT
  bool StartSuggestion(Segments *segments,
                       absl::string_view key) const override;
  ABSL_MUST_USE_RESULT
  bool StartPartialPredictionForRequest(const ConversionRequest &request,
                                        Segments *segments) const override;
  ABSL_MUST_USE_RESULT
  bool StartPartialPrediction(Segments *segments,
                              absl::string_view key) const override;
  ABSL_MUST_USE_RESULT
  bool StartPartialSuggestionForRequest(const ConversionRequest &request,
                                        Segments *segments) const override;
  ABSL_MUST_USE_RESULT
  bool StartPartialSuggestion(Segments *segments,
                              absl::string_view key) const override;

  void FinishConversion(const ConversionRequest &request,
                        Segments *segments) const override;
  void CancelConversion(Segments *segments) const override;
  void ResetConversion(Segments *segments) const override;
  void RevertConversion(Segments *segments) const override;

  ABSL_MUST_USE_RESULT
  bool ReconstructHistory(Segments *segments,
                          absl::string_view preceding_text) const override;

  ABSL_MUST_USE_RESULT
  bool CommitSegmentValue(Segments *segments, size_t segment_index,
                          int candidate_index) const override;
  ABSL_MUST_USE_RESULT
  bool CommitPartialSuggestionSegmentValue(
      Segments *segments, size_t segment_index, int candidate_index,
      absl::string_view current_segment_key,
      absl::string_view new_segment_key) const override;
  ABSL_MUST_USE_RESULT
  bool FocusSegmentValue(Segments *segments, size_t segment_index,
                         int candidate_index) const override;
  ABSL_MUST_USE_RESULT
  bool CommitSegments(
      Segments *segments,
      const std::vector<size_t> &candidate_index) const override;
  ABSL_MUST_USE_RESULT bool ResizeSegment(Segments *segments,
                                          const ConversionRequest &request,
                                          size_t segment_index,
                                          int offset_length) const override;
  ABSL_MUST_USE_RESULT bool ResizeSegment(
      Segments *segments, const ConversionRequest &request,
      size_t start_segment_index, size_t segments_size,
      absl::Span<const uint8_t> new_size_array) const override;

 private:
  FRIEND_TEST(ConverterTest, CompletePosIds);
  FRIEND_TEST(ConverterTest, DefaultPredictor);
  FRIEND_TEST(ConverterTest, MaybeSetConsumedKeySizeToSegment);
  FRIEND_TEST(ConverterTest, GetLastConnectivePart);
  FRIEND_TEST(ConverterTest, PredictSetKey);

  // Complete Left id/Right id if they are not defined.
  // Some users don't push conversion button but directly
  // input hiragana sequence only with composition mode. Converter
  // cannot know which POS ids should be used for these directly-
  // input strings. This function estimates IDs from value heuristically.
  void CompletePosIds(Segment::Candidate *candidate) const;

  bool CommitSegmentValueInternal(Segments *segments, size_t segment_index,
                                  int candidate_index,
                                  Segment::SegmentType segment_type) const;

  // Sets all the candidates' attribute PARTIALLY_KEY_CONSUMED
  // and consumed_key_size if the attribute is not set.
  static void MaybeSetConsumedKeySizeToCandidate(size_t consumed_key_size,
                                                 Segment::Candidate *candidate);

  // Sets all the candidates' attribute PARTIALLY_KEY_CONSUMED
  // and consumed_key_size if the attribute is not set.
  static void MaybeSetConsumedKeySizeToSegment(size_t consumed_key_size,
                                               Segment *segment);

  // Rewrites and applies the suppression dictionary.
  void RewriteAndSuppressCandidates(const ConversionRequest &request,
                                    Segments *segments) const;

  // Limits the number of candidates based on a request.
  // This method doesn't drop meta candidates for T13n conversion.
  void TrimCandidates(const ConversionRequest &request,
                      Segments *segments) const;

  // Commits usage stats for committed text.
  // |begin_segment_index| is a index of whole segments. (history and conversion
  // segments)
  void CommitUsageStats(const Segments *segments, size_t begin_segment_index,
                        size_t segment_length) const;

  // Returns the substring of |str|. This substring consists of similar script
  // type and you can use it as preceding text for conversion.
  bool GetLastConnectivePart(absl::string_view preceding_text, std::string *key,
                             std::string *value, uint16_t *id) const;

  ABSL_MUST_USE_RESULT bool Predict(const ConversionRequest &request,
                                    absl::string_view key,
                                    Segments *segments) const;

  ABSL_MUST_USE_RESULT bool Convert(const ConversionRequest &request,
                                    absl::string_view key,
                                    Segments *segments) const;

  const dictionary::PosMatcher *pos_matcher_ = nullptr;
  const dictionary::SuppressionDictionary *suppression_dictionary_;
  std::unique_ptr<prediction::PredictorInterface> predictor_;
  std::unique_ptr<RewriterInterface> rewriter_;
  const ImmutableConverterInterface *immutable_converter_ = nullptr;
  uint16_t general_noun_id_ = std::numeric_limits<uint16_t>::max();
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_H_
