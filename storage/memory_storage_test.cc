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

#include "storage/memory_storage.h"

#include <map>
#include <string>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace storage {
namespace {

void CreateKeyValue(map<string, string> *output, int size) {
  output->clear();
  for (int i = 0; i < size; ++i) {
    char key[64];
    char value[64];
    snprintf(key, sizeof(key), "key%d", i);
    snprintf(value, sizeof(value), "value%d", i);
    output->insert(pair<string, string>(key, value));
  }
}

}  // namespace

TEST(MemoryStorageTest, SimpleTest) {
  static const int kSize[] = {10, 100, 1000};

  for (int i = 0; i < arraysize(kSize); ++i) {
    scoped_ptr<StorageInterface> storage(MemoryStorage::New());

    // Insert
    map<string, string> target;
    CreateKeyValue(&target,  kSize[i]);
    {
      for (map<string, string>::const_iterator it = target.begin();
           it != target.end(); ++it) {
        EXPECT_TRUE(storage->Insert(it->first, it->second));
      }
    }

    // Lookup
    for (map<string, string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      string value;
      EXPECT_TRUE(storage->Lookup(it->first, &value));
      EXPECT_EQ(value, it->second);
    }

    for (map<string, string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      const string key = it->first + ".dummy";
      string value;
      EXPECT_FALSE(storage->Lookup(key, &value));
    }

    // Erase
    int id = 0;
    for (map<string, string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      if (id % 2 == 0) {
        EXPECT_TRUE(storage->Erase(it->first));
        const string key = it->first + ".dummy";
        EXPECT_FALSE(storage->Erase(key));
      }
    }

    for (map<string, string>::const_iterator it = target.begin();
         it != target.end(); ++it) {
      string value;
      const string &key = it->first;
      if (id % 2 == 0) {
        EXPECT_FALSE(storage->Lookup(key, &value));
      } else {
        EXPECT_TRUE(storage->Lookup(key, &value));
      }
    }
  }
}

}  // namespace storage
}  // namespace mozc
