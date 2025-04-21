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

#ifndef MOZC_BASE_BITARRAY_H_
#define MOZC_BASE_BITARRAY_H_

#include <cstdint>
#include <cstring>  // memset
#include <memory>

#include "base/port.h"

namespace mozc {

class BitArray {
 public:
  // Specify the size of bit vector
  explicit BitArray(uint32_t size)
      : array_(new uint32_t[1 + (size >> 5)]), size_(size) {
    memset(reinterpret_cast<char *>(array_.get()), 0, 4 * (1 + (size >> 5)));
  }

  ~BitArray() {}

  // Gets true/false of |index|
  bool get(uint32_t index) const {
    return static_cast<bool>((array_[(index >> 5)] >> (index & 0x0000001F)) &
                             0x00000001);
  }

  // Sets the bit at |index| to true.
  void set(uint32_t index) {
    array_[(index >> 5)] |= 0x00000001 << (index & 0x0000001F);
  }

  // Sets the bit at |index| to false.
  void clear(uint32_t index) {
    array_[(index >> 5)] &= ~(0x00000001 << (index & 0x0000001F));
  }

  // Returns the body of bit vector.
  const char *array() const {
    return reinterpret_cast<const char *>(array_.get());
  }

  // Returns the required buffer size for saving the bit vector.
  size_t array_size() const { return 4 * (1 + (size_ >> 5)); }

  // Returns the number of bit(s).
  size_t size() const { return size_; }

  // Immutable accessor.
  static bool GetValue(const char *array, uint32_t index) {
    const uint32_t *uarray = reinterpret_cast<const uint32_t *>(array);
    return static_cast<bool>((uarray[(index >> 5)] >> (index & 0x0000001F)) &
                             0x00000001);
  }

 private:
  std::unique_ptr<uint32_t[]> array_;
  const size_t size_;

  DISALLOW_COPY_AND_ASSIGN(BitArray);
};

}  // namespace mozc

#endif  // MOZC_BASE_BITARRAY_H_
