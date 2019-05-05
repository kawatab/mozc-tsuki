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

#include "converter/key_corrector.h"

#include <string>

#include "base/port.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

TEST(KeyCorrectorTest, KeyCorrectorBasicTest) {
  EXPECT_FALSE(KeyCorrector::IsValidPosition(
      KeyCorrector::InvalidPosition()));

  EXPECT_TRUE(KeyCorrector::IsInvalidPosition(
      KeyCorrector::InvalidPosition()));

  {
    KeyCorrector corrector("", KeyCorrector::KANA, 0);
    EXPECT_EQ(KeyCorrector::KANA, corrector.mode());
    EXPECT_FALSE(corrector.IsAvailable());
  }

  {
    KeyCorrector corrector("", KeyCorrector::ROMAN, 0);
    EXPECT_EQ(KeyCorrector::ROMAN, corrector.mode());
    EXPECT_FALSE(corrector.IsAvailable());
  }

  {
    KeyCorrector corrector("てすと", KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("てすと", corrector.original_key());
    corrector.Clear();
    EXPECT_FALSE(corrector.IsAvailable());
  }
}

TEST(KeyCorrectorTest, KeyCorrectorKanaTest) {
  const string input = "みんあであそぼう";
  KeyCorrector corrector(input, KeyCorrector::KANA, 0);
  EXPECT_FALSE(corrector.IsAvailable());
  EXPECT_EQ("", corrector.corrected_key());
  EXPECT_EQ("", corrector.original_key());

  EXPECT_EQ(KeyCorrector::InvalidPosition(),
            corrector.GetCorrectedPosition(0));

  EXPECT_EQ(KeyCorrector::InvalidPosition(),
            corrector.GetOriginalPosition(0));
}

TEST(KeyCorrectorTest, KeyCorrectorRomanTest) {
  {
    const string input = "ん";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("ん", corrector.corrected_key());
  }

  {
    const string input = "かん";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("かん", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "かに";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("かに", corrector.corrected_key());
  }

  {
    const string input = "かｍ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("かｍ", corrector.corrected_key());
  }

  {
    const string input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("みんなであそぼう", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    // don't rewrite 1st "ん"
    const string input = "んあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("んあであそぼう", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "こんかいのみんあはこんんでた";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんかいのみんなはこんでた", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "みんあみんいみんうみんえみんおみんんか";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("みんなみんにみんぬみんねみんのみんか",
              corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちは", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "こんんいちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちは", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "しぜんんお";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("しぜんの", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "あんんんたい";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("あんんんたい", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "せにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("せんよう", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "せにゃうせにゅうせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("せんやうせんゆうせんよう", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちはせんよう", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "おんあのここんいちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("おんなのここんにちは", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "きっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("きって", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "きっっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("きっっって", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "きっっっ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("きっっっ", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "っっ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("っっ", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "しｍばし";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("しんばし", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "しｍはししｍぱしー";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("しｍはししんぱしー", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "ちゅごく";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("ちゅうごく", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }

  {
    const string input = "きゅきゅしゃ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("きゅうきゅうしゃ", corrector.corrected_key());
    EXPECT_EQ(input, corrector.original_key());
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanPositionTest) {
  {
    const string input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("みんなであそぼう", corrector.corrected_key());

    EXPECT_EQ(0, corrector.GetCorrectedPosition(0));
    EXPECT_EQ(1, corrector.GetCorrectedPosition(1));

    EXPECT_EQ(3, corrector.GetCorrectedPosition(3));
    EXPECT_EQ(6, corrector.GetCorrectedPosition(6));

    EXPECT_EQ(9, corrector.GetCorrectedPosition(9));
    EXPECT_EQ(12, corrector.GetCorrectedPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(30));

    EXPECT_EQ(0, corrector.GetOriginalPosition(0));
    EXPECT_EQ(1, corrector.GetOriginalPosition(1));

    EXPECT_EQ(3, corrector.GetOriginalPosition(3));
    EXPECT_EQ(6, corrector.GetOriginalPosition(6));

    EXPECT_EQ(9, corrector.GetOriginalPosition(9));
    EXPECT_EQ(12, corrector.GetOriginalPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(30));
  }

  {
    const string input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちは", corrector.corrected_key());

    EXPECT_EQ(0, corrector.GetCorrectedPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(1));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(3));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(6));

    EXPECT_EQ(6, corrector.GetCorrectedPosition(9));
    EXPECT_EQ(9, corrector.GetCorrectedPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(30));

    EXPECT_EQ(0, corrector.GetOriginalPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(1));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(3));
    EXPECT_EQ(9, corrector.GetOriginalPosition(6));

    EXPECT_EQ(12, corrector.GetOriginalPosition(9));
    EXPECT_EQ(15, corrector.GetOriginalPosition(12));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(30));
  }

  {
    const string input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちはせんよう", corrector.corrected_key());

    EXPECT_EQ(0, corrector.GetCorrectedPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetCorrectedPosition(3));
    EXPECT_EQ(6, corrector.GetCorrectedPosition(9));
    EXPECT_EQ(9, corrector.GetCorrectedPosition(12));
    EXPECT_EQ(21, corrector.GetCorrectedPosition(24));
    EXPECT_EQ(24, corrector.GetCorrectedPosition(27));

    EXPECT_EQ(0, corrector.GetOriginalPosition(0));
    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalPosition(3));
    EXPECT_EQ(9, corrector.GetOriginalPosition(6));
    EXPECT_EQ(27, corrector.GetOriginalPosition(24));
    EXPECT_EQ(24, corrector.GetOriginalPosition(21));
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanCorrectedPrefixTest) {
  {
    const string input = "てすと";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    size_t length = 0;

    // same as the original key
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(0, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(1, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(2, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(3, &length));
  }

  {
    const string input = "みんあであそぼう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("みんなであそぼう", corrector.corrected_key());

    const char *output = NULL;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    EXPECT_EQ("みんなであそぼう", string(output, length));

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_EQ("んなであそぼう", string(output, length));

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_EQ("なであそぼう", string(output, length));

    output = corrector.GetCorrectedPrefix(9, &length);
    // same
    EXPECT_TRUE(NULL == output);
  }

  {
    const string input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちは", corrector.corrected_key());

    const char *output = NULL;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    EXPECT_EQ("こんにちは", string(output, length));

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(9, &length);
    EXPECT_TRUE(NULL == output);
  }

  {
    const string input = "こんんにちはせにょう";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちはせんよう", corrector.corrected_key());

    const char *output = NULL;
    size_t length = 0;

    output = corrector.GetCorrectedPrefix(0, &length);
    EXPECT_EQ("こんにちはせんよう", string(output, length));

    output = corrector.GetCorrectedPrefix(3, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(6, &length);
    EXPECT_TRUE(NULL == output);

    output = corrector.GetCorrectedPrefix(9, &length);
    EXPECT_EQ("にちはせんよう", string(output, length));
  }
}

TEST(KeyCorrectorTest, KeyCorrectorRomanGetOriginalOffsetTest) {
  {
    const string input = "てすと";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(3, corrector.GetOriginalOffset(0, 3));
    EXPECT_EQ(6, corrector.GetOriginalOffset(0, 6));
    EXPECT_EQ(9, corrector.GetOriginalOffset(0, 9));

    EXPECT_EQ(3, corrector.GetOriginalOffset(3, 3));
    EXPECT_EQ(6, corrector.GetOriginalOffset(3, 6));
  }

  {
    const string input = "みんあ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(3, corrector.GetOriginalOffset(0, 3));
    EXPECT_EQ(6, corrector.GetOriginalOffset(0, 6));
    EXPECT_EQ(9, corrector.GetOriginalOffset(0, 9));
  }

  {
    const string input = "きっって";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(12, corrector.GetOriginalOffset(0, 9));
  }

  {
    const string input = "こんんにちは";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ("こんにちは", corrector.corrected_key());

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(0, 3));
    EXPECT_EQ(9, corrector.GetOriginalOffset(0, 6));
    EXPECT_EQ(12, corrector.GetOriginalOffset(0, 9));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(3, 3));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(3, 6));

    EXPECT_EQ(KeyCorrector::InvalidPosition(),
              corrector.GetOriginalOffset(6, 3));

    EXPECT_EQ(3, corrector.GetOriginalOffset(9, 3));

    EXPECT_EQ(6, corrector.GetOriginalOffset(9, 6));
  }
}

// Check if UCS4 is supported. b/3386634
TEST(KeyCorrectorTest, UCS4IsAvailable) {
  {
    const string input = "𠮟";  // UCS4 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }

  {
    const string input = "こ";      // UCS2 char in UTF8
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
  }
}

TEST(KeyCorrectorTest, UCS4Test) {
  {
    const string input = "😁みんあ";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 0);
    EXPECT_TRUE(corrector.IsAvailable());
    EXPECT_EQ(7, corrector.GetOriginalOffset(0, 7));
    EXPECT_EQ(10, corrector.GetOriginalOffset(0, 10));
    EXPECT_EQ(13, corrector.GetOriginalOffset(0, 13));
  }
}

// Should not rewrite the character which is at the beginning of current input
TEST(KeyCorrectorTest, Bug3046266Test) {
  {
    const string input = "かんあか";
    KeyCorrector corrector(input, KeyCorrector::ROMAN, 6);  // history_size = 6
    EXPECT_TRUE(corrector.IsAvailable());
    size_t length = 0;

    // same as the original key
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(0, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(1, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(2, &length));
    EXPECT_TRUE(NULL == corrector.GetCorrectedPrefix(3, &length));
  }
}

}  // namespace
}  // namespace mozc
