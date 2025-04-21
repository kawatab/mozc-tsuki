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

#ifndef MOZC_PREDICTION_SINGLE_KANJI_PREDICTION_AGGREGATOR_H_
#define MOZC_PREDICTION_SINGLE_KANJI_PREDICTION_AGGREGATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "converter/segments.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/single_kanji_dictionary.h"
#include "prediction/prediction_aggregator_interface.h"
#include "prediction/result.h"
#include "request/conversion_request.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace mozc::prediction {

class SingleKanjiPredictionAggregator : public PredictionAggregatorInterface {
 public:
  explicit SingleKanjiPredictionAggregator(
      const DataManagerInterface &data_manager);
  ~SingleKanjiPredictionAggregator() override;

  std::vector<Result> AggregateResults(const ConversionRequest &request,
                                       const Segments &Segments) const override;

 private:
  void AppendResults(absl::string_view kanji_key,
                     absl::string_view original_input_key,
                     absl::Span<const std::string> kanji_list, int offset,
                     std::vector<Result> *results) const;

  std::unique_ptr<dictionary::SingleKanjiDictionary> single_kanji_dictionary_;
  std::unique_ptr<dictionary::PosMatcher> pos_matcher_;
  const uint16_t general_symbol_id_;
};

}  // namespace mozc::prediction

#endif  // MOZC_PREDICTION_SINGLE_KANJI_PREDICTION_AGGREGATOR_H_
