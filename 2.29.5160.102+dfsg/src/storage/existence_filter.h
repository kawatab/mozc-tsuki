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

#ifndef MOZC_STORAGE_EXISTENCE_FILTER_H_
#define MOZC_STORAGE_EXISTENCE_FILTER_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"

namespace mozc {
namespace storage {
namespace existence_filter_internal {

inline constexpr int kBlockShift = 21;  // 2^21 bits == 256KB block
inline constexpr int kBlockBits = 1 << kBlockShift;
inline constexpr int kBlockMask = kBlockBits - 1;
inline constexpr int kBlockBytes = kBlockBits >> 3;
inline constexpr int kBlockWords = kBlockBits >> 5;

// BlockBitmap is an immutable view, directly referencing data given to the
// constructors.
class BlockBitmap {
 public:
  BlockBitmap() = default;
  BlockBitmap(uint32_t size,
              absl::Span<const uint32_t> buf ABSL_ATTRIBUTE_LIFETIME_BOUND);
  explicit BlockBitmap(std::vector<absl::Span<const uint32_t>> blocks
                           ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : blocks_(std::move(blocks)) {}

  inline bool Get(uint32_t index) const {
    const uint32_t bindex = index >> kBlockShift;
    const uint32_t windex = (index & kBlockMask) >> 5;
    const uint32_t bitpos = index & 31;
    return (blocks_[bindex][windex] >> bitpos) & 1;
  }

 protected:
  // Array of blocks. Each block has kBlockBits region except for last block.
  std::vector<absl::Span<const uint32_t>> blocks_;
};

// BlockBitmapBuilder is a utility class to create a BlockBitmap data.
class BlockBitmapBuilder {
 public:
  explicit BlockBitmapBuilder(uint32_t size);

  void clear();

  inline void Set(uint32_t index) {
    const uint32_t bindex = index >> kBlockShift;
    const uint32_t windex = (index & kBlockMask) >> 5;
    const uint32_t bitpos = index & 31;
    blocks_[bindex][windex] |= (static_cast<uint32_t>(1) << bitpos);
  }

  // Serializes the bitmap to the area indicated by `it`.
  std::string::iterator SerializeTo(std::string::iterator it);
  // Builds a BlockBitmap from the underlying data. It doesn't copy the data, so
  // any changes will be visible to the returned bitmap.
  BlockBitmap Build() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  std::vector<std::vector<uint32_t>> blocks_;
};

}  // namespace existence_filter_internal

// ExistenceFilter parameters.
struct ExistenceFilterParams {
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const ExistenceFilterParams& params) {
    absl::Format(&sink,
                 "size: %d bits, estimated insertions: %d, num_hashes: %d",
                 params.size, params.expected_nelts, params.num_hashes);
  }

  uint32_t size;            // the number of bits in the bit vector
  uint32_t expected_nelts;  // the number of values that will be stored
  int num_hashes;  // the number of hash values to use per insert/lookup.
                   // num_hashes must be less than 8.
};

// For Mozc's LOG().
std::ostream& operator<<(std::ostream& os, const ExistenceFilterParams& params);

// Bloom filter
class ExistenceFilter {
 public:
  ExistenceFilter() = default;

  // Constructs a new ExistenceFilter view from the parameters and the
  // BlockBitmap buffer.
  ExistenceFilter(ExistenceFilterParams params,
                  const absl::Span<const uint32_t> buf
                      ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : params_(std::move(params)), rep_(params_.size, buf) {}
  ExistenceFilter(ExistenceFilterParams params,
                  existence_filter_internal::BlockBitmap rep)
      : params_(std::move(params)), rep_(std::move(rep)) {}

  // Read Existence filter from buf.
  static absl::StatusOr<ExistenceFilter> Read(
      absl::Span<const uint32_t> buf ABSL_ATTRIBUTE_LIFETIME_BOUND);

  // Checks if the given 'hash' was previously inserted int the filter
  // It may return some false positives
  bool Exists(uint64_t hash) const;

 private:
  ExistenceFilterParams params_;
  existence_filter_internal::BlockBitmap rep_;  // points to bitmap
};

// ExistenceFilterBuilder is a utility class to construct ExistenceFilter data.
// Use MinFilterSizeInBytesForErrorRate to determine the size and call the
// CreateOptimal function to create an instance.
class ExistenceFilterBuilder {
 public:
  explicit ExistenceFilterBuilder(ExistenceFilterParams params)
      : params_(std::move(params)), rep_(params_.size) {}

  static ExistenceFilterBuilder CreateOptimal(size_t size_in_bytes,
                                              uint32_t estimated_insertions);

  // Inserts a hash value into the filter
  // We generate 'k' separate internal hash values
  void Insert(uint64_t hash);

  // Writes the existence filter to a buffer and returns it.
  std::string SerializeAsString();

  // Builds an ExistenceFilter directly from the internal buffer.
  ExistenceFilter Build() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  // Returns the minimum required size of the filter in bytes
  // under the given error rate and number of elements
  static size_t MinFilterSizeInBytesForErrorRate(float error_rate,
                                                 size_t num_elements);

 private:
  ExistenceFilterParams params_;
  existence_filter_internal::BlockBitmapBuilder rep_;
};

}  // namespace storage
}  // namespace mozc

#endif  // MOZC_STORAGE_EXISTENCE_FILTER_H_
