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

#include "engine/minimal_engine.h"

#include "composer/composer.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "data_manager/data_manager.h"
#include "dictionary/suppression_dictionary.h"
#include "engine/user_data_manager_interface.h"
#include "prediction/predictor_interface.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

class UserDataManagerStub : public UserDataManagerInterface {
 public:
  UserDataManagerStub() = default;
  ~UserDataManagerStub() override = default;
  bool Sync() override { return true; }
  bool Reload() override { return true; }
  bool ClearUserHistory() override { return true; }
  bool ClearUserPrediction() override { return true; }
  bool ClearUnusedUserPrediction() override { return true; }
  bool ClearUserPredictionEntry(const string &key,
                                const string &value) override {
    return true;
  }
  bool Wait() override { return true; }
};

bool AddAsIsCandidate(const string &key, Segments *segments) {
  if (segments == nullptr) {
    return false;
  }
  segments->Clear();
  Segment *segment = segments->add_segment();
  DCHECK(segment);

  Segment::Candidate *candidate = segment->push_back_candidate();
  DCHECK(candidate);
  candidate->Init();
  candidate->content_key = key;
  candidate->content_value = key;
  candidate->key = key;
  candidate->value = key;
  candidate->lid = 0;
  candidate->rid = 0;
  candidate->wcost = 0;
  candidate->cost = 0;
  candidate->attributes = Segment::Candidate::DEFAULT_ATTRIBUTE;

  return true;
}

bool AddAsIsCandidate(const ConversionRequest &request, Segments *segments) {
  if (!request.has_composer()) {
    return false;
  }
  string key;
  request.composer().GetQueryForConversion(&key);
  return AddAsIsCandidate(key, segments);
}

class MinimalConverter : public ConverterInterface {
 public:
  MinimalConverter() = default;
  ~MinimalConverter() override = default;

  bool StartConversionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override {
    return AddAsIsCandidate(request, segments);
  }

  bool StartConversion(Segments *segments, const string &key) const override {
    return AddAsIsCandidate(key, segments);
  }

  bool StartReverseConversion(Segments *segments,
                              const string &key) const override {
    return false;
  }

  bool StartPredictionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override {
    return AddAsIsCandidate(request, segments);
  }

  bool StartPrediction(Segments *segments, const string &key) const override {
    return AddAsIsCandidate(key, segments);
  }

  bool StartSuggestionForRequest(const ConversionRequest &request,
                                 Segments *segments) const override {
    return AddAsIsCandidate(request, segments);
  }

  bool StartSuggestion(Segments *segments, const string &key) const override {
    return AddAsIsCandidate(key, segments);
  }

  bool StartPartialPredictionForRequest(const ConversionRequest &request,
                                        Segments *segments) const override {
    return false;
  }

  bool StartPartialPrediction(Segments *segments,
                              const string &key) const override {
    return false;
  }

  bool StartPartialSuggestionForRequest(const ConversionRequest &request,
                                        Segments *segments) const override {
    return false;
  }

  bool StartPartialSuggestion(Segments *segments,
                              const string &key) const override {
    return false;
  }

  bool FinishConversion(const ConversionRequest &request,
                        Segments *segments) const override {
    return true;
  }

  bool CancelConversion(Segments *segments) const override {
    return true;
  }

  bool ResetConversion(Segments *segments) const override {
    return true;
  }

  bool RevertConversion(Segments *segments) const override {
    return true;
  }

  bool ReconstructHistory(Segments *segments,
                          const string &preceding_text) const override {
    return true;
  }

  bool CommitSegmentValue(Segments *segments, size_t segment_index,
                          int candidate_index) const override {
    return true;
  }

  bool CommitPartialSuggestionSegmentValue(
      Segments *segments, size_t segment_index, int candidate_index,
      const string &current_segment_key,
      const string &new_segment_key) const override {
    return true;
  }

  bool FocusSegmentValue(Segments *segments, size_t segment_index,
                         int candidate_index) const override {
    return true;
  }

  bool FreeSegmentValue(Segments *segments,
                        size_t segment_index) const override {
    return true;
  }

  bool CommitSegments(
      Segments *segments,
      const std::vector<size_t> &candidate_index) const override {
    return true;
  }

  bool ResizeSegment(Segments *segments, const ConversionRequest &request,
                     size_t segment_index, int offset_length) const override {
    return true;
  }

  bool ResizeSegment(Segments *segments, const ConversionRequest &request,
                     size_t start_segment_index, size_t segments_size,
                     const uint8 *new_size_array,
                     size_t array_size) const override {
    return true;
  }
};

class MinimalPredictor : public PredictorInterface {
 public:
  MinimalPredictor() : name_("MinimalPredictor") {}
  ~MinimalPredictor() override = default;

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override {
    return AddAsIsCandidate(request, segments);
  }

  const string &GetPredictorName() const override { return name_; }

 private:
  const string name_;
};

}  // namespace

MinimalEngine::MinimalEngine()
    : converter_(new MinimalConverter()),
      predictor_(new MinimalPredictor()),
      suppression_dictionary_(new dictionary::SuppressionDictionary()),
      user_data_manager_(new UserDataManagerStub()),
      data_manager_(new DataManager()) {}

MinimalEngine::~MinimalEngine() = default;

ConverterInterface *MinimalEngine::GetConverter() const {
  return converter_.get();
}

PredictorInterface *MinimalEngine::GetPredictor() const {
  return predictor_.get();
}

dictionary::SuppressionDictionary *MinimalEngine::GetSuppressionDictionary() {
  return suppression_dictionary_.get();
}

UserDataManagerInterface *MinimalEngine::GetUserDataManager() {
  return user_data_manager_.get();
}

const DataManagerInterface *MinimalEngine::GetDataManager() const {
  return data_manager_.get();
}

}  // namespace mozc
