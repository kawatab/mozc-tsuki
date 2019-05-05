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

#include "base/clock.h"

#include "base/clock_mock.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

// 2020-12-23 13:24:35 (Wed) UTC
// 123456 [usec]
const uint64 kTestSeconds = 1608729875uLL;
const uint32 kTestMicroSeconds = 123456u;

TEST(ClockTest, TimeTestWithMock) {
  ClockMock clock_mock(kTestSeconds, kTestMicroSeconds);
  Clock::SetClockForUnitTest(&clock_mock);

  // GetTime
  {
    EXPECT_EQ(kTestSeconds, Clock::GetTime());
  }

  // GetTimeOfDay
  {
    uint64 current_sec;
    uint32 current_usec;
    Clock::GetTimeOfDay(&current_sec, &current_usec);
    EXPECT_EQ(kTestSeconds, current_sec);
    EXPECT_EQ(kTestMicroSeconds, current_usec);
  }

  // GetCurrentTm
  // 2020-12-23 13:24:35 (Wed)
  {
    tm current_tm;
    Clock::GetCurrentTm(&current_tm);
    EXPECT_EQ(120, current_tm.tm_year);
    EXPECT_EQ(11,  current_tm.tm_mon);
    EXPECT_EQ(23,  current_tm.tm_mday);
    EXPECT_EQ(13,  current_tm.tm_hour);
    EXPECT_EQ(24,  current_tm.tm_min);
    EXPECT_EQ(35,  current_tm.tm_sec);
    EXPECT_EQ(3,   current_tm.tm_wday);
  }

  // GetTmWithoutOffsetSecond
  // 2024/02/23 23:11:15 (Fri)
  {
    const int offset_seconds = 100000000;
    tm offset_tm;
    Clock::GetTmWithOffsetSecond(&offset_tm, offset_seconds);
    EXPECT_EQ(124, offset_tm.tm_year);
    EXPECT_EQ(1,   offset_tm.tm_mon);
    EXPECT_EQ(23,  offset_tm.tm_mday);
    EXPECT_EQ(23,  offset_tm.tm_hour);
    EXPECT_EQ(11,  offset_tm.tm_min);
    EXPECT_EQ(15,  offset_tm.tm_sec);
    EXPECT_EQ(5,   offset_tm.tm_wday);
  }

  // GetFrequency / GetTicks
  {
    const uint64 kFrequency = 12345;
    const uint64 kTicks = 54321;
    clock_mock.SetFrequency(kFrequency);
    EXPECT_EQ(kFrequency, Clock::GetFrequency());
    clock_mock.SetTicks(kTicks);
    EXPECT_EQ(kTicks, Clock::GetTicks());
  }

  // Restore the default clock
  Clock::SetClockForUnitTest(nullptr);

  // GetFrequency / GetTicks without ClockMock
  {
    EXPECT_NE(0, Clock::GetFrequency());
    EXPECT_NE(0, Clock::GetTicks());
  }
}

TEST(ClockTest, TimeTestWithoutMock) {
  uint64 get_time_of_day_sec, get_time_sec;
  uint32 get_time_of_day_usec;

  Clock::GetTimeOfDay(&get_time_of_day_sec, &get_time_of_day_usec);
  get_time_sec = Clock::GetTime();

  // hmm, unstable test.
  const int margin = 1;
  EXPECT_NEAR(get_time_of_day_sec, get_time_sec, margin)
      << ": This test have possibilities to fail "
      << "when system is busy and slow.";
}

}  // namespace
}  // namespace mozc
