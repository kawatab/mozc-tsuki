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

#include "base/hash.h"

#include <string>

#include "base/port.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace {

TEST(HashTest, Basic) {
  string s = "";
  EXPECT_EQ(0x0d46d8e3, Hash::Fingerprint32(s));
  EXPECT_EQ(0x1153f4be, Hash::Fingerprint32WithSeed(s, 0xdeadbeef));
  EXPECT_EQ(0x2dcdbae1b24d9501, Hash::Fingerprint(s));
  EXPECT_EQ(0x1153f4beb24d9501, Hash::FingerprintWithSeed(s, 0xdeadbeef));

  s = "google";
  EXPECT_EQ(0x74290877, Hash::Fingerprint32(s));
  EXPECT_EQ(0x1f8cbc0c, Hash::Fingerprint32WithSeed(s, 0xdeadbeef));
  EXPECT_EQ(0x56d4ad5eafa6beed, Hash::Fingerprint(s));
  EXPECT_EQ(0x1f8cbc0cafa6beed, Hash::FingerprintWithSeed(s, 0xdeadbeef));

  s = "Hello, world!  Hello, Tokyo!  Good afternoon!  Ladies and gentlemen.";
  EXPECT_EQ(0xb0f5a2ba, Hash::Fingerprint32(s));
  EXPECT_EQ(0xe3fd2997, Hash::Fingerprint32WithSeed(s, 0xdeadbeef));
  EXPECT_EQ(0x936ccddf9d4f0b39, Hash::Fingerprint(s));
  EXPECT_EQ(0xe3fd29979d4f0b39, Hash::FingerprintWithSeed(s, 0xdeadbeef));
}

TEST(HashTest, Fingerprint32WithSeed_IntegralTypes) {
  const uint32 seed = 0xabcdef;
  {
    const int32 num = 0x12345678;  // Little endian is assumed.
    const char* str = "\x78\x56\x34\x12";

    const uint32 num_hash32 = Hash::Fingerprint32WithSeed(num, seed);
    const uint32 str_hash32 = Hash::Fingerprint32WithSeed(str, seed);
    EXPECT_EQ(num_hash32, str_hash32);

    const uint64 num_hash64 = Hash::FingerprintWithSeed(num, seed);
    const uint64 str_hash64 = Hash::FingerprintWithSeed(str, seed);
    EXPECT_EQ(num_hash64, str_hash64);
  }
  {
    const uint8 num = 0x12;  // Little endian is assumed.
    const char* str = "\x12";

    const uint32 num_hash32 = Hash::Fingerprint32WithSeed(num, seed);
    const uint32 str_hash32 = Hash::Fingerprint32WithSeed(str, seed);
    EXPECT_EQ(num_hash32, str_hash32);

    const uint64 num_hash64 = Hash::FingerprintWithSeed(num, seed);
    const uint64 str_hash64 = Hash::FingerprintWithSeed(str, seed);
    EXPECT_EQ(num_hash64, str_hash64);
  }
  {
    const uint32 num = 0x12345678;  // Little endian is assumed.
    const char* str = "\x78\x56\x34\x12";

    const uint32 num_hash32 = Hash::Fingerprint32WithSeed(num, seed);
    const uint32 str_hash32 = Hash::Fingerprint32WithSeed(str, seed);
    EXPECT_EQ(num_hash32, str_hash32);

    const uint64 num_hash64 = Hash::FingerprintWithSeed(num, seed);
    const uint64 str_hash64 = Hash::FingerprintWithSeed(str, seed);
    EXPECT_EQ(num_hash64, str_hash64);
  }
}

}  // namespace
}  // namespace mozc
