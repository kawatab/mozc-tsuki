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

// Converts a dictionary file into existence filter header file.
// The header file will be used to suppress "Ateji".
//
// input format:
// <reading>[TAB]<cost>[TAB]<rid>[TAB]<lid>[TAB]<surface form>[...]
// ...
// (cost, rid and lid are ignored.)
//
// example:
// ./gen_collocation_suppression_data_main.cc
// --suppression_data=collocation_suppression.txt
// > embedded_collocation_suppression_data.h

#include <iostream>
#include <vector>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/util.h"
#include "rewriter/gen_existence_data.h"

DEFINE_string(suppression_data, "", "suppression data text");
DEFINE_string(output, "", "output file name (default: stdout)");
DEFINE_double(error_rate, 0.00001, "error rate");
DEFINE_bool(binary_mode, false, "outputs binary file");

namespace mozc {
namespace {

void Convert() {
  const char kSeparator[] = "\t";
  vector<string> entries;

  if (FLAGS_suppression_data.empty()) {
    const string kDummyStr = "__NO_DATA__";
    entries.push_back(kDummyStr + kSeparator + kDummyStr);
  } else {
    InputFileStream ifs(FLAGS_suppression_data.c_str());
    string line;

    while (!getline(ifs, line).fail()) {
      if (line.empty()) {
        continue;
      }
      vector<string> fields;
      Util::SplitStringUsing(line, kSeparator, &fields);
      CHECK_GE(fields.size(), 2);
      entries.push_back(fields[0] + kSeparator + fields[1]);
    }
  }

  ostream *ofs = &cout;
  if (!FLAGS_output.empty()) {
    if (FLAGS_binary_mode) {
      ofs = new OutputFileStream(FLAGS_output.c_str(), ios::out | ios::binary);
    } else {
      ofs = new OutputFileStream(FLAGS_output.c_str());
    }
  }

  if (FLAGS_binary_mode) {
    OutputExistenceBinary(entries, ofs, FLAGS_error_rate);
  } else {
    const string kNameSpace = "CollocationSuppressionData";
    OutputExistenceHeader(entries, kNameSpace, ofs, FLAGS_error_rate);
  }

  if (ofs != &cout) {
    delete ofs;
  }
}
}  // namespace
}  // namespace mozc

int main(int argc, char *argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);

  LOG(INFO) << FLAGS_suppression_data;

  mozc::Convert();

  return 0;
}
