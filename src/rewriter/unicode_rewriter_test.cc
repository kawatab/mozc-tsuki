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

#include "rewriter/unicode_rewriter.h"

#include <cstddef>
#include <cstdlib>
#include <memory>
#include <string>

#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "composer/composer.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "engine/engine_interface.h"
#include "engine/mock_data_engine_factory.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

void AddSegment(const string &key, const string &value, Segments *segments) {
  Segment *seg = segments->add_segment();
  Segment::Candidate *candidate = seg->add_candidate();
  seg->set_key(key);
  candidate->content_key = key;
  candidate->value = value;
  candidate->content_value = value;
}

void InitSegments(const string &key, const string &value, Segments *segments) {
  segments->Clear();
  AddSegment(key, value, segments);
}

bool ContainCandidate(const Segments &segments, const string &candidate) {
  const Segment &segment = segments.segment(0);
  for (size_t i = 0; i < segment.candidates_size(); ++i) {
    if (candidate == segment.candidate(i).value) {
      return true;
    }
  }
  return false;
}

}  // namespace

class UnicodeRewriterTest : public ::testing::Test {
 protected:
  // Workaround for C2512 error (no default appropriate constructor) on MSVS.
  UnicodeRewriterTest() {}
  virtual ~UnicodeRewriterTest() {}

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    engine_.reset(MockDataEngineFactory::Create());
  }

  std::unique_ptr<EngineInterface> engine_;
  const commands::Request &default_request() const {
    return default_request_;
  }
  const config::Config &default_config() const {
    return default_config_;
  }
 private:
  const commands::Request default_request_;
  const config::Config default_config_;
};

TEST_F(UnicodeRewriterTest, UnicodeConversionTest) {
  Segments segments;
  UnicodeRewriter rewriter(engine_->GetConverter());
  const ConversionRequest request;

  struct UCS4UTF8Data {
    const char *ucs4;
    const char *utf8;
  };

  const UCS4UTF8Data kUcs4Utf8Data[] = {
    // Hiragana
    { "U+3042", "あ" },
    { "U+3044", "い" },
    { "U+3046", "う" },
    { "U+3048", "え" },
    { "U+304A", "お" },

    // Katakana
    { "U+30A2", "ア" },
    { "U+30A4", "イ" },
    { "U+30A6", "ウ" },
    { "U+30A8", "エ" },
    { "U+30AA", "オ" },

    // half-Katakana
    { "U+FF71", "ｱ" },
    { "U+FF72", "ｲ" },
    { "U+FF73", "ｳ" },
    { "U+FF74", "ｴ" },
    { "U+FF75", "ｵ" },

    // CJK
    { "U+611B", "愛" },
    { "U+690D", "植" },
    { "U+7537", "男" },

    // Other types (Oriya script)
    { "U+0B00", "\xE0\xAC\x80" },  // "଀"
    { "U+0B01", "ଁ" },  // "ଁ"
    { "U+0B02", "ଂ" },  // "ଂ"

    // Other types (Arabic)
    { "U+0600", "؀" },
    { "U+0601", "؁" },
    { "U+0602", "؂" },

    // Latin-1 support
    { "U+00A0", "\xC2\xA0" },  // " " (nbsp)
    { "U+00A1", "¡" },
  };

  const char* kMozcUnsupportedUtf8[] = {
    // Control characters
    "U+0000", "U+001F", "U+007F", "U+0080", "U+009F",
    // Out of Unicode
    "U+110000",
    // Bidirectional text
    "U+200E", "U+202D",
  };

  // All ascii code would be accepted.
  for (uint32 ascii = 0x20; ascii < 0x7F; ++ascii) {
    const string ucs4 = Util::StringPrintf("U+00%02X", ascii);
    InitSegments(ucs4, ucs4, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_EQ(ascii, segments.segment(0).candidate(0).value.at(0));
  }

  // Mozc accepts Japanese characters
  for (size_t i = 0; i < arraysize(kUcs4Utf8Data); ++i) {
    InitSegments(kUcs4Utf8Data[i].ucs4, kUcs4Utf8Data[i].ucs4, &segments);
    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, kUcs4Utf8Data[i].utf8));
  }

  // Mozc does not accept other characters
  for (size_t i = 0; i < arraysize(kMozcUnsupportedUtf8); ++i) {
    InitSegments(kMozcUnsupportedUtf8[i], kMozcUnsupportedUtf8[i], &segments);
    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  // Invalid style input
  InitSegments("U+1234567", "U+12345678", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("U+XYZ", "U+XYZ", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("12345", "12345", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  InitSegments("U12345", "U12345", &segments);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));
}

TEST_F(UnicodeRewriterTest, MultipleSegment) {
  Segments segments;
  UnicodeRewriter rewriter(engine_->GetConverter());
  const ConversionRequest request;

  // Multiple segments are combined.
  InitSegments("U+0", "U+0", &segments);
  AddSegment("02", "02", &segments);
  AddSegment("0", "0", &segments);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_EQ(1, segments.conversion_segments_size());
  EXPECT_EQ(' ', segments.conversion_segment(0).candidate(0).value.at(0));

  // If the segments is already resized, returns false.
  InitSegments("U+0020", "U+0020", &segments);
  AddSegment("U+0020", "U+0020", &segments);
  segments.set_resized(true);
  EXPECT_FALSE(rewriter.Rewrite(request, &segments));

  // History segment has to be ignored.
  // In this case 1st segment is HISTORY
  // so this rewriting returns true.
  InitSegments("U+0020", "U+0020", &segments);
  AddSegment("U+0020", "U+0020", &segments);
  segments.set_resized(true);
  segments.mutable_segment(0)->set_segment_type(Segment::HISTORY);
  EXPECT_TRUE(rewriter.Rewrite(request, &segments));
  EXPECT_EQ(' ', segments.conversion_segment(0).candidate(0).value.at(0));
}

TEST_F(UnicodeRewriterTest, RewriteToUnicodeCharFormat) {
  UnicodeRewriter rewriter(engine_->GetConverter());
  {  // Typical case
    composer::Composer composer(NULL, &default_request(), &default_config());
    composer.set_source_text("A");
    ConversionRequest request(&composer, &default_request(), &default_config());

    Segments segments;
    AddSegment("A", "A", &segments);

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "U+0041"));
  }

  {  // If source_text is not set, this rewrite is not triggered.
    composer::Composer composer(NULL, &default_request(), &default_config());
    ConversionRequest request(&composer, &default_request(), &default_config());

    Segments segments;
    AddSegment("A", "A", &segments);

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
    EXPECT_FALSE(ContainCandidate(segments, "U+0041"));
  }

  {  // If source_text is not a single character, this rewrite is not
     // triggered.
    composer::Composer composer(NULL, &default_request(), &default_config());
    composer.set_source_text("AB");
    ConversionRequest request(&composer, &default_request(), &default_config());

    Segments segments;
    AddSegment("AB", "AB", &segments);

    EXPECT_FALSE(rewriter.Rewrite(request, &segments));
  }

  {  // Multibyte character is also supported.
    composer::Composer composer(NULL, &default_request(), &default_config());
    composer.set_source_text("愛");
    ConversionRequest request(&composer, &default_request(), &default_config());

    Segments segments;
    AddSegment("あい", "愛", &segments);

    EXPECT_TRUE(rewriter.Rewrite(request, &segments));
    EXPECT_TRUE(ContainCandidate(segments, "U+611B"));
  }
}

}  // namespace mozc
