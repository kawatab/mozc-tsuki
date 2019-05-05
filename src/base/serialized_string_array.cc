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

#include "base/serialized_string_array.h"

#include <memory>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"

namespace mozc {
namespace {

const uint32 kEmptyArrayData = 0x00000000;

}  // namespace

SerializedStringArray::SerializedStringArray() {
  DCHECK(Util::IsLittleEndian()) << "Little endian is assumed";
  clear();
}

SerializedStringArray::~SerializedStringArray() = default;

bool SerializedStringArray::Init(StringPiece data_aligned_at_4byte_boundary) {
  if (VerifyData(data_aligned_at_4byte_boundary)) {
    data_ = data_aligned_at_4byte_boundary;
    return true;
  }
  clear();
  return false;
}

void SerializedStringArray::Set(StringPiece data_aligned_at_4byte_boundary) {
  DCHECK(VerifyData(data_aligned_at_4byte_boundary));
  data_ = data_aligned_at_4byte_boundary;
}

void SerializedStringArray::clear() {
  data_ = StringPiece(reinterpret_cast<const char *>(&kEmptyArrayData), 4);
}

bool SerializedStringArray::VerifyData(StringPiece data) {
  if (data.size() < 4) {
    LOG(ERROR) << "Array size is missing";
    return false;
  }
  const uint32 *u32_array = reinterpret_cast<const uint32 *>(data.data());
  const uint32 size = u32_array[0];

  const size_t min_required_data_size = 4 + (4 + 4) * size;
  if (data.size() < min_required_data_size) {
    LOG(ERROR) << "Lack of data.  At least " << min_required_data_size
               << " bytes are required";
    return false;
  }

  uint32 prev_str_end = min_required_data_size;
  for (uint32 i = 0; i < size; ++i) {
    const uint32 offset = u32_array[2 * i + 1];
    const uint32 len = u32_array[2 * i + 2];
    if (offset < prev_str_end) {
      LOG(ERROR) << "Invalid offset for string " << i << ": len = " << len
                 << ", offset = " << offset;
      return false;
    }
    if (len >= data.size() || offset > data.size() - len) {
      LOG(ERROR) << "Invalid length for string " << i << ": len = " << len
                 << ", offset = " << offset << ", " << data.size();
      return false;
    }
    if (data[offset + len] != '\0') {
      LOG(ERROR) << "string[" << i << "] is not null-terminated";
      return false;
    }
    prev_str_end = offset + len + 1;
  }

  return true;
}

StringPiece SerializedStringArray::SerializeToBuffer(
    const std::vector<StringPiece> &strs, std::unique_ptr<uint32[]> *buffer) {
  const size_t header_byte_size = 4 * (1 + 2 * strs.size());

  // Calculate the offsets of each string.
  std::unique_ptr<uint32[]> offsets(new uint32[strs.size()]);
  size_t current_offset = header_byte_size;  // The offset for first string.
  for (size_t i = 0; i < strs.size(); ++i) {
    offsets[i] = static_cast<uint32>(current_offset);
    // The next string is written after terminating '\0', so increment one byte
    // in addition to the string byte length.
    current_offset += strs[i].size() + 1;
  }

  // At this point, |current_offset| is the byte length of the whole binary
  // image.  Allocate a necessary buffer as uint32 array.
  buffer->reset(new uint32[(current_offset + 3) / 4]);

  (*buffer)[0] = static_cast<uint32>(strs.size());
  for (size_t i = 0; i < strs.size(); ++i) {
    // Fill offset and length.
    (*buffer)[2 * i + 1] = offsets[i];
    (*buffer)[2 * i + 2] = static_cast<uint32>(strs[i].size());

    // Copy string buffer at the calculated offset.  Guarantee that the buffer
    // is null-terminated.
    char *dest = reinterpret_cast<char *>(buffer->get()) + offsets[i];
    memcpy(dest, strs[i].data(), strs[i].size());
    dest[strs[i].size()] = '\0';
  }

  return StringPiece(reinterpret_cast<const char *>(buffer->get()),
                     current_offset);
}

void SerializedStringArray::SerializeToFile(
    const std::vector<StringPiece> &strs, const string &filepath) {
  std::unique_ptr<uint32[]> buffer;
  const StringPiece data = SerializeToBuffer(strs, &buffer);
  OutputFileStream ofs(filepath.c_str(),
                       std::ios_base::out | std::ios_base::binary);
  CHECK(ofs.write(data.data(), data.size()));
}

}  // namespace mozc
