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

#include "base/strings/japanese.h"

#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "testing/gunit.h"

namespace mozc::japanese {
namespace {

TEST(JapaneseUtilTest, HiraganaToKatakana) {
  {
    const std::string input =
        "あいうえおぁぃぅぇぉかきくけこがぎぐげごさしすせそざじずぜぞたちつてと"
        "だぢづでどっなにぬねのはひふへほばびぶべぼぱぴぷぺぽまみむめもやゆよゃ"
        "ゅょらりるれろわゎをんゔ";
    std::string output;
    HiraganaToKatakana(input, &output);
    EXPECT_EQ(
        output,
        "アイウエオァィゥェォカキクケコガギグゲゴサシスセソザジズゼゾタチツテト"
        "ダヂヅデドッナニヌネノハヒフヘホバビブベボパピプペポマミムメモヤユヨャ"
        "ュョラリルレロワヮヲンヴ");
  }
  {
    const std::string input = "わたしのなまえはなかのですうまーよろしゅう";
    std::string output;
    HiraganaToKatakana(input, &output);
    EXPECT_EQ(output, "ワタシノナマエハナカノデスウマーヨロシュウ");
  }
  {
    const std::string input = "グーグル工藤よろしくabc";
    std::string output;
    HiraganaToKatakana(input, &output);
    EXPECT_EQ(output, "グーグル工藤ヨロシクabc");
  }
}

TEST(JapaneseUtilTest, KatakanaToHiragana) {
  {
    const std::string input =
        "アイウエオァィゥェォカキクケコガギグゲゴサシスセソザジズゼゾタチツテト"
        "ダヂヅデドッナニヌネノハヒフヘホバビブベボパピプペポマミムメモヤユヨャ"
        "ュョラリルレロワヮヲンヰヱヴ";
    std::string output;
    KatakanaToHiragana(input, &output);
    EXPECT_EQ(
        output,
        "あいうえおぁぃぅぇぉかきくけこがぎぐげごさしすせそざじずぜぞたちつてと"
        "だぢづでどっなにぬねのはひふへほばびぶべぼぱぴぷぺぽまみむめもやゆよゃ"
        "ゅょらりるれろわゎをんゐゑゔ");
  }
  {
    const std::string input = "ワタシノナマエハナカノデスウマーヨロシュウ";
    std::string output;
    KatakanaToHiragana(input, &output);
    EXPECT_EQ(output, "わたしのなまえはなかのですうまーよろしゅう");
  }
  {
    const std::string input = "グーグル工藤ヨロシクabc";
    std::string output;
    KatakanaToHiragana(input, &output);
    EXPECT_EQ(output, "ぐーぐる工藤よろしくabc");
  }
}

TEST(JapaneseUtilTest, RomanjiToHiragana) {
  struct {
    const char *input;
    const char *expected;
  } kTestCases[] = {
      {"watasinonamaehatakahashinoriyukidesu",
       "わたしのなまえはたかはしのりゆきです"},
      {"majissukamajiyabexe", "まじっすかまじやべぇ"},
      {"kk", "っk"},
      {"xyz", "xyz"},
  };
  for (const auto &test_case : kTestCases) {
    std::string actual;
    RomanjiToHiragana(test_case.input, &actual);
    EXPECT_EQ(actual, test_case.expected);
  }
}

TEST(JapaneseUtilTest, HiraganaToRomaji) {
  struct {
    const char *input;
    const char *expected;
  } kTestCases[] = {
      {"わたしのなまえはたかはしのりゆきです",
       "watasinonamaehatakahasinoriyukidesu"},
      {"まじっすかまじやべぇ", "mazissukamaziyabexe"},
      {"おっっっ", "oxtuxtuxtu"},
      {"おっっっと", "oxtuxtutto"},
      {"らーめん", "ra-men"},
      {"かんな", "kanna"},
      {"かんnな", "kannna"},
      {"はんにゃ", "hannya"},
      {"はんnにゃ", "hannnya"},
      {"xyz", "xyz"},
  };
  for (const auto &test_case : kTestCases) {
    std::string actual;
    HiraganaToRomanji(test_case.input, &actual);
    EXPECT_EQ(actual, test_case.expected);
  }
}

TEST(JapaneseUtilTest, NormalizeVoicedSoundMark) {
  const std::string input = "僕のう゛ぁいおりん";
  std::string output;
  NormalizeVoicedSoundMark(input, &output);
  EXPECT_EQ(output, "僕のゔぁいおりん");
}

TEST(JapaneseUtilTest, FullWidthAndHalfWidth) {
  std::string output;

  FullWidthToHalfWidth("", &output);
  EXPECT_EQ(output, "");

  HalfWidthToFullWidth("", &output);
  EXPECT_EQ(output, "");

  HalfWidthToFullWidth("abc[]?.", &output);
  EXPECT_EQ(output, "ａｂｃ［］？．");

  HalfWidthToFullWidth("ｲﾝﾀｰﾈｯﾄ｢」", &output);
  EXPECT_EQ(output, "インターネット「」");

  HalfWidthToFullWidth("ｲﾝﾀｰﾈｯﾄグーグル", &output);
  EXPECT_EQ(output, "インターネットグーグル");

  FullWidthToHalfWidth("ａｂｃ［］？．", &output);
  EXPECT_EQ(output, "abc[]?.");

  FullWidthToHalfWidth("インターネット", &output);
  EXPECT_EQ(output, "ｲﾝﾀｰﾈｯﾄ");

  FullWidthToHalfWidth("ｲﾝﾀｰﾈｯﾄグーグル", &output);
  EXPECT_EQ(output, "ｲﾝﾀｰﾈｯﾄｸﾞｰｸﾞﾙ");

  // spaces
  FullWidthToHalfWidth(" 　", &output);  // Half- and full-width spaces
  EXPECT_EQ(output, "  ");               // 2 half-width spaces

  HalfWidthToFullWidth(" 　", &output);  // Half- and full-width spaces
  EXPECT_EQ(output, "　　");             // 2 full-width spaces

  // Spaces are treated as Ascii here
  // Half- and full-width spaces
  FullWidthAsciiToHalfWidthAscii(" 　", &output);
  EXPECT_EQ(output, "  ");  // 2 half-width spaces

  HalfWidthAsciiToFullWidthAscii("  ", &output);
  EXPECT_EQ(output, "　　");  // 2 full-width spaces

  // Half- and full-width spaces
  FullWidthKatakanaToHalfWidthKatakana(" 　", &output);
  EXPECT_EQ(output, " 　");  // Not changed

  // Half- and full-width spaces
  HalfWidthKatakanaToFullWidthKatakana(" 　", &output);
  EXPECT_EQ(output, " 　");  // Not changed
}

TEST(JapaneseUtilTest, AlignTest) {
  using V = std::vector<std::pair<absl::string_view, absl::string_view>>;

  EXPECT_EQ(V({{"ga", "が"}, {"k", "っ"}, {"ko", "こ"}, {"u", "う"}}),
            AlignRomanjiToHiragana("gakkou"));

  EXPECT_EQ(V({{"が", "ga"}, {"っこ", "kko"}, {"う", "u"}}),
            AlignHiraganaToRomanji("がっこう"));

  EXPECT_EQ(V({{"re", "れ"},
               {"si", "し"},
               {"pi", "ぴ"},
               {"no", "の"},
               {"ka", "か"},
               {"l", "l"},
               {"ze", "ぜ"},
               {"nn", "ん"}}),
            AlignRomanjiToHiragana("resipinokalzenn"));
}

}  // namespace
}  // namespace mozc::japanese
