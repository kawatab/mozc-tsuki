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

#include "dictionary/suffix_dictionary.h"

#include "base/scoped_ptr.h"
#include "base/util.h"
#include "data_manager/testing/mock_data_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/suffix_dictionary_token.h"
#include "testing/base/public/gunit.h"

using ::mozc::dictionary::CollectTokenCallback;

namespace mozc {

TEST(SuffixDictionaryTest, LookupPredictive) {
  // Test SuffixDictionary with mock data.
  scoped_ptr<const SuffixDictionary> dic;
  {
    const testing::MockDataManager manager;
    const SuffixToken *tokens = NULL;
    size_t tokens_size = 0;
    manager.GetSuffixDictionaryData(&tokens, &tokens_size);
    dic.reset(new SuffixDictionary(tokens, tokens_size));
    ASSERT_NE(nullptr, dic.get());
  }

  {
    // Lookup with empty key.  All tokens are looked up.  Here, just verify the
    // result is nonempty and each token has valid data.
    CollectTokenCallback callback;
    dic->LookupPredictive("", false, &callback);
    EXPECT_FALSE(callback.tokens().empty());
    for (size_t i = 0; i < callback.tokens().size(); ++i) {
      const Token &token = callback.tokens()[i];
      EXPECT_FALSE(token.key.empty());
      EXPECT_FALSE(token.value.empty());
      EXPECT_LT(0, token.lid);
      EXPECT_LT(0, token.rid);
      EXPECT_EQ(Token::NONE, token.attributes);
    }
  }
  {
    // Non-empty prefix.
    const string kPrefix = "\xE3\x81\x9F";  // "た"
    CollectTokenCallback callback;
    dic->LookupPredictive(kPrefix, false, &callback);
    EXPECT_FALSE(callback.tokens().empty());
    for (size_t i = 0; i < callback.tokens().size(); ++i) {
      const Token &token = callback.tokens()[i];
      EXPECT_TRUE(Util::StartsWith(token.key, kPrefix));
      EXPECT_FALSE(token.value.empty());
      EXPECT_LT(0, token.lid);
      EXPECT_LT(0, token.rid);
      EXPECT_EQ(Token::NONE, token.attributes);
    }
  }
}

}  // namespace mozc
