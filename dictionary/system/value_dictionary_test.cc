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

#include "dictionary/system/value_dictionary.h"

#include <vector>

#include "base/file_util.h"
#include "base/stl_util.h"
#include "base/system_util.h"
#include "base/trie.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/system/system_dictionary_builder.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace dictionary {

class ValueDictionaryTest : public testing::Test {
 protected:
  ValueDictionaryTest() :
      dict_name_(FLAGS_test_tmpdir + "/value_dict_test.dic") {}

  virtual void SetUp() {
    STLDeleteElements(&tokens_);
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    FileUtil::Unlink(dict_name_);
    pos_matcher_ = UserPosManager::GetUserPosManager()->GetPOSMatcher();
  }

  virtual void TearDown() {
    STLDeleteElements(&tokens_);
    FileUtil::Unlink(dict_name_);
  }

  void AddToken(const string &key, const string &value) {
    Token *token = new Token;
    token->key = key;
    token->value = value;
    token->cost = 0;
    token->lid = 0;
    token->rid = 0;
    tokens_.push_back(token);
  }

  void BuildDictionary() {
    dictionary::SystemDictionaryBuilder builder;
    builder.BuildFromTokens(tokens_);
    builder.WriteToFile(dict_name_);
  }

  void InitToken(const string &value, Token *token) const {
    token->key = token->value = value;
    token->cost = 10000;
    token->lid = token->rid = pos_matcher_->GetSuggestOnlyWordId();
    token->attributes = Token::NONE;
  }

  const string dict_name_;
  const POSMatcher *pos_matcher_;

 private:
  vector<Token *> tokens_;
};

TEST_F(ValueDictionaryTest, HasValue) {
  // "うぃー"
  AddToken("\xE3\x81\x86\xE3\x81\x83\xE3\x83\xBC", "we");
  // "うぉー"
  AddToken("\xE3\x81\x86\xE3\x81\x89\xE3\x83\xBC", "war");
  // "わーど"
  AddToken("\xE3\x82\x8F\xE3\x83\xBC\xE3\x81\xA9", "word");
  // "わーるど"
  AddToken("\xE3\x82\x8F\xE3\x83\xBC\xE3\x82\x8B\xE3\x81\xA9", "world");
  BuildDictionary();
  scoped_ptr<ValueDictionary> dictionary(
      ValueDictionary::CreateValueDictionaryFromFile(*pos_matcher_,
                                                     dict_name_));

  // ValueDictionary is supposed to use the same data with SystemDictionary
  // and SystemDictionary::HasValue should return the same result with
  // ValueDictionary::HasValue.  So we can skip the actual logic of HasValue
  // and return just false.
  EXPECT_FALSE(dictionary->HasValue("we"));
  EXPECT_FALSE(dictionary->HasValue("war"));
  EXPECT_FALSE(dictionary->HasValue("word"));
  EXPECT_FALSE(dictionary->HasValue("world"));

  EXPECT_FALSE(dictionary->HasValue("hoge"));
  EXPECT_FALSE(dictionary->HasValue("piyo"));
}

TEST_F(ValueDictionaryTest, LookupPredictive) {
  // "ぐーぐる"
  AddToken("\xE3\x81\x90\xE3\x83\xBC\xE3\x81\x90\xE3\x82\x8B", "google");
  // "うぃー"
  AddToken("\xE3\x81\x86\xE3\x81\x83\xE3\x83\xBC", "we");
  // "うぉー"
  AddToken("\xE3\x81\x86\xE3\x81\x89\xE3\x83\xBC", "war");
  // "わーど"
  AddToken("\xE3\x82\x8F\xE3\x83\xBC\xE3\x81\xA9", "word");
  // "わーるど"
  AddToken("\xE3\x82\x8F\xE3\x83\xBC\xE3\x82\x8B\xE3\x81\xA9", "world");
  BuildDictionary();
  scoped_ptr<ValueDictionary> dictionary(
      ValueDictionary::CreateValueDictionaryFromFile(*pos_matcher_,
                                                     dict_name_));

  // Reading fields are irrelevant to value dictionary.  Prepare actual tokens
  // that are to be looked up.
  Token token_we, token_war, token_word, token_world;
  InitToken("we", &token_we);
  InitToken("war", &token_war);
  InitToken("word", &token_word);
  InitToken("world", &token_world);

  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("", false, &callback);
    EXPECT_TRUE(callback.tokens().empty());
  }
  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("w", false, &callback);
    vector<Token *> expected;
    expected.push_back(&token_we);
    expected.push_back(&token_war);
    expected.push_back(&token_word);
    expected.push_back(&token_world);
    EXPECT_TOKENS_EQ_UNORDERED(expected, callback.tokens());
  }
  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("wo", false, &callback);
    vector<Token *> expected;
    expected.push_back(&token_word);
    expected.push_back(&token_world);
    EXPECT_TOKENS_EQ_UNORDERED(expected, callback.tokens());
  }
  {
    CollectTokenCallback callback;
    dictionary->LookupPredictive("ho", false, &callback);
    EXPECT_TRUE(callback.tokens().empty());
  }
}

TEST_F(ValueDictionaryTest, LookupExact) {
  // "うぃー"
  AddToken("\xE3\x81\x86\xE3\x81\x83\xE3\x83\xBC", "we");
  // "うぉー"
  AddToken("\xE3\x81\x86\xE3\x81\x89\xE3\x83\xBC", "war");
  // "わーど"
  AddToken("\xE3\x82\x8F\xE3\x83\xBC\xE3\x81\xA9", "word");
  BuildDictionary();

  scoped_ptr<ValueDictionary> dictionary(
      ValueDictionary::CreateValueDictionaryFromFile(*pos_matcher_,
                                                     dict_name_));
  CollectTokenCallback callback;
  dictionary->LookupExact("war", &callback);
  ASSERT_EQ(1, callback.tokens().size());
  EXPECT_EQ("war", callback.tokens()[0].value);
}

}  // namespace dictionary
}  // namespace mozc
