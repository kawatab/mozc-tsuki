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

#include "storage/louds/bit_vector_based_array.h"

#include <cstddef>
#include <cstdint>

#include "base/bits.h"
#include "base/logging.h"
#include "storage/louds/simple_succinct_bit_vector_index.h"

namespace mozc {
namespace storage {
namespace louds {
namespace {

// Select1 is not used, so cache is unnecessary.
constexpr size_t kLb0CacheSize = 1024;
constexpr size_t kLb1CacheSize = 0;

}  // namespace

void BitVectorBasedArray::Open(const uint8_t *image) {
  const int index_length = LoadUnalignedAdvance<uint32_t>(image);
  const int base_length = LoadUnalignedAdvance<uint32_t>(image);
  const int step_length = LoadUnalignedAdvance<uint32_t>(image);
  // Check 0 padding.
  CHECK_EQ(LoadUnalignedAdvance<uint32_t>(image), 0);

  index_.Init(image, index_length, kLb0CacheSize, kLb1CacheSize);
  base_length_ = base_length;
  step_length_ = step_length;
  data_ = reinterpret_cast<const char *>(image + index_length);
}

void BitVectorBasedArray::Close() {
  index_.Reset();
  base_length_ = 0;
  step_length_ = 0;
  data_ = nullptr;
}

const char *BitVectorBasedArray::Get(size_t index, size_t *length) const {
  DCHECK(length);
  const int bit_index = index_.Select0(index + 1);
  const int data_index =
      base_length_ * index + step_length_ * index_.Rank1(bit_index);
  // Linear search.
  int i = bit_index + 1;
  while (index_.Get(i)) {
    ++i;
  }
  *length = base_length_ + step_length_ * (i - bit_index - 1);
  return data_ + data_index;
}

}  // namespace louds
}  // namespace storage
}  // namespace mozc
