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

#include "data_manager/data_manager_test_base.h"

#include <cstring>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mozc_hash_set.h"
#include "base/serialized_string_array.h"
#include "base/util.h"
#include "converter/connector.h"
#include "converter/node.h"
#include "converter/segmenter.h"
#include "data_manager/connection_file_reader.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/suggestion_filter.h"
#include "testing/base/public/gunit.h"

using mozc::dictionary::POSMatcher;

namespace mozc {

DataManagerTestBase::DataManagerTestBase(
    DataManagerInterface *data_manager,
    const size_t lsize,
    const size_t rsize,
    IsBoundaryFunc is_boundary,
    const string &connection_txt_file,
    const int expected_resolution,
    const std::vector<string> &dictionary_files,
    const std::vector<string> &suggestion_filter_files,
    const std::vector<std::pair<string, string>> &typing_model_files)
    : data_manager_(data_manager),
      lsize_(lsize),
      rsize_(rsize),
      is_boundary_(is_boundary),
      connection_txt_file_(connection_txt_file),
      expected_resolution_(expected_resolution),
      dictionary_files_(dictionary_files),
      suggestion_filter_files_(suggestion_filter_files),
      typing_model_files_(typing_model_files) {}

DataManagerTestBase::~DataManagerTestBase() = default;

void DataManagerTestBase::SegmenterTest_SameAsInternal() {
  // This test verifies that a segmenter created by MockDataManager provides
  // the expected boundary rule.
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      EXPECT_EQ(is_boundary_(rid, lid),
                segmenter->IsBoundary(rid, lid)) << rid << " " << lid;
    }
  }
}

void DataManagerTestBase::SegmenterTest_LNodeTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));

  // lnode is BOS
  Node lnode, rnode;
  lnode.node_type = Node::BOS_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, false));
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_RNodeTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));

  // rnode is EOS
  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::EOS_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, false));
      EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_NodeTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));

  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      rnode.lid = lid;
      EXPECT_EQ(segmenter->IsBoundary(rid, lid),
                segmenter->IsBoundary(lnode, rnode, false));
      EXPECT_FALSE(segmenter->IsBoundary(lnode, rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_ParticleTest() {
  std::unique_ptr<Segmenter> segmenter(
      Segmenter::CreateFromDataManager(*data_manager_));
  const POSMatcher pos_matcher(data_manager_->GetPOSMatcherData());

  Node lnode, rnode;
  lnode.Init();
  rnode.Init();
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  // "助詞"
  lnode.rid = pos_matcher.GetAcceptableParticleAtBeginOfSegmentId();
  // "名詞,サ変".
  rnode.lid = pos_matcher.GetUnknownId();
  EXPECT_TRUE(segmenter->IsBoundary(lnode, rnode, false));

  lnode.attributes |= Node::STARTS_WITH_PARTICLE;
  EXPECT_FALSE(segmenter->IsBoundary(lnode, rnode, false));
}

void DataManagerTestBase::ConnectorTest_RandomValueCheck() {
  std::unique_ptr<const Connector> connector(
      Connector::CreateFromDataManager(*data_manager_));
  ASSERT_TRUE(connector.get() != NULL);

  EXPECT_EQ(expected_resolution_, connector->GetResolution());
  for (ConnectionFileReader reader(connection_txt_file_);
       !reader.done(); reader.Next()) {
    // Randomly sample test entries because connection data have several
    // millions of entries.
    if (Util::Random(100000) != 0) {
      continue;
    }
    const int cost = reader.cost();
    EXPECT_GE(cost, 0);
    const int actual_cost =
        connector->GetTransitionCost(reader.rid_of_left_node(),
                                     reader.lid_of_right_node());
    if (cost == Connector::kInvalidCost) {
      EXPECT_EQ(cost, actual_cost);
    } else {
      EXPECT_TRUE(cost == actual_cost ||
                  (cost - cost % expected_resolution_) == actual_cost)
          << "cost: " << cost << ", actual_cost: " << actual_cost;
    }
  }
}

void DataManagerTestBase::SuggestionFilterTest_IsBadSuggestion() {
  const double kErrorRatio = 0.0001;

  // Load embedded suggestion filter (bloom filter)
  std::unique_ptr<SuggestionFilter> suggestion_filter;
  {
    const char *data = NULL;
    size_t size;
    data_manager_->GetSuggestionFilterData(&data, &size);
    suggestion_filter.reset(new SuggestionFilter(data, size));
  }

  // Load the original suggestion filter from file.
  mozc_hash_set<string> suggestion_filter_set;

  for (size_t i = 0; i < suggestion_filter_files_.size(); ++i) {
    InputFileStream input(suggestion_filter_files_[i].c_str());
    CHECK(input) << "cannot open: " << suggestion_filter_files_[i];
    string line;
    while (getline(input, line)) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      Util::LowerString(&line);
      suggestion_filter_set.insert(line);
    }
  }

  LOG(INFO) << "Filter word size:\t" << suggestion_filter_set.size();

  size_t false_positives = 0;
  size_t num_words = 0;
  for (size_t i = 0; i < dictionary_files_.size(); ++i) {
    InputFileStream input(dictionary_files_[i].c_str());
    CHECK(input) << "cannot open: " << dictionary_files_[i];
    std::vector<string> fields;
    string line;
    while (getline(input, line)) {
      fields.clear();
      Util::SplitStringUsing(line, "\t", &fields);
      CHECK_GE(fields.size(), 5);
      string value = fields[4];
      Util::LowerString(&value);

      const bool true_result =
          (suggestion_filter_set.find(value) != suggestion_filter_set.end());
      const bool bloom_filter_result
          = suggestion_filter->IsBadSuggestion(value);

      // never emits false negative
      if (true_result) {
        EXPECT_TRUE(bloom_filter_result) << value;
      } else {
        if (bloom_filter_result) {
          ++false_positives;
          LOG(INFO) << value << " is false positive";
        }
      }
      ++num_words;
    }
  }

  const float error_ratio = 1.0 * false_positives / num_words;

  LOG(INFO) << "False positive ratio is " << error_ratio;

  EXPECT_LT(error_ratio, kErrorRatio);
}

void DataManagerTestBase::CounterSuffixTest_ValidateTest() {
  const char *data = nullptr;
  size_t data_size = 0;
  data_manager_->GetCounterSuffixSortedArray(&data, &data_size);

  SerializedStringArray suffix_array;
  ASSERT_TRUE(suffix_array.Init(StringPiece(data, data_size)));

  // Check if the array is sorted in ascending order.
  StringPiece prev_suffix;  // The smallest string.
  for (size_t i = 0; i < suffix_array.size(); ++i) {
    const StringPiece suffix = suffix_array[i];
    EXPECT_LE(prev_suffix, suffix);
    prev_suffix = suffix;
  }
}

void DataManagerTestBase::TypingModelTest() {
  // Check if typing models are included in the data set.
  for (const auto &key_and_fname : typing_model_files_) {
    InputFileStream ifs(key_and_fname.second.c_str(),
                        std::ios_base::in | std::ios_base::binary);
    EXPECT_EQ(ifs.Read(), data_manager_->GetTypingModel(key_and_fname.first));
  }
}

void DataManagerTestBase::RunAllTests() {
  ConnectorTest_RandomValueCheck();
  SegmenterTest_LNodeTest();
  SegmenterTest_NodeTest();
  SegmenterTest_ParticleTest();
  SegmenterTest_RNodeTest();
  SegmenterTest_SameAsInternal();
  SuggestionFilterTest_IsBadSuggestion();
  CounterSuffixTest_ValidateTest();
  TypingModelTest();
}

}  // namespace mozc
