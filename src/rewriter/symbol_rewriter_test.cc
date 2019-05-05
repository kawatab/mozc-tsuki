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

#include "rewriter/symbol_rewriter.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {

namespace {
void AddSegment(const string &key, const string &value,
                Segments *segments) {
  Segment *seg = segments->push_back_segment();
  seg->set_key(key);
  Segment::Candidate *candidate = seg->add_candidate();
  candidate->Init();
  candidate->value = key;
  candidate->content_key = key;
  candidate->content_value = value;
}

void AddCandidate(const string &value, Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->value = value;
  candidate->content_key = segment->key();
  candidate->content_value = value;
}

bool HasCandidateAndDescription(const Segments &segments,
                                int index,
                                const string &key,
                                const string &description) {
  CHECK_GT(segments.segments_size(), index);
  bool check_description = !description.empty();

  for (size_t i = 0; i < segments.segment(index).candidates_size(); ++i) {
    const Segment::Candidate &candidate = segments.segment(index).candidate(i);
    if (candidate.value == key) {
      if (check_description) {
        bool result = candidate.description == description;
        return result;
      } else {
        return true;
      }
    }
  }
  return false;
}

bool HasCandidate(const Segments &segments, int index, const string &value) {
  return HasCandidateAndDescription(segments, index, value, "");
}
}  // namespace

class SymbolRewriterTest : public ::testing::Test {
 protected:
  SymbolRewriterTest() = default;
  ~SymbolRewriterTest() override = default;

  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    // We cannot use mock converter here because SymbolRewriter uses
    // ResizeSegment of converter implementation. However, SymbolRewriter is
    // independent of underlying dictionary and, hence, we can use a converter
    // with mock data.
    engine_.reset(MockDataEngineFactory::Create());
    converter_ = engine_->GetConverter();

    data_manager_.reset(new testing::MockDataManager);
  }

  std::unique_ptr<EngineInterface> engine_;
  const ConverterInterface *converter_;
  std::unique_ptr<testing::MockDataManager> data_manager_;
};

// Note that these tests are using default symbol dictionary.
// Test result can be changed if symbol dictionary is modified.
// TODO(toshiyuki): Modify symbol rewriter so that we can use symbol dictionary
// for testing.
TEST_F(SymbolRewriterTest, TriggerRewriteTest) {
  SymbolRewriter symbol_rewriter(converter_, data_manager_.get());
  const ConversionRequest request;

  {
    Segments segments;
    AddSegment("ー", "test", &segments);
    AddSegment(">", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(HasCandidate(segments, 0, "→"));
  }
  {
    Segments segments;
    AddSegment("ー", "test", &segments);
    AddSegment("ー", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(HasCandidate(segments, 0, "―"));
    EXPECT_TRUE(HasCandidate(segments, 1, "―"));
  }
}

TEST_F(SymbolRewriterTest, TriggerRewriteEntireTest) {
  SymbolRewriter symbol_rewriter(converter_, data_manager_.get());
  const ConversionRequest request;
  {
    Segments segments;
    AddSegment("ー", "test", &segments);
    AddSegment(">", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.RewriteEntireCandidate(request, &segments));
    EXPECT_TRUE(HasCandidate(segments, 0, "→"));
  }
  {
    Segments segments;
    AddSegment("ー", "test", &segments);
    AddSegment("ー", "test", &segments);
    EXPECT_FALSE(symbol_rewriter.RewriteEntireCandidate(request, &segments));
  }
}

TEST_F(SymbolRewriterTest, TriggerRewriteEachTest) {
  SymbolRewriter symbol_rewriter(converter_, data_manager_.get());
  {
    Segments segments;
    AddSegment("ー", "test", &segments);
    AddSegment(">", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.RewriteEachCandidate(&segments));
    EXPECT_EQ(2, segments.segments_size());
    EXPECT_TRUE(HasCandidate(segments, 0, "―"));
    EXPECT_FALSE(HasCandidate(segments, 0, "→"));
    EXPECT_TRUE(HasCandidate(segments, 1, "〉"));
  }
}

TEST_F(SymbolRewriterTest, TriggerRewriteDescriptionTest) {
  SymbolRewriter symbol_rewriter(converter_, data_manager_.get());
  {
    Segments segments;
    AddSegment("したつき", "test", &segments);
    EXPECT_TRUE(symbol_rewriter.RewriteEachCandidate(&segments));
    EXPECT_EQ(1, segments.segments_size());
    EXPECT_TRUE(HasCandidateAndDescription(segments, 0, "₍",
                                           "下付き文字(始め丸括弧)"));
  }
}

TEST_F(SymbolRewriterTest, InsertAfterSingleKanjiAndT13n) {
  SymbolRewriter symbol_rewriter(converter_, data_manager_.get());
  const ConversionRequest request;
  {
    Segments segments;
    AddSegment("てん", "てん", &segments);
    Segment *seg = segments.mutable_segment(0);
    // Add 15 single-kanji and transliterated candidates
    AddCandidate("点", seg);
    AddCandidate("転", seg);
    AddCandidate("天", seg);
    AddCandidate("てん", seg);
    AddCandidate("テン", seg);
    AddCandidate("展", seg);
    AddCandidate("店", seg);
    AddCandidate("典", seg);
    AddCandidate("添", seg);
    AddCandidate("填", seg);
    AddCandidate("顛", seg);
    AddCandidate("辿", seg);
    AddCandidate("纏", seg);
    AddCandidate("甜", seg);
    AddCandidate("貼", seg);

    EXPECT_TRUE(symbol_rewriter.Rewrite(request, &segments));
    EXPECT_GT(segments.segment(0).candidates_size(), 16);
    for (int i = 0; i < 16; ++i) {
      const string &value = segments.segment(0).candidate(i).value;
      EXPECT_FALSE(Util::IsScriptType(value, Util::UNKNOWN_SCRIPT))
          << i << ": " << value;
    }
  }
}

TEST_F(SymbolRewriterTest, SetKey) {
  SymbolRewriter symbol_rewriter(converter_, data_manager_.get());
  Segments segments;
  const ConversionRequest request;

  Segment *segment = segments.push_back_segment();
  const string kKey = "てん";
  segment->set_key(kKey);
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->key = "strange key";
  candidate->value = "strange value";
  candidate->content_key = "strange key";
  candidate->content_value = "strange value";
  EXPECT_EQ(1, segment->candidates_size());
  EXPECT_TRUE(symbol_rewriter.Rewrite(request, &segments));
  EXPECT_GT(segment->candidates_size(), 1);
  for (size_t i = 1; i < segment->candidates_size(); ++i) {
    EXPECT_EQ(kKey, segment->candidate(i).key);
  }
}

TEST_F(SymbolRewriterTest, MobileEnvironmentTest) {
  ConversionRequest convreq;
  commands::Request request;
  convreq.set_request(&request);
  SymbolRewriter rewriter(converter_, data_manager_.get());

  {
    request.set_mixed_conversion(true);
    EXPECT_EQ(RewriterInterface::ALL, rewriter.capability(convreq));
  }

  {
    request.set_mixed_conversion(false);
    EXPECT_EQ(RewriterInterface::CONVERSION, rewriter.capability(convreq));
  }
}

TEST_F(SymbolRewriterTest, ExpandSpace) {
  SymbolRewriter symbol_rewriter(converter_, data_manager_.get());
  Segments segments;
  const ConversionRequest request;

  Segment *segment = segments.push_back_segment();
  segment->set_key(" ");
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->Init();
  candidate->key = " ";
  candidate->value = " ";
  candidate->content_key = " ";
  candidate->content_value = " ";
  candidate->PushBackInnerSegmentBoundary(1, 1, 1, 1);

  EXPECT_TRUE(symbol_rewriter.Rewrite(request, &segments));
  EXPECT_LE(2, segment->candidates_size());

  const Segment::Candidate &cand0 = segment->candidate(0);
  EXPECT_EQ(" ", cand0.key);
  EXPECT_EQ(" ", cand0.value);
  EXPECT_EQ(" ", cand0.content_key);
  EXPECT_EQ(" ", cand0.content_value);
  ASSERT_EQ(1, cand0.inner_segment_boundary.size());
  EXPECT_EQ(Segment::Candidate::EncodeLengths(1, 1, 1, 1),
            cand0.inner_segment_boundary[0]);

  const char *kFullWidthSpace = "　";
  const Segment::Candidate &cand1 = segment->candidate(1);
  EXPECT_EQ(" ", cand1.key);
  EXPECT_EQ(kFullWidthSpace, cand1.value);
  EXPECT_EQ(" ", cand1.content_key);
  EXPECT_EQ(kFullWidthSpace, cand1.content_value);
  EXPECT_TRUE(cand1.inner_segment_boundary.empty());
}

}  // namespace mozc
