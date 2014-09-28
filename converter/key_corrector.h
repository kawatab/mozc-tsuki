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

#ifndef MOZC_CONVERTER_KEY_CORRECTOR_H_
#define MOZC_CONVERTER_KEY_CORRECTOR_H_

#include <string>
#include <vector>

#include "base/port.h"

namespace mozc {

class KeyCorrector {
 public:
  enum InputMode {
    ROMAN,
    KANA,
  };

  KeyCorrector(const string &key, InputMode mode, size_t history_size);
  KeyCorrector();
  virtual ~KeyCorrector();

  InputMode mode() const;

  bool CorrectKey(const string &key, InputMode mode, size_t history_size);

  // return corrected key;
  const string &corrected_key() const;

  // return original key;
  const string &original_key() const;

  // return true key correction was done successfully
  bool IsAvailable() const;

  // return the poistion of corrected_key correspoinding
  // to the original_key_pos
  // return InvalidPosition() if invalid pos is passed.
  // Note that the position is not by Unicode Character but by bytes.
  size_t GetCorrectedPosition(size_t original_key_pos) const;

  // return the poistion of original_key correspoinding
  // to the corrected_key_pos
  // return InvalidPosition() if invalid pos is passed.
  // Note that the position is not by Unicode Character but by bytes.
  size_t GetOriginalPosition(size_t corrected_key_pos) const;

  // return true if pos is NOT kInvalidPos
  static bool IsValidPosition(size_t pos);

  // return true if pos is kInvalidPos
  static bool IsInvalidPosition(size_t pos);

  // return kInvalidPos
  static size_t InvalidPosition();

  // return new prefix of string correspoindng to
  // the prefix of the original key at "original_key_pos"
  // if new prefix and original prefix are the same, return NULL.
  // Note that return value won't be NULL terminated.
  // "length" stores the length of return value.
  // We don't allow empty matching (see GetPrefix(15) below)
  //
  // More formally, this function can be defined as:
  // GetNewPrefix(original_key_pos) ==
  //   corrected_key.substr(GetCorrectedPosition(original_key),
  //                        corrected_key.size() -
  //                        GetCorrectedPosition(original_key))
  //
  // Example:
  //  original: "せかいじゅのはっぱ"
  //  corrected: "せかいじゅうのはっぱ"
  //  GetPrefix(0) = "せかいじゅうのはっぱ"
  //  GetPrefix(3) = "かいじゅうのはっぱ"
  //  GetPrefix(9) = "じゅうのはっぱ"
  //  GetPrefix(12) = "ゅうのはっぱ"
  //  GetPrefix(15) = NULL (not "うのはっぱ")
  //                  "う" itself doesn't correspond to the original key,
  //                   so, we don't make a new prefix
  //  GetPrefix(18) = NULL (same as original)
  //
  // Example2:
  //  original: "みんあのほん"
  //  GetPrefix(0) = "みんなのほん"
  //  GetPrefix(3) = "んなのほん"
  //  GetPrefix(9) = "なのほん"
  //  GetPrefix(12) = NULL
  const char *GetCorrectedPrefix(const size_t original_key_pos,
                                 size_t *length) const;

  // This is a helper function for CommonPrefixSearch in Converter.
  // Basically it is equivalent to
  // GetOriginalPosition(GetCorrectedPosition(original_key_pos)
  //                     + new_key_offset) - original_key_pos;
  //
  // Usage:
  // const char *corrected_prefix = GetCorrectedPrefix(original_key_pos,
  //                                                   &length);
  // Node *nodes = Lookup(corrected_prefix, length);
  // for node in nodes {
  //   original_offset = GetOriginalOffset(original_key_pos, node->length);
  //   InsertLattice(original_key_pos, original_offset);
  // }
  //
  // Example:
  //  original:  "せかいじゅのはっぱ"
  //  corrected: "せかいじゅうのはっぱ"
  // GetOffset(0, 3) == 3
  // GetOffset(0, 12) == 12
  // GetOffset(0, 15) == 12
  // GetOffset(0, 18) == 15
  //
  // By combining GetCorrectedPrefix() and GetOriginalOffset(),
  // Converter is able to know the position of the lattice
  size_t GetOriginalOffset(const size_t original_key_pos,
                           const size_t new_key_offset) const;


  // return the cost penalty for the corrected key.
  // The return value is added to the original cost as a penalty.
  static int GetCorrectedCostPenalty(const string &key);

  // clear internal data
  void Clear();

 private:
  bool available_;
  InputMode mode_;
  string corrected_key_;
  string original_key_;
  vector<size_t> alignment_;
  vector<size_t> rev_alignment_;

  DISALLOW_COPY_AND_ASSIGN(KeyCorrector);
};

}  // namespace mozc

#endif  // MOZC_CONVERTER_KEY_CORRECTOR_H_
