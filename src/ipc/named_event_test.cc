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

#include "ipc/named_event.h"

#include <atomic>
#include <memory>
#include <string>

#include "base/clock.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

const char kName[] = "named_event_test";

class NamedEventListenerThread : public Thread {
 public:
  NamedEventListenerThread(const string &name,
                           uint32 initial_wait_msec,
                           uint32 wait_msec,
                           size_t max_num_wait)
      : listener_(name.c_str()),
        initial_wait_msec_(initial_wait_msec),
        wait_msec_(wait_msec),
        max_num_wait_(max_num_wait),
        first_triggered_ticks_(0) {
    EXPECT_TRUE(listener_.IsAvailable());
  }

  void Run() override {
    Util::Sleep(initial_wait_msec_);
    for (size_t i = 0; i < max_num_wait_; ++i) {
      const bool result = listener_.Wait(wait_msec_);
      const uint64 ticks = Clock::GetTicks();
      if (result) {
        first_triggered_ticks_ = ticks;
        return;
      }
    }
  }

  uint64 first_triggered_ticks() const {
    return first_triggered_ticks_;
  }

  bool IsTriggered() const {
    return first_triggered_ticks() > 0;
  }

 private:
  NamedEventListener listener_;
  const uint32 initial_wait_msec_;
  const uint32 wait_msec_;
  const size_t max_num_wait_;
  std::atomic<uint64> first_triggered_ticks_;
};

class NamedEventTest : public testing::Test {
  void SetUp() override {
    original_user_profile_directory_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }

  void TearDown() override {
    SystemUtil::SetUserProfileDirectory(original_user_profile_directory_);
  }

 private:
  string original_user_profile_directory_;
};

TEST_F(NamedEventTest, NamedEventBasicTest) {
  NamedEventListenerThread listner(kName, 0, 50, 100);
  listner.Start("NamedEventBasicTest");
  Util::Sleep(200);
  NamedEventNotifier notifier(kName);
  ASSERT_TRUE(notifier.IsAvailable());
  const uint64 notify_ticks = Clock::GetTicks();
  notifier.Notify();
  listner.Join();

  // There is a chance that |listner| is not triggered.
  if (listner.IsTriggered()) {
    EXPECT_LT(notify_ticks, listner.first_triggered_ticks());
  }
}

TEST_F(NamedEventTest, IsAvailableTest) {
  {
    NamedEventListener l(kName);
    EXPECT_TRUE(l.IsAvailable());
    NamedEventNotifier n(kName);
    EXPECT_TRUE(n.IsAvailable());
  }

  // no listner
  {
    NamedEventNotifier n(kName);
    EXPECT_FALSE(n.IsAvailable());
  }
}

TEST_F(NamedEventTest, IsOwnerTest) {
  NamedEventListener l1(kName);
  EXPECT_TRUE(l1.IsOwner());
  EXPECT_TRUE(l1.IsAvailable());
  NamedEventListener l2(kName);
  EXPECT_FALSE(l2.IsOwner());   // the instance is owneded by l1
  EXPECT_TRUE(l2.IsAvailable());
}

TEST_F(NamedEventTest, NamedEventMultipleListenerTest) {
  const size_t kNumRequests = 4;

  // mozc::Thread is not designed as value-semantics.
  // So here we use pointers to maintain these instances.
  std::vector<std::unique_ptr<NamedEventListenerThread>> listeners(
      kNumRequests);
  for (size_t i = 0; i < kNumRequests; ++i) {
    listeners[i].reset(new NamedEventListenerThread(kName, 33 * i, 50, 100));
    listeners[i]->Start("NamedEventMultipleListenerTest");
  }

  Util::Sleep(200);

  // all |kNumRequests| listener events should be raised
  // at once with single notifier event
  NamedEventNotifier notifier(kName);
  ASSERT_TRUE(notifier.IsAvailable());
  const uint64 notify_ticks = Clock::GetTicks();
  ASSERT_TRUE(notifier.Notify());

  for (size_t i = 0; i < kNumRequests; ++i) {
    listeners[i]->Join();
  }

  for (size_t i = 0; i < kNumRequests; ++i) {
    // There is a chance that |listeners[i]| is not triggered.
    if (listeners[i]->IsTriggered()) {
      EXPECT_LT(notify_ticks, listeners[i]->first_triggered_ticks());
    }
  }
}

TEST_F(NamedEventTest, NamedEventPathLengthTest) {
#ifndef OS_WIN
  const string name_path = NamedEventUtil::GetEventPath(kName);
  // length should be less than 14 not includeing terminating null.
  EXPECT_EQ(13, strlen(name_path.c_str()));
#endif  // OS_WIN
}

}  // namespace
}  // namespace mozc
