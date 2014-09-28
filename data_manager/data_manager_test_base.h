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

#ifndef MOZC_DATA_MANAGER_DATA_MANAGER_TEST_BASE_H_
#define MOZC_DATA_MANAGER_DATA_MANAGER_TEST_BASE_H_

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/gunit.h"

namespace mozc {

class DataManagerInterface;

// Provides common unit tests for DataManager.
class DataManagerTestBase : public ::testing::Test {
 protected:
  typedef bool (*IsBoundaryFunc)(uint16, uint16);

  DataManagerTestBase(DataManagerInterface *data_manager,
                      // The following three are used in segmenter test.
                      const size_t lsize,
                      const size_t rsize,
                      IsBoundaryFunc is_boundary,
                      // The following two are used in connector test.
                      const char *connection_txt_file,
                      const int expected_resolution,
                      // The following two are used in suggestion filter test
                      const char *dictionary_files,
                      const char *suggestion_filter_files);
  virtual ~DataManagerTestBase();

  void RunAllTests();

 private:
  void ConnectorTest_RandomValueCheck();
  void SegmenterTest_LNodeTest();
  void SegmenterTest_NodeTest();
  void SegmenterTest_ParticleTest();
  void SegmenterTest_RNodeTest();
  void SegmenterTest_SameAsInternal();
  void SuggestionFilterTest_IsBadSuggestion();
  void CounterSuffixTest_ValidateTest();

  scoped_ptr<DataManagerInterface> data_manager_;
  const uint16 lsize_;
  const uint16 rsize_;
  IsBoundaryFunc is_boundary_;
  const char *connection_txt_file_;
  const int expected_resolution_;
  const char *dictionary_files_;
  const char *suggestion_filter_files_;

  DISALLOW_COPY_AND_ASSIGN(DataManagerTestBase);
};

}  // namespace mozc

#endif  // MOZC_DATA_MANAGER_DATA_MANAGER_TEST_BASE_H_
