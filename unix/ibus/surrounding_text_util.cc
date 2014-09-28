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

#include "unix/ibus/surrounding_text_util.h"

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"

namespace mozc {
namespace ibus {

bool SurroundingTextUtil::GetSafeDelta(guint from, guint to, int32 *delta) {
  DCHECK(delta);

  static_assert(sizeof(int64) >= sizeof(guint),
                "int64 must be sufficient to store a guint value.");
  static_assert(sizeof(int64) == sizeof(llabs(0)),
                "|llabs(0)| must returns a 64-bit integer.");
  const int64 kInt32AbsMax =
      llabs(static_cast<int64>(numeric_limits<int32>::max()));
  const int64 kInt32AbsMin =
      llabs(static_cast<int64>(numeric_limits<int32>::min()));
  const int64 kInt32SafeAbsMax =
      min(kInt32AbsMax, kInt32AbsMin);

  const int64 diff = static_cast<int64>(from) - static_cast<int64>(to);
  if (llabs(diff) > kInt32SafeAbsMax) {
    return false;
  }

  *delta = static_cast<int32>(diff);
  return true;
}

namespace {

// Moves |iter| with |skip_count| characters.
// Returns false if |iter| reaches to the end before skipping
// |skip_count| characters.
bool Skip(ConstChar32Iterator *iter, size_t skip_count) {
  for (size_t i = 0; i < skip_count; ++i) {
    if (iter->Done()) {
      return false;
    }
    iter->Next();
  }
  return true;
}

// Returns true if |prefix_iter| is the prefix of |iter|.
// Returns false if |prefix_iter| is an empty sequence.
// Otherwise returns false.
// This function receives ConstChar32Iterator as pointer because
// ConstChar32Iterator is defined as non-copyable.
bool StartsWith(ConstChar32Iterator *iter,
                ConstChar32Iterator *prefix_iter) {
  if (iter->Done() || prefix_iter->Done()) {
    return false;
  }

  while (true) {
    if (iter->Get() != prefix_iter->Get()) {
      return false;
    }
    prefix_iter->Next();
    if (prefix_iter->Done()) {
      return true;
    }
    iter->Next();
    if (iter->Done()) {
      return false;
    }
  }
}


// Returns true if |surrounding_text| contains |selected_text|
// from |cursor_pos| to |*anchor_pos|.
// Otherwise returns false.
bool SearchAnchorPosForward(
    const string &surrounding_text,
    const string &selected_text,
    size_t selected_chars_len,
    guint cursor_pos,
    guint *anchor_pos) {

  ConstChar32Iterator iter(surrounding_text);
  // Move |iter| to cursor pos.
  if (!Skip(&iter, cursor_pos)) {
    return false;
  }

  ConstChar32Iterator sel_iter(selected_text);
  if (!StartsWith(&iter, &sel_iter)) {
    return false;
  }
  *anchor_pos = cursor_pos + selected_chars_len;
  return true;
}

// Returns true if |surrounding_text| contains |selected_text|
// from |*anchor_pos| to |cursor_pos|.
// Otherwise returns false.
bool SearchAnchorPosBackward(
    const string &surrounding_text,
    const string &selected_text,
    size_t selected_chars_len,
    guint cursor_pos,
    guint *anchor_pos) {
  if (cursor_pos < selected_chars_len) {
    return false;
  }

  ConstChar32Iterator iter(surrounding_text);
  // Skip |iter| to (potential) anchor pos.
  const guint skip_count = cursor_pos - selected_chars_len;
  DCHECK_LE(skip_count, cursor_pos);
  if (!Skip(&iter, skip_count)) {
    return false;
  }

  ConstChar32Iterator sel_iter(selected_text);
  if (!StartsWith(&iter, &sel_iter)) {
    return false;
  }
  *anchor_pos = cursor_pos - selected_chars_len;
  return true;
}

}  // namespace

bool SurroundingTextUtil::GetAnchorPosFromSelection(
    const string &surrounding_text,
    const string &selected_text,
    guint cursor_pos,
    guint *anchor_pos) {
  DCHECK(anchor_pos);

  if (surrounding_text.empty()) {
    return false;
  }

  if (selected_text.empty()) {
    return false;
  }

  const size_t selected_chars_len = Util::CharsLen(selected_text);

  if (SearchAnchorPosForward(surrounding_text, selected_text,
                             selected_chars_len,
                             cursor_pos, anchor_pos)) {
    return true;
  }

  return SearchAnchorPosBackward(surrounding_text, selected_text,
                                 selected_chars_len,
                                 cursor_pos, anchor_pos);
}

}  // namespace ibus
}  // namespace mozc
