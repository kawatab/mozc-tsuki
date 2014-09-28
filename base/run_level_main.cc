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

#include "base/run_level.h"

#include <iostream>
#include <string>
#include <vector>

#include "base/flags.h"

DEFINE_bool(server, false, "server mode");
DEFINE_bool(client, false, "client mode");

// This is a simple command line tool
// too check RunLevel class
int main(int argc, char **argv) {
  InitGoogle(argv[0], &argc, &argv, false);

  mozc::RunLevel::RequestType type = mozc::RunLevel::SERVER;

  if (FLAGS_client) {
    type = mozc::RunLevel::CLIENT;
  } else if (FLAGS_server) {
    type = mozc::RunLevel::SERVER;
  }

  const mozc::RunLevel::RunLevelType run_level =
      mozc::RunLevel::GetRunLevel(type);

  switch (run_level) {
    case mozc::RunLevel::NORMAL:
      cout << "NORMAL" << endl;
      break;
    case mozc::RunLevel::RESTRICTED:
      cout << "RESTRICTED" << endl;
      break;
    case mozc::RunLevel::DENY:
      cout << "DENY" << endl;
      break;
    default:
      cout << "UNKNOWN" << endl;
      break;
  }

  return static_cast<int>(run_level);
}
