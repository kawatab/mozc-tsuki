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

#include <iostream>
#include <memory>
#include <string>

#include "base/cpu_stats.h"
#include "base/flags.h"
#include "base/init_mozc.h"
#include "base/port.h"
#include "base/thread.h"
#include "base/util.h"

DEFINE_int32(iterations, 1000, "number of iterations");
DEFINE_int32(polling_duration, 1000, "duration period in msec");
DEFINE_int32(dummy_threads_size, 0, "number of dummy threads");

namespace {
class DummyThread : public mozc::Thread {
 public:
  DummyThread() {}
  void Run() {
    volatile uint64 n = 0;
    while (true) {
      ++n;
      --n;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyThread);
};
}  // namespace

int main(int argc, char **argv) {
  mozc::InitMozc(argv[0], &argc, &argv, false);

  std::unique_ptr<DummyThread[]> threads;

  if (FLAGS_dummy_threads_size > 0) {
    threads.reset(new DummyThread[FLAGS_dummy_threads_size]);
    for (int i = 0; i < FLAGS_dummy_threads_size; ++i) {
      threads[i].Start("CpuStatsMain");
    }
  }

  mozc::CPUStats stats;
  std::cout << "NumberOfProcessors: " << stats.GetNumberOfProcessors()
            << std::endl;
  for (int i = 0; i < FLAGS_iterations; ++i) {
    std::cout << "CPUStats: " << stats.GetSystemCPULoad() << " "
              << stats.GetCurrentProcessCPULoad() << std::endl;
    mozc::Util::Sleep(FLAGS_polling_duration);
  }

  return 0;
}
