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

#include "data_manager/data_manager_test_base.h"

#include <cstring>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/util.h"
#include "converter/connector_base.h"
#include "converter/connector_interface.h"
#include "converter/node.h"
#include "converter/segmenter_base.h"
#include "converter/segmenter_interface.h"
#include "data_manager/connection_file_reader.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_matcher.h"
#include "prediction/suggestion_filter.h"
#include "rewriter/counter_suffix.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_srcdir);

namespace mozc {
namespace {


// Get actual file path for testing
string GetFilePath(const string &path) {
  return FileUtil::JoinPath(FLAGS_test_srcdir, path);
}

}  // namespace

DataManagerTestBase::DataManagerTestBase(DataManagerInterface *data_manager,
                                         const size_t lsize,
                                         const size_t rsize,
                                         IsBoundaryFunc is_boundary,
                                         const char *connection_txt_file,
                                         const int expected_resolution,
                                         const char *dictionary_files,
                                         const char *suggestion_filter_files)
    : data_manager_(data_manager),
      lsize_(lsize),
      rsize_(rsize),
      is_boundary_(is_boundary),
      connection_txt_file_(connection_txt_file),
      expected_resolution_(expected_resolution),
      dictionary_files_(dictionary_files),
      suggestion_filter_files_(suggestion_filter_files) {}

DataManagerTestBase::~DataManagerTestBase() {}

void DataManagerTestBase::SegmenterTest_SameAsInternal() {
  // This test verifies that a segmenter created by MockDataManager provides
  // the expected boundary rule.
  scoped_ptr<SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(*data_manager_));
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      EXPECT_EQ(is_boundary_(rid, lid),
                segmenter->IsBoundary(rid, lid)) << rid << " " << lid;
    }
  }
}

void DataManagerTestBase::SegmenterTest_LNodeTest() {
  scoped_ptr<SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(*data_manager_));

  // lnode is BOS
  Node lnode, rnode;
  lnode.node_type = Node::BOS_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter->IsBoundary(&lnode, &rnode, false));
      EXPECT_TRUE(segmenter->IsBoundary(&lnode, &rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_RNodeTest() {
  scoped_ptr<SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(*data_manager_));

  // rnode is EOS
  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::EOS_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      lnode.lid = lid;
      EXPECT_TRUE(segmenter->IsBoundary(&lnode, &rnode, false));
      EXPECT_TRUE(segmenter->IsBoundary(&lnode, &rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_NodeTest() {
  scoped_ptr<SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(*data_manager_));

  Node lnode, rnode;
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  for (size_t rid = 0; rid < lsize_; ++rid) {
    for (size_t lid = 0; lid < rsize_; ++lid) {
      lnode.rid = rid;
      rnode.lid = lid;
      EXPECT_EQ(segmenter->IsBoundary(rid, lid),
                segmenter->IsBoundary(&lnode, &rnode, false));
      EXPECT_FALSE(segmenter->IsBoundary(&lnode, &rnode, true));
    }
  }
}

void DataManagerTestBase::SegmenterTest_ParticleTest() {
  scoped_ptr<SegmenterInterface> segmenter(
      SegmenterBase::CreateFromDataManager(*data_manager_));
  const POSMatcher *pos_matcher = data_manager_->GetPOSMatcher();

  Node lnode, rnode;
  lnode.Init();
  rnode.Init();
  lnode.node_type = Node::NOR_NODE;
  rnode.node_type = Node::NOR_NODE;
  // "助詞"
  lnode.rid = pos_matcher->GetAcceptableParticleAtBeginOfSegmentId();
  // "名詞,サ変".
  rnode.lid = pos_matcher->GetUnknownId();
  EXPECT_TRUE(segmenter->IsBoundary(&lnode, &rnode, false));

  lnode.attributes |= Node::STARTS_WITH_PARTICLE;
  EXPECT_FALSE(segmenter->IsBoundary(&lnode, &rnode, false));
}

void DataManagerTestBase::ConnectorTest_RandomValueCheck() {
  scoped_ptr<const ConnectorInterface> connector(
      ConnectorBase::CreateFromDataManager(*data_manager_));
  ASSERT_TRUE(connector.get() != NULL);

  EXPECT_EQ(expected_resolution_, connector->GetResolution());
  for (ConnectionFileReader reader(GetFilePath(connection_txt_file_));
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
    if (cost == ConnectorInterface::kInvalidCost) {
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
  scoped_ptr<SuggestionFilter> suggestion_filter;
  {
    const char *data = NULL;
    size_t size;
    data_manager_->GetSuggestionFilterData(&data, &size);
    suggestion_filter.reset(new SuggestionFilter(data, size));
  }

  // Load the original suggestion filter from file.
  hash_set<string> suggestion_filter_set;

  vector<string> files;
  Util::SplitStringUsing(suggestion_filter_files_, ",", &files);
  for (size_t i = 0; i < files.size(); ++i) {
    const string filter_file = GetFilePath(files[i]);
    InputFileStream input(filter_file.c_str());
    CHECK(input) << "cannot open: " << filter_file;
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

  vector<string> dic_files;
  Util::SplitStringUsing(dictionary_files_, ",", &dic_files);
  size_t false_positives = 0;
  size_t num_words = 0;
  for (size_t i = 0; i < dic_files.size(); ++i) {
    LOG(INFO) << dic_files[i];
    const string dic_file = GetFilePath(dic_files[i]);
    InputFileStream input(dic_file.c_str());
    CHECK(input) << "cannot open: " << dic_file;
    vector<string> fields;
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
  const CounterSuffixEntry *suffix_array = nullptr;
  size_t size = 0;
  data_manager_->GetCounterSuffixSortedArray(&suffix_array, &size);

  const char *prev_suffix = "";  // The smallest string.
  for (size_t i = 0; i < size; ++i) {
    const CounterSuffixEntry &entry = suffix_array[i];

    // |entry.size| must be the length of |entry.suffix|.
    EXPECT_EQ(entry.size, strlen(entry.suffix));

    // Check if the array is sorted in ascending order of suffix string.
    EXPECT_GE(0, strcmp(prev_suffix, entry.suffix));
    prev_suffix = entry.suffix;
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
}

}  // namespace mozc
