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

// Generates:
// |kCompressedLSize|, |kCompressedRSize|,
// |kCompressedLIDTable|, |kCompressedRIDTable|,
// |kSegmenterBitArrayData_size|, |kSegmenterBitArrayData_data|

#include "converter/gen_segmenter_bitarray.h"

#include <map>
#include <string>
#include <vector>

#include "base/bitarray.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/util.h"
#include "protocol/segmenter_data.pb.h"

namespace mozc {

namespace {
class StateTable {
 public:
  explicit StateTable(const size_t size) : compressed_size_(0) {
    idarray_.resize(size);
  }

  // |str| is an 1-dimentional row (or column) represented in byte array.
  void Add(uint16 id, const string &str) {
    CHECK_LT(id, idarray_.size());
    idarray_[id] = str;
  }

  void Build() {
    compressed_table_.resize(idarray_.size());
    uint16 id = 0;
    std::map<string, uint16> dup;
    for (size_t i = 0; i < idarray_.size(); ++i) {
      std::map<string, uint16>::const_iterator it = dup.find(idarray_[i]);
      if (it != dup.end()) {
        compressed_table_[i] = it->second;
      } else {
        compressed_table_[i] = id;
        dup.insert(std::make_pair(idarray_[i], id));
        ++id;
      }
    }

    compressed_size_ = dup.size();

    // verify
    for (size_t i = 0; i < idarray_.size(); ++i) {
      CHECK_LT(compressed_table_[i], compressed_size_);
      CHECK_EQ(dup[idarray_[i]], compressed_table_[i]);
    }

    CHECK_LT(compressed_size_, idarray_.size());
  }

  uint16 id(uint16 id) const {
    CHECK_LT(id, idarray_.size());
    return compressed_table_[id];
  }

  size_t compressed_size() const { return compressed_size_; }

  void Output(std::ostream *os) {
    const char* data = reinterpret_cast<const char*>(compressed_table_.data());
    const size_t bytelen = compressed_table_.size() * sizeof(uint16);
    os->write(data, bytelen);
  }

 private:
  std::vector<string> idarray_;
  std::vector<uint16> compressed_table_;
  size_t compressed_size_;

  DISALLOW_COPY_AND_ASSIGN(StateTable);
};
}  // namespace

void SegmenterBitarrayGenerator::GenerateBitarray(
    int lsize, int rsize, IsBoundaryFunc func, const string &output_size_info,
    const string &output_ltable, const string &output_rtable,
    const string &output_bitarray) {
  // Load the original matrix into an array
  std::vector<uint8> array((lsize + 1) * (rsize + 1));

  for (size_t rid = 0; rid <= lsize; ++rid) {
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const uint32 index = rid + lsize * lid;
      CHECK_LT(index, array.size());
      if (rid == lsize || lid == rsize) {
        array[index] = 1;
        continue;
      }
      if ((*func)(rid, lid)) {
        array[index] = 1;
      } else {
        array[index] = 0;
      }
    }
  }

  // Reduce left states (remove dupliacate rows)
  StateTable ltable(lsize + 1);
  for (size_t rid = 0; rid <= lsize; ++rid) {
    string buf;
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const uint32 index = rid + lsize * lid;
      buf += array[index];
    }
    ltable.Add(rid, buf);
  }

  // Reduce right states (remove dupliacate columns)
  StateTable rtable(rsize + 1);
  for (size_t lid = 0; lid <= rsize; ++lid) {
    string buf;
    for (size_t rid = 0; rid <= lsize; ++rid) {
      const uint32 index = rid + lsize * lid;
      buf += array[index];
    }
    rtable.Add(lid, buf);
  }

  // make lookup table
  rtable.Build();
  ltable.Build();

  const size_t kCompressedLSize = ltable.compressed_size();
  const size_t kCompressedRSize = rtable.compressed_size();

  CHECK_GT(kCompressedLSize, 0);
  CHECK_GT(kCompressedRSize, 0);

  // make bitarray
  mozc::BitArray barray(kCompressedLSize * kCompressedRSize);
  for (size_t rid = 0; rid <= lsize; ++rid) {
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const int index = rid + lsize * lid;
      const uint32 cindex = ltable.id(rid) + kCompressedLSize * rtable.id(lid);
      if (array[index] > 0) {
        barray.set(cindex);
      } else {
        barray.clear(cindex);
      }
    }
  }

  // verify the table
  for (size_t rid = 0; rid <= lsize; ++rid) {
    for (size_t lid = 0; lid <= rsize; ++lid) {
      const int index = rid + lsize * lid;
      const uint32 cindex = ltable.id(rid) + kCompressedLSize * rtable.id(lid);
      CHECK_EQ(barray.get(cindex), (array[index] != 0));
    }
  }

  CHECK(barray.array());
  CHECK_GT(barray.size(), 0);

  CHECK(Util::IsLittleEndian())
      << "Architecture must be little endian";
  {
    mozc::converter::SegmenterDataSizeInfo pb;
    pb.set_compressed_lsize(kCompressedLSize);
    pb.set_compressed_rsize(kCompressedRSize);
    mozc::OutputFileStream ofs(output_size_info.c_str(),
                               std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    CHECK(pb.SerializeToOstream(&ofs));
    ofs.close();
  }
  {
    mozc::OutputFileStream ofs(output_ltable.c_str(),
                               std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    ltable.Output(&ofs);
    ofs.close();
  }
  {
    mozc::OutputFileStream ofs(output_rtable.c_str(),
                               std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    rtable.Output(&ofs);
    ofs.close();
  }
  {
    mozc::OutputFileStream ofs(output_bitarray.c_str(),
                               std::ios_base::out | std::ios_base::binary);
    CHECK(ofs);
    ofs.write(barray.array(), barray.array_size());
    ofs.close();
  }
}

}  // namespace mozc
