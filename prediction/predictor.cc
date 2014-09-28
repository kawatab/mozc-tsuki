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

#include "prediction/predictor.h"

#include <string>
#include <vector>

#include "base/flags.h"
#include "base/logging.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "session/commands.pb.h"

// TODO(team): Implement ambiguity expansion for rewriters.
DEFINE_bool(enable_ambiguity_expansion, true,
            "Enable ambiguity trigger expansion for predictions");
DECLARE_bool(enable_expansion_for_dictionary_predictor);
DECLARE_bool(enable_expansion_for_user_history_predictor);

namespace mozc {
namespace {
const int kPredictionSize = 100;
// On Mobile mode PREDICTION (including PARTIAL_PREDICTION) behaves like as
// conversion so very large limit is preferable.
const int kMobilePredictionSize = 1000;

size_t GetCandidatesSize(const Segments &segments) {
  if (segments.conversion_segments_size() <= 0) {
    LOG(ERROR) << "No conversion segments found";
    return 0;
  }
  return segments.conversion_segment(0).candidates_size();
}

// TODO(taku): Is it OK to check only |zero_query_suggestion| and
// |mixed_conversion|?
bool IsZeroQuery(const ConversionRequest &request) {
  return request.request().zero_query_suggestion();
}

}  // namespace

BasePredictor::BasePredictor(PredictorInterface *dictionary_predictor,
                             PredictorInterface *user_history_predictor)
    : dictionary_predictor_(dictionary_predictor),
      user_history_predictor_(user_history_predictor) {
  DCHECK(dictionary_predictor_.get());
  DCHECK(user_history_predictor_.get());
  FLAGS_enable_expansion_for_dictionary_predictor =
      FLAGS_enable_ambiguity_expansion;
  FLAGS_enable_expansion_for_user_history_predictor =
      FLAGS_enable_ambiguity_expansion;
}

BasePredictor::~BasePredictor() {}

void BasePredictor::Finish(Segments *segments) {
  user_history_predictor_->Finish(segments);
  dictionary_predictor_->Finish(segments);

  if (segments->conversion_segments_size() < 1 ||
      segments->request_type() == Segments::CONVERSION) {
    return;
  }
  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment->candidates_size() < 1) {
    return;
  }
  // update the key as the original key only contains
  // the 'prefix'.
  // note that candidate key may be different from request key (=segment key)
  // due to suggestion/prediction.
  segment->set_key(segment->candidate(0).key);
}

// Since DictionaryPredictor is immutable, no need
// to call DictionaryPredictor::Revert/Clear*/Finish methods.
void BasePredictor::Revert(Segments *segments) {
  user_history_predictor_->Revert(segments);
}

bool BasePredictor::ClearAllHistory() {
  return user_history_predictor_->ClearAllHistory();
}

bool BasePredictor::ClearUnusedHistory() {
  return user_history_predictor_->ClearUnusedHistory();
}

bool BasePredictor::ClearHistoryEntry(const string &key, const string &value) {
  return user_history_predictor_->ClearHistoryEntry(key, value);
}

bool BasePredictor::WaitForSyncerForTest() {
  return user_history_predictor_->WaitForSyncerForTest();
}

bool BasePredictor::Sync() {
  return user_history_predictor_->Sync();
}

bool BasePredictor::Reload() {
  return user_history_predictor_->Reload();
}

// static
PredictorInterface *DefaultPredictor::CreateDefaultPredictor(
    PredictorInterface *dictionary_predictor,
    PredictorInterface *user_history_predictor) {
  return new DefaultPredictor(dictionary_predictor, user_history_predictor);
}

DefaultPredictor::DefaultPredictor(PredictorInterface *dictionary_predictor,
                                   PredictorInterface *user_history_predictor)
    : BasePredictor(dictionary_predictor, user_history_predictor),
      empty_request_(),
      predictor_name_("DefaultPredictor") {}

DefaultPredictor::~DefaultPredictor() {}

bool DefaultPredictor::PredictForRequest(const ConversionRequest &request,
                                         Segments *segments) const {
  DCHECK(segments->request_type() == Segments::PREDICTION ||
         segments->request_type() == Segments::SUGGESTION ||
         segments->request_type() == Segments::PARTIAL_PREDICTION ||
         segments->request_type() == Segments::PARTIAL_SUGGESTION);

  if (GET_CONFIG(presentation_mode)) {
    return false;
  }

  int size = kPredictionSize;
  if (segments->request_type() == Segments::SUGGESTION) {
    size = min(9, max(1, static_cast<int>(GET_CONFIG(suggestions_size))));
  }

  bool result = false;
  int remained_size = size;
  segments->set_max_prediction_candidates_size(static_cast<size_t>(size));
  result |= user_history_predictor_->PredictForRequest(request, segments);
  remained_size = size - static_cast<size_t>(GetCandidatesSize(*segments));

  // Do not call dictionary_predictor if the size of candidates get
  // >= suggestions_size.
  if (remained_size <= 0) {
    return result;
  }

  segments->set_max_prediction_candidates_size(remained_size);
  result |= dictionary_predictor_->PredictForRequest(request, segments);
  remained_size = size - static_cast<size_t>(GetCandidatesSize(*segments));

  // Do not call extra_predictor if the size of candidates get
  // >= suggestions_size.
  if (remained_size <= 0) {
    return result;
  }

  return result;
}

// static
PredictorInterface *MobilePredictor::CreateMobilePredictor(
    PredictorInterface *dictionary_predictor,
    PredictorInterface *user_history_predictor) {
  return new MobilePredictor(dictionary_predictor, user_history_predictor);
}

MobilePredictor::MobilePredictor(PredictorInterface *dictionary_predictor,
                                 PredictorInterface *user_history_predictor)
    : BasePredictor(dictionary_predictor, user_history_predictor),
      empty_request_(),
      predictor_name_("MobilePredictor") {}

MobilePredictor::~MobilePredictor() {}

bool MobilePredictor::PredictForRequest(const ConversionRequest &request,
                                        Segments *segments) const {
  DCHECK(segments->request_type() == Segments::PREDICTION ||
         segments->request_type() == Segments::SUGGESTION ||
         segments->request_type() == Segments::PARTIAL_PREDICTION ||
         segments->request_type() == Segments::PARTIAL_SUGGESTION);

  if (GET_CONFIG(presentation_mode)) {
    return false;
  }

  DCHECK(segments);

  bool result = false;
  size_t size = 0;
  size_t history_suggestion_size = IsZeroQuery(request) ? 3 : 2;

  // TODO(taku,toshiyuki): Must rewrite the logic.
  switch (segments->request_type()) {
    case Segments::SUGGESTION: {
      // Suggestion is triggered at every character insertion.
      // So here we should use slow predictors.
      size = GetCandidatesSize(*segments) + history_suggestion_size;
      segments->set_max_prediction_candidates_size(size);
      result |= user_history_predictor_->PredictForRequest(request, segments);

      size = GetCandidatesSize(*segments) + 20;
      segments->set_max_prediction_candidates_size(size);
      result |= dictionary_predictor_->PredictForRequest(request, segments);

      break;
    }
    case Segments::PARTIAL_SUGGESTION: {
      // PARTIAL SUGGESTION can be triggered in a similar manner to that of
      // SUGGESTION. We don't call slow predictors by the same reason.
      size = GetCandidatesSize(*segments) + 20;
      segments->set_max_prediction_candidates_size(size);
      result |= dictionary_predictor_->PredictForRequest(request, segments);

      break;
    }
    case Segments::PARTIAL_PREDICTION: {
      segments->set_max_prediction_candidates_size(kMobilePredictionSize);
      result |= dictionary_predictor_->PredictForRequest(request, segments);
      break;
    }
    case Segments::PREDICTION: {
      size = GetCandidatesSize(*segments) + history_suggestion_size;
      segments->set_max_prediction_candidates_size(size);
      result |= user_history_predictor_->PredictForRequest(request, segments);

      segments->set_max_prediction_candidates_size(kMobilePredictionSize);
      result |= dictionary_predictor_->PredictForRequest(request, segments);
      break;
    }
    default: {
    }  // Never reach here
  }

  return result;
}
}  // namespace mozc
