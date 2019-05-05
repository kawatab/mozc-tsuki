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

#include "win32/base/string_util.h"

#include <memory>

#include "base/util.h"
#include "protocol/commands.pb.h"

using std::unique_ptr;

namespace mozc {
namespace win32 {
namespace {

const size_t kMaxReadingChars = 512;

void UTF8ToSJIS(StringPiece input, string *output) {
  std::wstring utf16;
  Util::UTF8ToWide(input, &utf16);
  if (utf16.empty()) {
    output->clear();
    return;
  }

  const int kCodePageShiftJIS = 932;

  const int output_length_without_null = ::WideCharToMultiByte(
      kCodePageShiftJIS, 0, utf16.data(), utf16.size(), nullptr, 0, nullptr,
      nullptr);
  if (output_length_without_null == 0) {
    output->clear();
    return;
  }

  unique_ptr<char[]> sjis(new char[output_length_without_null]);
  const int actual_output_length_without_null = ::WideCharToMultiByte(
      kCodePageShiftJIS, 0, utf16.data(), utf16.size(), sjis.get(),
      output_length_without_null, nullptr, nullptr);
  if (output_length_without_null != actual_output_length_without_null) {
    output->clear();
    return;
  }

  output->assign(sjis.get(), actual_output_length_without_null);
}

}  // namespace

std::wstring StringUtil::KeyToReading(StringPiece key) {
  string katakana;
  Util::HiraganaToKatakana(key, &katakana);

  DWORD lcid = MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),
                        SORT_JAPANESE_XJIS);
  string sjis;
  UTF8ToSJIS(katakana, &sjis);

  // Convert "\x81\x65" (backquote in SJIFT-JIS) to ` by myself since
  // LCMapStringA converts it to ' for some reason.
  string sjis2;
  mozc::Util::StringReplace(sjis, "\x81\x65", "`", true, &sjis2);

  const size_t halfwidth_len_without_null = ::LCMapStringA(
      lcid, LCMAP_HALFWIDTH, sjis2.c_str(), sjis2.size(), nullptr, 0);
  if (halfwidth_len_without_null == 0) {
    return L"";
  }

  if (halfwidth_len_without_null >= kMaxReadingChars) {
    return L"";
  }

  unique_ptr<char[]> halfwidth_chars(new char[halfwidth_len_without_null]);
  const size_t actual_halfwidth_len_without_null = ::LCMapStringA(
      lcid, LCMAP_HALFWIDTH, sjis2.c_str(), sjis2.size(), halfwidth_chars.get(),
      halfwidth_len_without_null);
  if (halfwidth_len_without_null != actual_halfwidth_len_without_null) {
    return L"";
  }
  const UINT cp_sjis = 932;  // ANSI/OEM - Japanese, Shift-JIS
  const int output_length_without_null = ::MultiByteToWideChar(
      cp_sjis, 0, halfwidth_chars.get(), actual_halfwidth_len_without_null,
      nullptr, 0);
  if (output_length_without_null == 0) {
    return L"";
  }

  unique_ptr<wchar_t[]> wide_output(new wchar_t[output_length_without_null]);
  const int actual_output_length_without_null = ::MultiByteToWideChar(
      cp_sjis, 0, halfwidth_chars.get(), actual_halfwidth_len_without_null,
      wide_output.get(), output_length_without_null);
  if (output_length_without_null != actual_output_length_without_null) {
    return L"";
  }
  return std::wstring(wide_output.get(), actual_output_length_without_null);
}

string StringUtil::KeyToReadingA(StringPiece key) {
  string ret;
  mozc::Util::WideToUTF8(KeyToReading(key), &ret);
  return ret;
}

std::wstring StringUtil::ComposePreeditText(const commands::Preedit &preedit) {
  std::wstring value;
  for (int i = 0; i < preedit.segment_size(); ++i) {
    std::wstring segment_value;
    mozc::Util::UTF8ToWide(preedit.segment(i).value(), &segment_value);
    value.append(segment_value);
  }
  return value;
}

}  // namespace win32
}  // namespace mozc
