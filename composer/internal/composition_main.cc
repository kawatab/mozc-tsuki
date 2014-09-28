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

#include <iostream>  // NOLINT
#include <sstream>

#include "base/flags.h"
#include "composer/internal/composition.h"
#include "composer/table.h"

DEFINE_string(table, "system://romanji-hiragana.tsv",
              "preedit conversion table file.");


int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::composer::Table table;
  table.LoadFromFile(FLAGS_table.c_str());

  mozc::composer::Composition composition(&table);

  string command;
  string result;
  size_t pos = 0;

  while (getline(cin, command)) {
    char initial = command[0];
    if (initial == '-' || (initial >= '0' && initial <= '9')) {
      stringstream ss;
      int delta;
      ss << command;
      ss >> delta;
      pos += delta;
    } else if (initial == '!') {
      pos = composition.DeleteAt(pos);
    } else {
      pos = composition.InsertAt(pos, command);
    }
    composition.GetString(&result);
    cout << result << " : " << pos << endl;
  }
}
