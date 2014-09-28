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

#include "base/text_normalizer.h"

#include <string>

#include "base/util.h"

namespace mozc {
namespace {

// http://www.ingrid.org/java/i18n/unicode-sjis.html

// Unicode vender specific character table:
// http://www.ingrid.org/java/i18n/unicode-sjis.html
// http://hp.vector.co.jp/authors/VA010341/unicode/
// http://www.notoinsatu.co.jp/font/omake/OTF_other.pdf
//
// Example: WAVE_DASH / FULLWIDTH TILDE
// http://ja.wikipedia.org/wiki/%E6%B3%A2%E3%83%80%E3%83%83%E3%82%B7%E3%83%A5
// Windows CP932 (shift-jis) maps WAVE_DASH to FULL_WIDTH_TILDA.
// Since the font of WAVE-DASH is ugly on Windows, here we convert WAVE-DHASH to
// FULL_WIDTH_TILDA as CP932 does.
#ifdef OS_WIN
inline char32 ConvertVenderSpecificCharacter(char32 c) {
  switch (c) {
    case 0x00A5:   // YEN SIGN
      return 0x005C;   // REVERSE SOLIDUS
      break;
    case 0x203E:   // OVERLINE
      return 0x007E;  // TILDE
      break;
    case 0x301C:  // WAVE DASH
      return 0xFF5E;   // FULLWIDTH TILDE
      break;
    case 0x2016:   // DOUBLE VERTICAL LINE
      return 0x2225;   // PARALLEL TO
      break;
    case 0x2212:  // MINUS SIGN
      return 0xFF0D;   // FULLWIDTH HYPHEN MINUS
      break;
    case 0x00A2:   // CENT SIGN
      return 0xFFE0;    // FULLWIDTH CENT SIGN
      break;
    case 0x00A3:   // POUND SIGN
      return 0xFFE1;   // FULLWIDTH POUND SIGN
      break;
    case 0x00AC:   // NOT SIGN
      return 0xFFE2;   // FULLWIDTH NOT SIGN
      break;
    default:
      return c;
      break;
  }
}

#else   // MAC & Linux
inline char32 ConvertVenderSpecificCharacter(char32 c) {
  return c;
}
#endif

void ConvertVenderSpecificString(StringPiece input, string *output) {
  output->clear();
  for (ConstChar32Iterator iter(input); !iter.Done(); iter.Next()) {
    Util::UCS4ToUTF8Append(ConvertVenderSpecificCharacter(iter.Get()), output);
  }
}

}  // namespace

void TextNormalizer::NormalizePreeditText(StringPiece input, string *output) {
  string tmp;
  // This is a workaround for hiragana v'
  //  Util::StringReplace(input, "ゔ", "ヴ", true, &tmp);
  Util::StringReplace(input, "\xE3\x82\x94", "\xE3\x83\xB4", true, &tmp);
  ConvertVenderSpecificString(tmp, output);
}

void TextNormalizer::NormalizeTransliterationText(StringPiece input,
                                                  string *output) {
  // Do the same thing with NormalizePreeditText at this morment.
  NormalizePreeditText(input, output);
}

void TextNormalizer::NormalizeConversionText(StringPiece input,
                                             string *output) {
  ConvertVenderSpecificString(input, output);
}

void TextNormalizer::NormalizeCandidateText(StringPiece input, string *output) {
  ConvertVenderSpecificString(input, output);
}

}  // namespace mozc
