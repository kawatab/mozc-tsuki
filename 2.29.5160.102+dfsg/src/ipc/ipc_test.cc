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

#include "ipc/ipc.h"

#include <cstdint>
#include <string>
#include <vector>

#include "base/thread2.h"
#include "testing/gunit.h"
#include "testing/mozctest.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

#ifdef __APPLE__
#include "ipc/ipc_test_util.h"
#endif  // __APPLE__

namespace mozc {
namespace {

// NOTE(komatsu): The name should not end with "_test", otherwise our
// testing tool rut.py misunderstood that the file named
// kServerAddress is a binary to be tested.
constexpr char kServerAddress[] = "test_echo_server";
#ifdef _WIN32
// On windows, multiple-connections failed.
constexpr int kNumThreads = 1;
#else   // _WIN32
constexpr int kNumThreads = 5;
#endif  // _WIN32
constexpr int kNumRequests = 100;

class EchoServer : public IPCServer {
 public:
  EchoServer(const std::string &path, int32_t num_connections,
             absl::Duration timeout)
      : IPCServer(path, num_connections, timeout) {}
  bool Process(absl::string_view input, std::string *output) override {
    if (input == "kill") {
      output->clear();
      return false;
    }
    output->assign(input.data(), input.size());
    return true;
  }
};

constexpr size_t kBaseBufferSizes[] = {
    16,        256,        1024,       16 * 1024,   32 * 1024,
    64 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024,
};

constexpr int kBufferDiffs[] = {
    0, -1, 1, -31, 31, -63, 63,
};

std::string GenerateInputData(int i) {
  size_t size = kBaseBufferSizes[i % std::size(kBaseBufferSizes)];
  const size_t d = i / std::size(kBaseBufferSizes);
  const int diff = kBufferDiffs[d % std::size(kBufferDiffs)];
  if (diff >= 0 || size >= -diff) {
    size += diff;
  }

  // Fill the result with 'x' then add some entropy to it.
  std::string result(size, 'x');
  for (size_t j = 0; true; ++j) {
    const size_t index = j * 13;
    if (index >= size) {
      break;
    }
    const size_t char_offset = (i * 11 + j * 17) % 89;
    result[index] = ' ' + char_offset;
  }

  return result;
}

class IPCTest : public testing::TestWithTempUserProfile {};

TEST_F(IPCTest, IPCTest) {
  testing::ScopedTempUserProfileDirectory temp_user_profile_dir_;

#ifdef __APPLE__
  TestMachPortManager manager;
#endif  // __APPLE__

  EchoServer con(kServerAddress, 10, absl::Milliseconds(1000));
#ifdef __APPLE__
  con.SetMachPortManager(&manager);
#endif  // __APPLE__
  con.LoopAndReturn();

  std::vector<Thread2> cons;
  for (int i = 0; i < kNumThreads; ++i) {
    cons.push_back(Thread2([
#ifdef __APPLE__
                               &manager
#endif  // __APPLE__
    ] {
      absl::SleepFor(absl::Milliseconds(100));
      for (int i = 0; i < kNumRequests; ++i) {
        const std::string input = GenerateInputData(i);
        IPCClient con(kServerAddress, "");
#ifdef __APPLE__
        con.SetMachPortManager(&manager);
#endif  // __APPLE__
        ASSERT_TRUE(con.Connected());
        std::string output;
        ASSERT_TRUE(con.Call(input, &output, absl::Milliseconds(1000)))
            << "size=" << input.size();
        EXPECT_EQ(output.size(), input.size());
        EXPECT_EQ(output, input);
      }
    }));
  }

  for (Thread2 &con : cons) {
    con.Join();
  }

  IPCClient kill(kServerAddress, "");
  const std::string kill_cmd = "kill";
  std::string output;
#ifdef __APPLE__
  kill.SetMachPortManager(&manager);
#endif  // __APPLE__
  // We don't care the return value of this Call() because the return
  // value for server finish can change based on the platform
  // implementations.
  // TODO(mukai, team): determine the spec of return value for that
  // case and add EXPECT_(TRUE|FALSE) here.
  kill.Call(kill_cmd, &output, absl::Milliseconds(1000));

  con.Wait();
}

}  // namespace
}  // namespace mozc
