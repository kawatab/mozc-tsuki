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

#include "testing/base/public/gunit.h"
#include "unix/ibus/message_translator.h"

namespace mozc {
namespace ibus {

TEST(NullMessageTranslatorTest, BasicTest) {
  NullMessageTranslator translator;

  // NullMessageTranslator always returns the given message.
  EXPECT_EQ("foobar", translator.MaybeTranslate("foobar"));
}

TEST(LocaleBasedMessageTranslatorTest, UnknownLocaleName) {
  // Note: any other locale name is not supported yet.
  LocaleBasedMessageTranslator translator("ja_JP");

  // For unknown key.
  EXPECT_EQ("foobar", translator.MaybeTranslate("foobar"));

  // For known key.
  EXPECT_EQ("Properties", translator.MaybeTranslate("Properties"));
}

TEST(LocaleBasedMessageTranslatorTest, KnownJapaneseLocaleName) {
  {
    LocaleBasedMessageTranslator translator("ja_JP.UTF-8");
    // For unknown key.
    EXPECT_EQ("foobar", translator.MaybeTranslate("foobar"));

    // For known key.
    // "プロパティ"
    EXPECT_EQ("\xE3\x83\x97\xE3\x83\xAD\xE3\x83\x91\xE3\x83\x86\xE3\x82\xA3",
              translator.MaybeTranslate("Properties"));
  }

  {
    LocaleBasedMessageTranslator translator("ja_JP.utf8");
    // For unknown key.
    EXPECT_EQ("foobar", translator.MaybeTranslate("foobar"));

    // For known key.
    // "プロパティ"
    EXPECT_EQ("\xE3\x83\x97\xE3\x83\xAD\xE3\x83\x91\xE3\x83\x86\xE3\x82\xA3",
              translator.MaybeTranslate("Properties"));
  }

  {
    LocaleBasedMessageTranslator translator("ja_JP.uTf-8");
    // For unknown key.
    EXPECT_EQ("foobar", translator.MaybeTranslate("foobar"));

    // For known key.
    // "プロパティ"
    EXPECT_EQ("\xE3\x83\x97\xE3\x83\xAD\xE3\x83\x91\xE3\x83\x86\xE3\x82\xA3",
              translator.MaybeTranslate("Properties"));
  }
}

}  // namespace ibus
}  // namespace mozc
