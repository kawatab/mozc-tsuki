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

#include "dictionary/suppression_dictionary.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/thread.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace dictionary {
namespace {

TEST(SuppressionDictionary, BasicTest) {
  SuppressionDictionary dic;

  // repeat 10 times
  for (int i = 0; i < 10; ++i) {
    // Not locked
    EXPECT_FALSE(dic.AddEntry("test", "test"));

    // IsEmpty() returns always true when dic is locked
    {
      const SuppressionDictionaryLock l(&dic);
      EXPECT_TRUE(dic.IsEmpty());
      EXPECT_FALSE(dic.AddEntry("", ""));
      EXPECT_TRUE(dic.AddEntry("key1", "value1"));
      EXPECT_TRUE(dic.AddEntry("key2", "value2"));
      EXPECT_TRUE(dic.AddEntry("key3", "value3"));
      EXPECT_TRUE(dic.AddEntry("key4", ""));
      EXPECT_TRUE(dic.AddEntry("key5", ""));
      EXPECT_TRUE(dic.AddEntry("", "value4"));
      EXPECT_TRUE(dic.AddEntry("", "value5"));
      EXPECT_TRUE(dic.IsEmpty());
    }

    EXPECT_FALSE(dic.IsEmpty());

    // Not locked
    EXPECT_FALSE(dic.AddEntry("test", "test"));

    // locked now => SuppressEntry always returns false
    {
      const SuppressionDictionaryLock l(&dic);
      EXPECT_FALSE(dic.SuppressEntry("key1", "value1"));
    }

    EXPECT_TRUE(dic.SuppressEntry("key1", "value1"));
    EXPECT_TRUE(dic.SuppressEntry("key2", "value2"));
    EXPECT_TRUE(dic.SuppressEntry("key3", "value3"));
    EXPECT_TRUE(dic.SuppressEntry("key4", ""));
    EXPECT_TRUE(dic.SuppressEntry("key5", ""));
    EXPECT_TRUE(dic.SuppressEntry("", "value4"));
    EXPECT_TRUE(dic.SuppressEntry("", "value5"));
    EXPECT_FALSE(dic.SuppressEntry("key1", ""));
    EXPECT_FALSE(dic.SuppressEntry("key2", ""));
    EXPECT_FALSE(dic.SuppressEntry("key3", ""));
    EXPECT_FALSE(dic.SuppressEntry("", "value1"));
    EXPECT_FALSE(dic.SuppressEntry("", "value2"));
    EXPECT_FALSE(dic.SuppressEntry("", "value3"));
    EXPECT_FALSE(dic.SuppressEntry("key1", "value2"));
    EXPECT_TRUE(dic.SuppressEntry("key4", "value2"));
    EXPECT_TRUE(dic.SuppressEntry("key4", "value3"));
    EXPECT_TRUE(dic.SuppressEntry("key5", "value0"));
    EXPECT_TRUE(dic.SuppressEntry("key5", "value4"));
    EXPECT_TRUE(dic.SuppressEntry("key0", "value5"));
    EXPECT_FALSE(dic.SuppressEntry("", ""));

    {
      const SuppressionDictionaryLock l(&dic);
      dic.Clear();
    }
  }
}

class DictionaryLoaderThread : public Thread {
 public:
  explicit DictionaryLoaderThread(SuppressionDictionary *dic) : dic_{dic} {}

  void Run() override {
    const SuppressionDictionaryLock l(dic_);
    dic_->Clear();
    for (int i = 0; i < 100; ++i) {
      const std::string key = "key" + std::to_string(i);
      const std::string value = "value" + std::to_string(i);
      EXPECT_TRUE(dic_->AddEntry(key, value));
#ifdef OS_IOS
      // Sleep only for 1 msec. On iOS, Util::Sleep takes a very longer time
      // like 30 times compaired with MacOS. This should be a temporary
      // solution.
      Util::Sleep(1);
#else
      Util::Sleep(5);
#endif
    }
  }

 private:
  SuppressionDictionary *dic_;
};

TEST(SuppressionDictionary, ThreadTest) {
  // Keys and values for testing.
  std::vector<std::string> keys, values;
  for (int i = 0; i < 100; ++i) {
    keys.push_back("key" + std::to_string(i));
    values.push_back("value" + std::to_string(i));
  }

  SuppressionDictionary dic;
  for (int iter = 0; iter < 3; ++iter) {
    DictionaryLoaderThread thread(&dic);

    // Load dictionary in another thread. `dic` will be locked.
    thread.Start("SuppressionDictionaryTest");

    for (int i = 0; i < 100; ++i) {
      if (!thread.IsRunning()) {
        EXPECT_TRUE(dic.SuppressEntry(keys[i], values[i]));
      }
    }

    thread.Join();

    for (int i = 0; i < 100; ++i) {
      EXPECT_TRUE(dic.SuppressEntry(keys[i], values[i]));
    }
  }
}

}  // namespace
}  // namespace dictionary
}  // namespace mozc
