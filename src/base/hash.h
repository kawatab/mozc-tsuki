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

#ifndef MOZC_BASE_HASH_H_
#define MOZC_BASE_HASH_H_

#include <type_traits>

#include "base/port.h"
#include "base/string_piece.h"

namespace mozc {

class Hash {
 public:
  // Calculates 64-bit fingerprint.
  static uint64 Fingerprint(StringPiece str);
  static uint64 FingerprintWithSeed(StringPiece str, uint32 seed);

  // Calculates 32-bit fingerprint.
  static uint32 Fingerprint32(StringPiece str);
  static uint32 Fingerprint32WithSeed(StringPiece str, uint32 seed);

  // Calculates 64-bit fingerprint for integral types.
  // Note: This function depends on endian.
  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, uint64>::type
  Fingerprint(T num) {
    return Fingerprint(
        StringPiece(reinterpret_cast<const char*>(&num), sizeof(num)));
  }

  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, uint64>::type
  FingerprintWithSeed(T num, uint32 seed) {
    return FingerprintWithSeed(
        StringPiece(reinterpret_cast<const char*>(&num), sizeof(num)), seed);
  }

  // Calculates 32-bit fingerprint for integral types.
  // Note: This function depends on endian.
  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, uint32>::type
  Fingerprint32(T num) {
    return Fingerprint32(
        StringPiece(reinterpret_cast<const char*>(&num), sizeof(num)));
  }

  template <typename T>
  static typename std::enable_if<std::is_integral<T>::value, uint32>::type
  Fingerprint32WithSeed(T num, uint32 seed) {
    return Fingerprint32WithSeed(
        StringPiece(reinterpret_cast<const char*>(&num), sizeof(num)), seed);
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Hash);
};

}  // namespace mozc

#endif  // MOZC_BASE_HASH_H_
