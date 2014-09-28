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

#ifndef GOOGLE_CLIENT_IME_MOZC_DATA_MANAGER_CONNECTION_FILE_READER_H_
#define GOOGLE_CLIENT_IME_MOZC_DATA_MANAGER_CONNECTION_FILE_READER_H_

#include <string>

#include "base/file_stream.h"
#include "base/port.h"

namespace mozc {

// Utility class to read connection_single_column.txt.
// Usage:
// for (ConnectionFileReader reader(filename); !reader.done(); reader.Next()) {
//   int rid = reader.rid_of_left_node();
//   ...
// }
class ConnectionFileReader {
 public:
  explicit ConnectionFileReader(const string &filename);
  ~ConnectionFileReader();

  bool done() const { return done_; }
  // Currently the matrix is square.
  size_t left_size() const { return pos_size_; }
  size_t right_size() const { return pos_size_; }
  int32 rid_of_left_node() const { return array_index_ / pos_size_; }
  int32 lid_of_right_node() const { return array_index_ % pos_size_; }
  int32 cost() const { return cost_; }

  void Next();

 private:
  InputFileStream stream_;
  bool done_;
  size_t pos_size_;
  int32 array_index_;
  int32 cost_;

  DISALLOW_COPY_AND_ASSIGN(ConnectionFileReader);
};

}  // namespace mozc

#endif  // GOOGLE_CLIENT_IME_MOZC_DATA_MANAGER_CONNECTION_FILE_READER_H_
