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

#include "base/thread2.h"

#include <atomic>
#include <memory>
#include <utility>

#include "testing/gunit.h"
#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace mozc {
namespace {

class CopyCounter {
 public:
  explicit CopyCounter() : count_(std::make_shared<std::atomic<int>>()) {}

  CopyCounter(const CopyCounter &other) { *this = other; }
  CopyCounter(CopyCounter &&other) = default;

  CopyCounter &operator=(const CopyCounter &other) {
    this->count_ = other.count_;

    count_->fetch_add(1);
    return *this;
  }
  CopyCounter &operator=(CopyCounter &&other) = default;

  std::shared_ptr<std::atomic<int>> get() const { return count_; }

  int count() const { return count_->load(); }

 private:
  std::shared_ptr<std::atomic<int>> count_;
};

TEST(ThreadTest, SpawnsSuccessfully) {
  std::atomic<int> counter = 0;

  Thread2 t1([&counter] {
    for (int i = 1; i <= 100; ++i) {
      counter.fetch_add(i);
    }
  });
  Thread2 t2([&counter](int x) { counter.fetch_add(x); }, 50);
  Thread2 t3([&counter](int x, int y) { counter.fetch_sub(x * y); }, 10, 10);
  t1.Join();
  t2.Join();
  t3.Join();

  EXPECT_EQ(counter.load(), 5000);
}

TEST(ThreadTest, CopiesThingsAtMostOnce) {
  CopyCounter counter1;
  CopyCounter counter2;
  std::shared_ptr<std::atomic<int>> c2 = counter2.get();

  Thread2 t([](CopyCounter, CopyCounter) {}, counter1, std::move(counter2));
  t.Join();

  EXPECT_EQ(counter1.count(), 1);
  EXPECT_EQ(c2->load(), 0);
}

TEST(BackgroundFutureTest, ReturnsComputedValueOnReady) {
  auto future = BackgroundFuture<int>([] {
    absl::SleepFor(absl::Milliseconds(100));
    return 42;
  });

  EXPECT_FALSE(future.Ready());
  future.Wait();
  EXPECT_EQ(future.Get(), 42);
}

TEST(BackgroundFutureTest, GetAlsoWaitsForValue) {
  auto future = BackgroundFuture<int>([] {
    absl::SleepFor(absl::Milliseconds(100));
    return 42;
  });

  EXPECT_FALSE(future.Ready());
  EXPECT_EQ(future.Get(), 42);
}

TEST(BackgroundFutureTest, GetByMoveDoesNotCopy) {
  auto future = BackgroundFuture<CopyCounter>([] {
    absl::SleepFor(absl::Milliseconds(100));
    return CopyCounter();
  });

  EXPECT_EQ(std::move(future).Get().count(), 0);
}

TEST(BackgroundFutureTest, WaitWaitsForCompletion) {
  absl::Notification done;

  auto future = BackgroundFuture<void>([&done] {
    absl::SleepFor(absl::Milliseconds(100));
    done.Notify();
  });

  EXPECT_FALSE(done.HasBeenNotified());
  future.Wait();
  EXPECT_TRUE(done.HasBeenNotified());
}

TEST(BackgroundFutureTest, CopiesThingsAtMostOnce) {
  {
    CopyCounter counter1;
    CopyCounter counter2;
    std::shared_ptr<std::atomic<int>> c2 = counter2.get();

    BackgroundFuture<int>([](CopyCounter, CopyCounter) { return 42; }, counter1,
                          std::move(counter2))
        .Wait();

    EXPECT_EQ(counter1.count(), 1);
    EXPECT_EQ(c2->load(), 0);
  }

  {
    CopyCounter counter1;
    CopyCounter counter2;
    std::shared_ptr<std::atomic<int>> c2 = counter2.get();

    BackgroundFuture<void>([](CopyCounter, CopyCounter) {}, counter1,
                           std::move(counter2))
        .Wait();

    EXPECT_EQ(counter1.count(), 1);
    EXPECT_EQ(c2->load(), 0);
  }
}

}  // namespace
}  // namespace mozc
