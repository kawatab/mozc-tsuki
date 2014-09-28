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

#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/text_dictionary_loader.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

const char kTextLines[] =
"key_test1\t0\t0\t1\tvalue_test1\n"
"foo\t1\t2\t3\tbar\n"
"buz\t10\t20\t30\tfoobar\n";

const char kReadingCorrectionLines[] =
    "bar\tfoo\tfoo_correct\n"
    "foobar\tfoobar_error\tfoobar_correct\n";
}  // namespace

class TextDictionaryLoaderTest : public ::testing::Test {
 protected:
  // Explicitly define constructor to prevent Visual C++ from
  // considering this class as POD.
  TextDictionaryLoaderTest() {}

  virtual void SetUp() {
    pos_matcher_ = UserPosManager::GetUserPosManager()->GetPOSMatcher();
  }

  TextDictionaryLoader *CreateTextDictionaryLoader() {
    return new TextDictionaryLoader(*pos_matcher_);
  }

  const POSMatcher *pos_matcher_;
};

TEST_F(TextDictionaryLoaderTest, BasicTest) {
  {
    scoped_ptr<TextDictionaryLoader> loader(CreateTextDictionaryLoader());
    vector<Token *> tokens;
    loader->CollectTokens(&tokens);
    EXPECT_TRUE(tokens.empty());
  }

  const string filename = FileUtil::JoinPath(FLAGS_test_tmpdir, "test.tsv");
  {
    OutputFileStream ofs(filename.c_str());
    ofs << kTextLines;
  }

  {
    scoped_ptr<TextDictionaryLoader> loader(CreateTextDictionaryLoader());
    loader->Load(filename, "");
    const vector<Token *> &tokens = loader->tokens();

    EXPECT_EQ(3, tokens.size());

    EXPECT_EQ("key_test1", tokens[0]->key);
    EXPECT_EQ("value_test1", tokens[0]->value);
    EXPECT_EQ(0, tokens[0]->lid);
    EXPECT_EQ(0, tokens[0]->rid);
    EXPECT_EQ(1, tokens[0]->cost);

    EXPECT_EQ("foo", tokens[1]->key);
    EXPECT_EQ("bar", tokens[1]->value);
    EXPECT_EQ(1, tokens[1]->lid);
    EXPECT_EQ(2, tokens[1]->rid);
    EXPECT_EQ(3, tokens[1]->cost);

    EXPECT_EQ("buz", tokens[2]->key);
    EXPECT_EQ("foobar", tokens[2]->value);
    EXPECT_EQ(10, tokens[2]->lid);
    EXPECT_EQ(20, tokens[2]->rid);
    EXPECT_EQ(30, tokens[2]->cost);

    loader->Clear();
    EXPECT_TRUE(loader->tokens().empty());
  }

  {
    scoped_ptr<TextDictionaryLoader> loader(CreateTextDictionaryLoader());
    loader->LoadWithLineLimit(filename, "", 2);
    const vector<Token *> &tokens = loader->tokens();

    EXPECT_EQ(2, tokens.size());

    EXPECT_EQ("key_test1", tokens[0]->key);
    EXPECT_EQ("value_test1", tokens[0]->value);
    EXPECT_EQ(0, tokens[0]->lid);
    EXPECT_EQ(0, tokens[0]->rid);
    EXPECT_EQ(1, tokens[0]->cost);

    EXPECT_EQ("foo", tokens[1]->key);
    EXPECT_EQ("bar", tokens[1]->value);
    EXPECT_EQ(1, tokens[1]->lid);
    EXPECT_EQ(2, tokens[1]->rid);
    EXPECT_EQ(3, tokens[1]->cost);

    loader->Clear();
    EXPECT_TRUE(loader->tokens().empty());
  }

  {
    scoped_ptr<TextDictionaryLoader> loader(CreateTextDictionaryLoader());
    // open twice -- tokens are cleared everytime
    loader->Load(filename, "");
    loader->Load(filename, "");
    const vector<Token *> &tokens = loader->tokens();
    EXPECT_EQ(3, tokens.size());
  }

  FileUtil::Unlink(filename);
}

TEST_F(TextDictionaryLoaderTest, RewriteSpecialTokenTest) {
  scoped_ptr<TextDictionaryLoader> loader(CreateTextDictionaryLoader());
  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader->RewriteSpecialToken(&token, ""));
    EXPECT_EQ(100, token.lid);
    EXPECT_EQ(200, token.rid);
    EXPECT_EQ(Token::NONE, token.attributes);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader->RewriteSpecialToken(&token, "SPELLING_CORRECTION"));
    EXPECT_EQ(100, token.lid);
    EXPECT_EQ(200, token.rid);
    EXPECT_EQ(Token::SPELLING_CORRECTION, token.attributes);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader->RewriteSpecialToken(&token, "ZIP_CODE"));
    EXPECT_EQ(pos_matcher_->GetZipcodeId(), token.lid);
    EXPECT_EQ(pos_matcher_->GetZipcodeId(), token.rid);
    EXPECT_EQ(Token::NONE, token.attributes);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_TRUE(loader->RewriteSpecialToken(&token, "ENGLISH:RATED"));
    EXPECT_EQ(pos_matcher_->GetIsolatedWordId(), token.lid);
    EXPECT_EQ(pos_matcher_->GetIsolatedWordId(), token.rid);
    EXPECT_EQ(Token::NONE, token.attributes);
  }

  {
    Token token;
    token.lid = 100;
    token.rid = 200;
    EXPECT_FALSE(loader->RewriteSpecialToken(&token, "foo"));
    EXPECT_EQ(100, token.lid);
    EXPECT_EQ(200, token.rid);
    EXPECT_EQ(Token::NONE, token.attributes);
  }
}

TEST_F(TextDictionaryLoaderTest, LoadMultipleFilesTest) {
  const string filename1 = FileUtil::JoinPath(FLAGS_test_tmpdir, "test1.tsv");
  const string filename2 = FileUtil::JoinPath(FLAGS_test_tmpdir, "test2.tsv");
  const string filename = filename1 + "," + filename2;

  {
    OutputFileStream ofs(filename1.c_str());
    ofs << kTextLines;
  }
  {
    OutputFileStream ofs(filename2.c_str());
    ofs << kTextLines;
  }

  {
    scoped_ptr<TextDictionaryLoader> loader(CreateTextDictionaryLoader());
    loader->Load(filename, "");
    EXPECT_EQ(6, loader->tokens().size());
  }

  FileUtil::Unlink(filename1);
  FileUtil::Unlink(filename2);
}

TEST_F(TextDictionaryLoaderTest, ReadingCorrectionTest) {
  scoped_ptr<TextDictionaryLoader> loader(CreateTextDictionaryLoader());

  const string dic_filename =
      FileUtil::JoinPath(FLAGS_test_tmpdir, "test.tsv");
  const string reading_correction_filename =
      FileUtil::JoinPath(FLAGS_test_tmpdir, "reading_correction.tsv");

  {
    OutputFileStream ofs(dic_filename.c_str());
    ofs << kTextLines;
  }
  {
    OutputFileStream ofs(reading_correction_filename.c_str());
    ofs << kReadingCorrectionLines;
  }

  loader->Load(dic_filename, reading_correction_filename);
  const vector<Token *> &tokens = loader->tokens();
  ASSERT_EQ(tokens.size(), 4);
  EXPECT_EQ("foobar_error", tokens[3]->key);
  EXPECT_EQ("foobar", tokens[3]->value);
  EXPECT_EQ(10, tokens[3]->lid);
  EXPECT_EQ(20, tokens[3]->rid);
  EXPECT_EQ(30 + 2302, tokens[3]->cost);
}

}  // namespace mozc
