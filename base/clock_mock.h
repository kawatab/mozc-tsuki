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

#ifndef MOZC_BASE_CLOCK_MOCK_H_
#define MOZC_BASE_CLOCK_MOCK_H_

#include <ctime>

#include "base/util.h"

namespace mozc {

// Standard mock clock implementation.
// This mock behaves in UTC
class ClockMock : public Util::ClockInterface {
 public:
  ClockMock(uint64 sec, uint32 usec);
  virtual ~ClockMock();

  virtual void GetTimeOfDay(uint64 *sec, uint32 *usec);
  virtual uint64 GetTime();
  virtual bool GetTmWithOffsetSecond(time_t offset_sec, tm *output);
  virtual uint64 GetFrequency();
  virtual uint64 GetTicks();
#ifdef __native_client__
  virtual void SetTimezoneOffset(int32 timezone_offset_sec);
#endif  // __native_client__

  // Puts this clock forward.
  // It has no impact on ticks.
  void PutClockForward(uint64 delta_sec, uint32 delta_usec);

  // Puts this clock forward by ticks
  // It has no impact on seconds and micro seconds.
  void PutClockForwardByTicks(uint64 ticks);

  // Automatically puts this clock forward on every time after it returns time.
  // It has no impact on ticks.
  void SetAutoPutClockForward(uint64 delta_sec, uint32 delta_usec);

  void SetTime(uint64 sec, uint32 usec);
  void SetFrequency(uint64 frequency);
  void SetTicks(uint64 ticks);

 private:
  uint64 seconds_;
  uint32 micro_seconds_;
  uint64 frequency_;
  uint64 ticks_;
#ifdef __native_client__
  int32 timezone_offset_sec_;
#endif  // __native_client__
  // Everytime user requests time clock, following time is added to the
  // internal clock.
  uint64 delta_seconds_;
  uint32 delta_micro_seconds_;

  DISALLOW_COPY_AND_ASSIGN(ClockMock);
};

}  // namespace mozc

#endif  // MOZC_BASE_CLOCK_MOCK_H_
