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

#ifndef MOZC_CONVERTER_CONVERTER_MOCK_H_
#define MOZC_CONVERTER_CONVERTER_MOCK_H_

#include "converter/converter_interface.h"

#include <string>
#include <vector>
#include "base/port.h"
#include "converter/segments.h"

namespace mozc {
class ConverterMock : public ConverterInterface {
 public:
  ConverterMock();
  virtual ~ConverterMock();

  // set next output of respective functions
  void SetStartConversionForRequest(Segments *segments, bool result);
  void SetStartConversion(Segments *segments, bool result);
  void SetStartReverseConversion(Segments *segments, bool result);
  void SetStartPredictionForRequest(Segments *segments, bool result);
  void SetStartPrediction(Segments *segments, bool result);
  void SetStartSuggestionForRequest(Segments *segments, bool result);
  void SetStartSuggestion(Segments *segments, bool result);
  void SetStartPartialPredictionForRequest(Segments *segments, bool result);
  void SetStartPartialPrediction(Segments *segments, bool result);
  void SetStartPartialSuggestionForRequest(Segments *segments, bool result);
  void SetStartPartialSuggestion(Segments *segments, bool result);
  void SetFinishConversion(Segments *segments, bool result);
  void SetCancelConversion(Segments *segments, bool result);
  void SetResetConversion(Segments *segments, bool result);
  void SetRevertConversion(Segments *segments, bool result);
  void SetReconstructHistory(Segments *segments, bool result);
  void SetCommitSegmentValue(Segments *segments, bool result);
  void SetCommitPartialSuggestionSegmentValue(Segments *segments, bool result);
  void SetFocusSegmentValue(Segments *segments, bool result);
  void SetFreeSegmentValue(Segments *segments, bool result);
  void SetCommitSegments(Segments *segments, bool result);
  void SetResizeSegment1(Segments *segments, bool result);
  void SetResizeSegment2(Segments *segments, bool result);

  // get last input of respective functions
  void GetStartConversionForRequest(Segments *segments,
                                    ConversionRequest *request);
  void GetStartConversion(Segments *segments, string *key);
  void GetStartReverseConversion(Segments *segments, string *key);
  void GetStartPredictionForRequest(Segments *segments,
                                    ConversionRequest *request);
  void GetStartPrediction(Segments *segments, string *key);
  void GetStartSuggestionForRequest(Segments *segments,
                                    ConversionRequest *request);
  void GetStartSuggestion(Segments *segments, string *key);
  void GetStartPartialPredictionForRequest(Segments *segments,
                                           ConversionRequest *request);
  void GetStartPartialPrediction(Segments *segments, string *key);
  void GetStartPartialSuggestionForRequest(Segments *segments,
                                           ConversionRequest *request);
  void GetStartPartialSuggestion(Segments *segments, string *key);
  void GetFinishConversion(Segments *segments);
  void GetCancelConversion(Segments *segments);
  void GetResetConversion(Segments *segments);
  void GetRevertConversion(Segments *segments);
  void GetReconstructHistory(Segments *segments);
  void GetCommitSegmentValue(Segments *segments, size_t *segment_index,
                             int *candidate_index);
  void GetCommitPartialSuggestionSegmentValue(Segments *segments,
                                              size_t *segment_index,
                                              int *candidate_index,
                                              string *current_segment_key,
                                              string *new_segment_key);
  void GetFocusSegmentValue(Segments *segments, size_t *segment_index,
                            int *candidate_index);
  void GetFreeSegmentValue(Segments *segments, size_t *segment_index);
  void GetCommitSegments(Segments *segments, vector<size_t> *candidate_index);
  void GetResizeSegment1(Segments *segments, size_t *segment_index,
                        int *offset_length);
  void GetResizeSegment2(Segments *segments, size_t *start_segment_index,
                        size_t *segments_size, uint8 **new_size_array,
                        size_t *array_size);

  // ConverterInterface
  bool StartConversionForRequest(const ConversionRequest &request,
                                 Segments *segments) const;
  bool StartConversion(Segments *segments,
                       const string &key) const;
  bool StartReverseConversion(Segments *segments,
                              const string &key) const;
  bool StartPredictionForRequest(const ConversionRequest &request,
                                 Segments *segments) const;
  bool StartPrediction(Segments *segments,
                       const string &key) const;
  bool StartSuggestionForRequest(const ConversionRequest &request,
                                 Segments *segments) const;
  bool StartSuggestion(Segments *segments,
                       const string &key) const;
  bool StartPartialPredictionForRequest(const ConversionRequest &request,
                                        Segments *segments) const;
  bool StartPartialPrediction(Segments *segments,
                              const string &key) const;
  bool StartPartialSuggestionForRequest(const ConversionRequest &request,
                                        Segments *segments) const;
  bool StartPartialSuggestion(Segments *segments,
                              const string &key) const;
  bool FinishConversion(const ConversionRequest &request,
                        Segments *segments) const;
  bool CancelConversion(Segments *segments) const;
  bool ResetConversion(Segments *segments) const;
  bool RevertConversion(Segments *segments) const;
  bool ReconstructHistory(Segments *segments,
                          const string &preceding_text) const;
  bool CommitSegmentValue(Segments *segments,
                          size_t segment_index,
                          int    candidate_index) const;
  bool CommitPartialSuggestionSegmentValue(
      Segments *segments,
      size_t segment_index,
      int candidate_index,
      const string &current_segment_key,
      const string &new_segment_key) const;
  bool FocusSegmentValue(Segments *segments,
                         size_t segment_index,
                         int candidate_index) const;
  bool FreeSegmentValue(Segments *segments,
                        size_t segment_index) const;
  bool CommitSegments(Segments *segments,
                      const vector<size_t> &candidate_index) const;
  bool ResizeSegment(Segments *segments,
                     const ConversionRequest &request,
                     size_t segment_index,
                     int offset_length) const;
  bool ResizeSegment(Segments *segments,
                     const ConversionRequest &request,
                     size_t start_segment_index,
                     size_t segments_size,
                     const uint8 *new_size_array,
                     size_t array_size) const;

 private:
  struct ConverterOutput {
    Segments segments;
    bool return_value;
    bool initialized;
    ConverterOutput() : return_value(false), initialized(false) {}
  };

  // TODO(toshiyuki): define more appropriate struct.
  struct ConverterInput {
    ConversionRequest request;
    Segments segments;
    string key;
    size_t segment_index;
    int candidate_index;
    vector<size_t> candidate_index_list;
    int offset_length;
    size_t start_segment_index;
    size_t segments_size;
    vector<uint8> new_size_array;
    string current_segment_key;
    string new_segment_key;
    ConverterInput() {}
  };

  // mutable for recode input in const functions
  mutable ConverterInput startconversionwithrequest_input_;
  mutable ConverterInput startconversion_input_;
  mutable ConverterInput startreverseconversion_input_;
  mutable ConverterInput startpredictionwithrequest_input_;
  mutable ConverterInput startprediction_input_;
  mutable ConverterInput startsuggestionforrequest_input_;
  mutable ConverterInput startsuggestion_input_;
  mutable ConverterInput startpartialpredictionforrequest_input_;
  mutable ConverterInput startpartialprediction_input_;
  mutable ConverterInput startpartialsuggestionforrequest_input_;
  mutable ConverterInput startpartialsuggestion_input_;
  mutable ConverterInput finishconversion_input_;
  mutable ConverterInput cancelconversion_input_;
  mutable ConverterInput resetconversion_input_;
  mutable ConverterInput revertconversion_input_;
  mutable ConverterInput reconstructhistory_input_;
  mutable ConverterInput commitsegmentvalue_input_;
  mutable ConverterInput commitpartialsuggestionsegmentvalue_input_;
  mutable ConverterInput focussegmentvalue_input_;
  mutable ConverterInput freesegmentvalue_input_;
  mutable ConverterInput submitsegments_input_;
  mutable ConverterInput resizesegment1_input_;
  mutable ConverterInput resizesegment2_input_;

  ConverterOutput startconversionwithrequest_output_;
  ConverterOutput startconversion_output_;
  ConverterOutput startreverseconversion_output_;
  ConverterOutput startpredictionwithrequest_output_;
  ConverterOutput startprediction_output_;
  ConverterOutput startsuggestionforrequest_output_;
  ConverterOutput startsuggestion_output_;
  ConverterOutput startpartialpredictionforrequest_output_;
  ConverterOutput startpartialprediction_output_;
  ConverterOutput startpartialsuggestionforrequest_output_;
  ConverterOutput startpartialsuggestion_output_;
  ConverterOutput finishconversion_output_;
  ConverterOutput cancelconversion_output_;
  ConverterOutput resetconversion_output_;
  ConverterOutput revertconversion_output_;
  ConverterOutput reconstructhistory_output_;
  ConverterOutput commitsegmentvalue_output_;
  ConverterOutput commitpartialsuggestionsegmentvalue_output_;
  ConverterOutput focussegmentvalue_output_;
  ConverterOutput freesegmentvalue_output_;
  ConverterOutput submitsegments_output_;
  ConverterOutput resizesegment1_output_;
  ConverterOutput resizesegment2_output_;
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_CONVERTER_MOCK_H_
