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

#ifndef MOZC_PREDICTION_PREDICTOR_H_
#define MOZC_PREDICTION_PREDICTOR_H_

#include <memory>
#include <string>

#include "prediction/predictor_interface.h"
#include "request/conversion_request.h"

namespace mozc {

class BasePredictor : public PredictorInterface {
 public:
  // Initializes the composite of predictor with given sub-predictors. All the
  // predictors are owned by this class and deleted on destruction of this
  // instance.
  BasePredictor(PredictorInterface *dictionary_predictor,
                PredictorInterface *user_history_predictor);
  ~BasePredictor() override;

  // Hook(s) for all mutable operations.
  void Finish(const ConversionRequest &request, Segments *segments) override;

  // Reverts the last Finish operation.
  void Revert(Segments *segments) override;

  // Clears all history data of UserHistoryPredictor.
  bool ClearAllHistory() override;

  // Clears unused history data of UserHistoryPredictor.
  bool ClearUnusedHistory() override;

  // Clears a specific user history data of UserHistoryPredictor.
  bool ClearHistoryEntry(const string &key, const string &value) override;

  // Syncs user history.
  bool Sync() override;

  // Reloads usre history.
  bool Reload() override;

  // Waits for syncer to complete.
  bool Wait() override;

  // The following interfaces are implemented in derived classes.
  // const string &GetPredictorName() const = 0;
  // bool PredictForRequest(const ConversionRequest &request,
  //                        Segments *segments) const = 0;

 protected:
  std::unique_ptr<PredictorInterface> dictionary_predictor_;
  std::unique_ptr<PredictorInterface> user_history_predictor_;
};

// TODO(team): The name should be DesktopPredictor
class DefaultPredictor : public BasePredictor {
 public:
  static PredictorInterface *CreateDefaultPredictor(
      PredictorInterface *dictionary_predictor,
      PredictorInterface *user_history_predictor);

  DefaultPredictor(PredictorInterface *dictionary_predictor,
                   PredictorInterface *user_history_predictor);
  ~DefaultPredictor() override;

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override;

  const string &GetPredictorName() const override { return predictor_name_; }

 private:
  const ConversionRequest empty_request_;
  const string predictor_name_;
};

class MobilePredictor : public BasePredictor {
 public:
  static PredictorInterface *CreateMobilePredictor(
      PredictorInterface *dictionary_predictor,
      PredictorInterface *user_history_predictor);

  MobilePredictor(PredictorInterface *dictionary_predictor,
                  PredictorInterface *user_history_predictor);
  ~MobilePredictor() override;

  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override;

  const string &GetPredictorName() const override { return predictor_name_; }

 private:
  const ConversionRequest empty_request_;
  const string predictor_name_;
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_PREDICTOR_H_
