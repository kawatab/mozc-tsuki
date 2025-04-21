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

#include "rewriter/correction_rewriter.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/container/serialized_string_array.h"
#include "base/port.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/gunit.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace {

Segment *AddSegment(const absl::string_view key, Segments *segments) {
  Segment *segment = segments->push_back_segment();
  segment->set_key(key);
  return segment;
}

Segment::Candidate *AddCandidate(const absl::string_view key,
                                 const absl::string_view value,
                                 const absl::string_view content_key,
                                 const absl::string_view content_value,
                                 Segment *segment) {
  Segment::Candidate *candidate = segment->add_candidate();
  candidate->key = std::string(key);
  candidate->value = std::string(value);
  candidate->content_key = std::string(content_key);
  candidate->content_value = std::string(content_value);
  return candidate;
}
}  // namespace

class CorrectionRewriterTest : public testing::Test {
 protected:
  CorrectionRewriterTest() {
    convreq_.set_request(&request_);
    convreq_.set_config(&config_);
  }

  void SetUp() override {
    // Create a rewriter with one entry: (TSUKIGIME, gekkyoku, tsukigime)
    const std::vector<absl::string_view> values = {"TSUKIGIME"};
    const std::vector<absl::string_view> errors = {"gekkyoku"};
    const std::vector<absl::string_view> corrections = {"tsukigime"};
    rewriter_ = std::make_unique<CorrectionRewriter>(
        SerializedStringArray::SerializeToBuffer(values, &values_buf_),
        SerializedStringArray::SerializeToBuffer(errors, &errors_buf_),
        SerializedStringArray::SerializeToBuffer(corrections,
                                                 &corrections_buf_));
    config::ConfigHandler::GetDefaultConfig(&config_);
    config_.set_use_spelling_correction(true);
  }

  std::unique_ptr<CorrectionRewriter> rewriter_;
  ConversionRequest convreq_;
  commands::Request request_;
  config::Config config_;

 private:
  std::unique_ptr<uint32_t[]> values_buf_;
  std::unique_ptr<uint32_t[]> errors_buf_;
  std::unique_ptr<uint32_t[]> corrections_buf_;
};

TEST_F(CorrectionRewriterTest, CapabilityTest) {
  EXPECT_EQ(rewriter_->capability(convreq_), RewriterInterface::ALL);
}

TEST_F(CorrectionRewriterTest, RewriteTest) {
  Segments segments;

  Segment *segment = AddSegment("gekkyokuwo", &segments);
  Segment::Candidate *candidate = AddCandidate(
      "gekkyokuwo", "TSUKIGIMEwo", "gekkyoku", "TSUKIGIME", segment);
  candidate->attributes |= Segment::Candidate::RERANKED;

  AddCandidate("gekkyokuwo", "GEKKYOKUwo", "gekkyoku", "GEKKYOKU", segment);

  config_.set_use_spelling_correction(false);

  EXPECT_FALSE(rewriter_->Rewrite(convreq_, &segments));

  config_.set_use_spelling_correction(true);
  EXPECT_TRUE(rewriter_->Rewrite(convreq_, &segments));

  // candidate 0
  EXPECT_EQ(
      segments.conversion_segment(0).candidate(0).attributes,
      (Segment::Candidate::RERANKED | Segment::Candidate::SPELLING_CORRECTION));
  EXPECT_EQ(segments.conversion_segment(0).candidate(0).description,
            "<もしかして: tsukigime>");

  // candidate 1
  EXPECT_EQ(segments.conversion_segment(0).candidate(1).attributes,
            Segment::Candidate::DEFAULT_ATTRIBUTE);
  EXPECT_TRUE(segments.conversion_segment(0).candidate(1).description.empty());
}

}  // namespace mozc
