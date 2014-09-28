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

#include "data_manager/connection_file_reader.h"

#include <string>

#include "base/logging.h"
#include "base/number_util.h"

namespace mozc {

ConnectionFileReader::ConnectionFileReader(const string &filename)
    : stream_(filename.c_str()), done_(false), array_index_(-1), cost_(0) {
  LOG(INFO) << "Loading " << filename;
  string header;
  CHECK(!getline(stream_, header).fail()) << filename << " is empty.";
  pos_size_ = NumberUtil::SimpleAtoi(header);
  Next();
}

ConnectionFileReader::~ConnectionFileReader() {
  stream_.close();
  LOG(INFO) << "Done";
}

void ConnectionFileReader::Next() {
  if (done_) {
    return;
  }
  string line;
  done_ = getline(stream_, line).fail();
  if (done_) {
    return;
  }
  ++array_index_;
  cost_ = NumberUtil::SimpleAtoi(line);
}

}  // namespace mozc
