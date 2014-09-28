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

#include "rewriter/collocation_util.h"

#include <string>

#include "base/string_piece.h"
#include "base/util.h"

namespace mozc {
void CollocationUtil::GetNormalizedScript(
    const StringPiece str, bool remove_number, string *output) {
  output->clear();
  string temp;
  RemoveExtraCharacters(str, remove_number, &temp);
  string temp2;
  // "％" -> "%"
  Util::StringReplace(temp, "\xef\xbc\x85", "%", true, &temp2);
  // "～" -> "〜"
  Util::StringReplace(temp2, "\xef\xbd\x9e", "\xe3\x80\x9c", true, output);
}

bool CollocationUtil::IsNumber(char32 c) {
  if (Util::GetScriptType(c) == Util::NUMBER) {
    return true;
  }

  switch (c) {
    case 0x3007:  // "〇"
    case 0x4e00:  // "一"
    case 0x4e8c:  // "二"
    case 0x4e09:  // "三"
    case 0x56db:  // "四"
    case 0x4e94:  // "五"
    case 0x516d:  // "六"
    case 0x4e03:  // "七"
    case 0x516b:  // "八"
    case 0x4e5d:  // "九"
    case 0x5341:  // "十"
    case 0x767e:  // "百"
    case 0x5343:  // "千"
    case 0x4e07:  // "万"
    case 0x5104:  // "億"
    case 0x5146:  // "兆"
      return true;
    default:
      break;
  }
  return false;
}

void CollocationUtil::RemoveExtraCharacters(
    const StringPiece input, bool remove_number, string *output) {
  for (ConstChar32Iterator iter(input); !iter.Done(); iter.Next()) {
    const char32 w = iter.Get();
    if (((Util::GetScriptType(w) != Util::UNKNOWN_SCRIPT) &&
         (!remove_number || !IsNumber(w))) ||
        w == 0x3005 ||  // "々"
        w == 0x0025 || w == 0xFF05 ||  // "%", "％"
        w == 0x3006 ||  // "〆"
        w == 0x301C || w == 0xFF5E) {  // "〜", "～"
      Util::UCS4ToUTF8Append(w, output);
    }
  }
}
}  // namespace mozc
