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

#include "rewriter/merger_rewriter.h"

#include <string>

#include "base/system_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {

// Tests in which order methods of each instance should be called
// and what value should be returned.
class TestRewriter : public RewriterInterface {
 public:
  TestRewriter(string *buffer, const string &name, bool return_value)
      : buffer_(buffer), name_(name), return_value_(return_value),
        capability_(RewriterInterface::CONVERSION) {}

  TestRewriter(string *buffer, const string &name, bool return_value,
               int capability)
      : buffer_(buffer), name_(name), return_value_(return_value),
        capability_(capability) {}

  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const {
    buffer_->append(name_ + ".Rewrite();");
    return return_value_;
  }

  virtual void set_capability(int capability) {
    capability_ = capability;
  }

  virtual int capability(const ConversionRequest &request) const {
    return capability_;
  }

  virtual bool Focus(Segments *segments,
                     size_t segment_index,
                     int candidate_index) const {
    buffer_->append(name_ + ".Focus();");
    return return_value_;
  }

  virtual void Finish(const ConversionRequest &request, Segments *segments) {
    buffer_->append(name_ + ".Finish();");
  }

  virtual bool Sync() {
    buffer_->append(name_ + ".Sync();");
    return return_value_;
  }

  virtual bool Reload() {
    buffer_->append(name_ + ".Reload();");
    return return_value_;
  }

  virtual void Clear() {
    buffer_->append(name_ + ".Clear();");
  }

 private:
  string *buffer_;
  const string name_;
  const bool return_value_;
  int capability_;
};

class MergerRewriterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::Config default_config;
    config::ConfigHandler::GetDefaultConfig(&default_config);
    config::ConfigHandler::SetConfig(default_config);
  }
};

TEST_F(MergerRewriterTest, Rewrite) {
  string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request;

  segments.set_request_type(Segments::CONVERSION);
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ("a.Rewrite();"
            "b.Rewrite();"
            "c.Rewrite();",
            call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ("a.Rewrite();"
            "b.Rewrite();"
            "c.Rewrite();"
            "d.Rewrite();",
            call_result);
}

TEST_F(MergerRewriterTest, RewriteSuggestion) {
  string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request;

  segments.set_request_type(Segments::SUGGESTION);
  merger.AddRewriter(
      new TestRewriter(&call_result, "a", true, RewriterInterface::SUGGESTION));

  EXPECT_EQ(0, segments.conversion_segments_size());
  Segment *segment = segments.push_back_segment();
  EXPECT_EQ(1, segments.conversion_segments_size());

  EXPECT_EQ(0, segment->candidates_size());
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  EXPECT_EQ(4, segment->candidates_size());
  EXPECT_EQ(3, GET_CONFIG(suggestions_size));

  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ("a.Rewrite();", call_result);

  EXPECT_EQ(3, segment->candidates_size());
}

TEST_F(MergerRewriterTest, RewriteSuggestionWithMixedConversion) {
  string call_result;
  MergerRewriter merger;
  Segments segments;

  // Initialize a ConversionRequest with mixed_conversion == true, which
  // should result that the merger rewriter does not trim exceeded suggestions.
  const composer::Composer *kNullComposer = NULL;
  commands::Request commands_request;
  commands_request.set_mixed_conversion(true);
  ConversionRequest request(kNullComposer, &commands_request);
  EXPECT_TRUE(request.request().mixed_conversion());

  segments.set_request_type(Segments::SUGGESTION);
  merger.AddRewriter(
      new TestRewriter(&call_result, "a", true, RewriterInterface::SUGGESTION));

  EXPECT_EQ(0, segments.conversion_segments_size());
  Segment *segment = segments.push_back_segment();
  EXPECT_EQ(1, segments.conversion_segments_size());

  EXPECT_EQ(0, segment->candidates_size());
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  segment->push_back_candidate();
  EXPECT_EQ(4, segment->candidates_size());
  EXPECT_EQ(3, GET_CONFIG(suggestions_size));

  EXPECT_TRUE(merger.Rewrite(request, &segments));
  EXPECT_EQ("a.Rewrite();", call_result);

  // If mixed_conversion is true, the suggestions are not deleted.
  EXPECT_EQ(4, segment->candidates_size());
}

TEST_F(MergerRewriterTest, RewriteCheckTest) {
  string call_result;
  MergerRewriter merger;
  Segments segments;
  const ConversionRequest request;
  merger.AddRewriter(new TestRewriter(
      &call_result, "a", false,
      RewriterInterface::CONVERSION));
  merger.AddRewriter(new TestRewriter(
      &call_result, "b", false,
      RewriterInterface::SUGGESTION));
  merger.AddRewriter(new TestRewriter(
      &call_result, "c", false,
      RewriterInterface::PREDICTION));
  merger.AddRewriter(new TestRewriter(
      &call_result, "d", false,
      RewriterInterface::PREDICTION |
      RewriterInterface::CONVERSION));
  merger.AddRewriter(new TestRewriter(
      &call_result, "e", false,
      RewriterInterface::ALL));

  segments.set_request_type(Segments::CONVERSION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ("a.Rewrite();"
            "d.Rewrite();"
            "e.Rewrite();",
            call_result);
  call_result.clear();

  segments.set_request_type(Segments::PREDICTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ("c.Rewrite();"
            "d.Rewrite();"
            "e.Rewrite();",
            call_result);
  call_result.clear();

  segments.set_request_type(Segments::SUGGESTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ("b.Rewrite();"
            "e.Rewrite();",
            call_result);
  call_result.clear();

  segments.set_request_type(Segments::PARTIAL_SUGGESTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ("b.Rewrite();"
            "e.Rewrite();",
            call_result);
  call_result.clear();

  segments.set_request_type(Segments::PARTIAL_PREDICTION);
  EXPECT_FALSE(merger.Rewrite(request, &segments));
  EXPECT_EQ("c.Rewrite();"
            "d.Rewrite();"
            "e.Rewrite();",
            call_result);
  call_result.clear();
}

TEST_F(MergerRewriterTest, Focus) {
  string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Focus(NULL, 0, 0));
  EXPECT_EQ("a.Focus();"
            "b.Focus();"
            "c.Focus();",
            call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Focus(NULL, 0, 0));
  EXPECT_EQ("a.Focus();"
            "b.Focus();"
            "c.Focus();"
            "d.Focus();",
            call_result);
}

TEST_F(MergerRewriterTest, Finish) {
  string call_result;
  const ConversionRequest request;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  merger.Finish(request, NULL);
  EXPECT_EQ("a.Finish();"
            "b.Finish();"
            "c.Finish();",
            call_result);
}

TEST_F(MergerRewriterTest, Sync) {
  string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Sync());
  EXPECT_EQ("a.Sync();"
            "b.Sync();"
            "c.Sync();",
            call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Sync());
  EXPECT_EQ("a.Sync();"
            "b.Sync();"
            "c.Sync();"
            "d.Sync();",
            call_result);
}

TEST_F(MergerRewriterTest, Reload) {
  string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  EXPECT_FALSE(merger.Reload());
  EXPECT_EQ("a.Reload();"
            "b.Reload();"
            "c.Reload();",
            call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  EXPECT_TRUE(merger.Reload());
  EXPECT_EQ("a.Reload();"
            "b.Reload();"
            "c.Reload();"
            "d.Reload();",
            call_result);
}

TEST_F(MergerRewriterTest, Clear) {
  string call_result;
  MergerRewriter merger;
  merger.AddRewriter(new TestRewriter(&call_result, "a", false));
  merger.AddRewriter(new TestRewriter(&call_result, "b", false));
  merger.AddRewriter(new TestRewriter(&call_result, "c", false));
  merger.Clear();
  EXPECT_EQ("a.Clear();"
            "b.Clear();"
            "c.Clear();",
            call_result);
  merger.AddRewriter(new TestRewriter(&call_result, "d", true));
  call_result.clear();
  merger.Clear();
  EXPECT_EQ("a.Clear();"
            "b.Clear();"
            "c.Clear();"
            "d.Clear();",
            call_result);
}

}  // namespace mozc
