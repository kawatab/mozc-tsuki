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

#include "storage/louds/bit_stream.h"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>

#include "base/logging.h"
#include "absl/base/casts.h"

namespace mozc {
namespace storage {
namespace louds {

void BitStream::PushBit(int bit) {
  DCHECK(bit == 0 || bit == 1);

  const size_t shift = num_bits_ % 8;
  if (shift == 0) {
    image_.push_back('\0');
  }
  *image_.rbegin() |= (bit & 1) << shift;
  ++num_bits_;
}

void BitStream::FillPadding32() {
  const size_t remaining = image_.length() % 4;
  if (remaining != 0) {
    image_.append(4 - remaining, '\0');
  }
  num_bits_ = image_.length() * 8;
}

namespace internal {

void PushInt32(size_t value, std::string &image) {
  // Make sure the value is fit in the 32-bit value.
  CHECK_LE(value, std::numeric_limits<uint32_t>::max());

  // Output LSB to MSB.
  image.push_back(absl::implicit_cast<char>(value & 0xFF));
  image.push_back(absl::implicit_cast<char>((value >> 8) & 0xFF));
  image.push_back(absl::implicit_cast<char>((value >> 16) & 0xFF));
  image.push_back(absl::implicit_cast<char>((value >> 24) & 0xFF));
}

}  // namespace internal

}  // namespace louds
}  // namespace storage
}  // namespace mozc
