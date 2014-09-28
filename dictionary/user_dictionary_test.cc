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

// Unit tests for UserDictionary.

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/trie.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "data_manager/testing/mock_user_pos_manager.h"
#include "dictionary/dictionary_test_util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_pos.h"
#include "dictionary/user_pos_interface.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

DECLARE_string(test_tmpdir);

using mozc::dictionary::CollectTokenCallback;

namespace mozc {
namespace {

const char kUserDictionary0[] =
    "start\tstart\tverb\n"
    "star\tstar\tnoun\n"
    "starting\tstarting\tnoun\n"
    "stamp\tstamp\tnoun\n"
    "stand\tstand\tverb\n"
    "smile\tsmile\tverb\n"
    "smog\tsmog\tnoun\n"
    // invalid characters "水雲" in reading
    "\xE6\xB0\xB4\xE9\x9B\xB2\tvalue\tnoun\n"

    // Empty key
    "\tvalue\tnoun\n"
    // Empty value
    "start\t\tnoun\n"
    // Invalid POS
    "star\tvalue\tpos\n"
    // Empty POS
    "star\tvalue\t\n"
    // Duplicate entry
    "start\tstart\tverb\n"
    // The following are for tests for LookupComment
    // No comment
    "comment_key1\tcomment_value1\tnoun\n"
    // Has comment
    "comment_key2\tcomment_value2\tnoun\tcomment\n"
    // Different POS
    "comment_key3\tcomment_value3\tnoun\tcomment1\n"
    "comment_key3\tcomment_value3\tverb\tcomment2\n"
    // White spaces comment
    "comment_key4\tcomment_value4\tverb\t     \n";

const char kUserDictionary1[] = "end\tend\tverb\n";

void PushBackToken(const string &key,
                   const string &value,
                   uint16 id,
                   vector<UserPOS::Token> *tokens) {
  tokens->resize(tokens->size() + 1);
  UserPOS::Token *t = &tokens->back();
  t->key = key;
  t->value = value;
  t->id = id;
  t->cost = 0;
}

// This class is a mock class for writing unit tests of a class that
// depends on POS. It accepts only two values for part-of-speech:
// "noun" as words without inflection and "verb" as words with
// inflection.
class UserPOSMock : public UserPOSInterface {
 public:
  UserPOSMock() {}
  virtual ~UserPOSMock() {}

  // This method returns true if the given pos is "noun" or "verb".
  virtual bool IsValidPOS(const string &pos) const {
    return true;
  }

  static const char *kNoun;
  static const char *kVerb;

  // Given a verb, this method expands it to three different forms,
  // i.e. base form (the word itself), "-ed" form and "-ing" form. For
  // example, if the given word is "play", the method returns "play",
  // "played" and "playing". When a noun is passed, it returns only
  // base form. The method set lid and rid of the word as following:
  //
  //  POS              | lid | rid
  // ------------------+-----+-----
  //  noun             | 100 | 100
  //  verb (base form) | 200 | 200
  //  verb (-ed form)  | 210 | 210
  //  verb (-ing form) | 220 | 220
  virtual bool GetTokens(const string &key,
                         const string &value,
                         const string &pos,
                         vector<UserPOS::Token> *tokens) const {
    if (key.empty() ||
        value.empty() ||
        pos.empty() ||
        tokens == NULL) {
      return false;
    }

    tokens->clear();
    if (pos == kNoun) {
      PushBackToken(key, value, 100, tokens);
      return true;
    } else if (pos == kVerb) {
      PushBackToken(key, value, 200, tokens);
      PushBackToken(key + "ed", value + "ed", 210, tokens);
      PushBackToken(key + "ing", value + "ing", 220, tokens);
      return true;
    } else {
      return false;
    }
  }

  virtual void GetPOSList(vector<string> *pos_list) const {}

  virtual bool GetPOSIDs(const string &pos, uint16 *id) const {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(UserPOSMock);
};
// "名詞"
const char *UserPOSMock::kNoun = "\xE5\x90\x8D\xE8\xA9\x9E";
// "動詞ワ行五段"
const char *UserPOSMock::kVerb =
    "\xE5\x8B\x95\xE8\xA9\x9E\xE3\x83\xAF\xE8\xA1\x8C\xE4\xBA\x94\xE6\xAE\xB5";

string GenRandomAlphabet(int size) {
  string result;
  const size_t len = Util::Random(size) + 1;
  for (int i = 0; i < len; ++i) {
    const char32 l = Util::Random(static_cast<int>('z' - 'a')) + 'a';
    Util::UCS4ToUTF8Append(l, &result);
  }
  return result;
}

class UserDictionaryTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    suppression_dictionary_.reset(new SuppressionDictionary);

    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  virtual void TearDown() {
    mozc::usage_stats::UsageStats::ClearAllStatsForTest();
  }

  // Workaround for the constructor of UserDictionary being protected.
  // Creates a user dictionary with mock pos data.
  UserDictionary *CreateDictionaryWithMockPos() {
    const testing::MockUserPosManager user_pos_manager;
    return new UserDictionary(
        new UserPOSMock(),
        user_pos_manager.GetPOSMatcher(),
        suppression_dictionary_.get());
  }

  // Creates a user dictionary with actual pos data.
  UserDictionary *CreateDictionary() {
    const testing::MockUserPosManager user_pos_manager;
    return new UserDictionary(new UserPOS(user_pos_manager.GetUserPOSData()),
                              user_pos_manager.GetPOSMatcher(),
                              Singleton<SuppressionDictionary>::get());
  }

  struct Entry {
    string key;
    string value;
    uint16 lid;
    uint16 rid;
  };

  class EntryCollector : public DictionaryInterface::Callback {
   public:
    virtual ResultType OnToken(StringPiece,  // key
                               StringPiece,  // actual_key
                               const Token &token) {
      // Collect only user dictionary entries.
      if (token.attributes & Token::USER_DICTIONARY) {
        entries_.push_back(Entry());
        entries_.back().key = token.key;
        entries_.back().value = token.value;
        entries_.back().lid = token.lid;
        entries_.back().rid = token.rid;
      }
      return TRAVERSE_CONTINUE;
    }

    const vector<Entry> &entries() const { return entries_; }

   private:
    vector<Entry> entries_;
  };

  static void TestLookupPredictiveHelper(const Entry *expected,
                                         size_t expected_size,
                                         StringPiece key,
                                         const UserDictionary &dic) {
    EntryCollector collector;
    dic.LookupPredictive(key, false, &collector);

    if (expected == NULL || expected_size == 0) {
      EXPECT_TRUE(collector.entries().empty());
    } else {
      ASSERT_FALSE(collector.entries().empty());
      CompareEntries(expected, expected_size, collector.entries());
    }
  }

  static void TestLookupPrefixHelper(const Entry *expected,
                                     size_t expected_size,
                                     const char *key,
                                     size_t key_size,
                                     const UserDictionary &dic) {
    EntryCollector collector;
    dic.LookupPrefix(StringPiece(key, key_size), false, &collector);

    if (expected == NULL || expected_size == 0) {
      EXPECT_TRUE(collector.entries().empty());
    } else {
      ASSERT_FALSE(collector.entries().empty());
      CompareEntries(expected, expected_size, collector.entries());
    }
  }

  static void TestLookupExactHelper(const Entry *expected,
                                    size_t expected_size,
                                    const char *key,
                                    size_t key_size,
                                    const UserDictionary &dic) {
    EntryCollector collector;
    dic.LookupExact(StringPiece(key, key_size), &collector);

    if (expected == NULL || expected_size == 0) {
      EXPECT_TRUE(collector.entries().empty());
    } else {
      ASSERT_FALSE(collector.entries().empty());
      CompareEntries(expected, expected_size, collector.entries());
    }
  }

  static string EncodeEntry(const Entry &entry) {
    return entry.key + "\t" +
           entry.value + "\t" +
           NumberUtil::SimpleItoa(entry.lid) + "\t" +
           NumberUtil::SimpleItoa(entry.rid) + "\n";
  }

  static string EncodeEntries(const Entry *array, size_t size) {
    vector<string> encoded_items;
    for (size_t i = 0; i < size; ++i) {
      encoded_items.push_back(EncodeEntry(array[i]));
    }
    sort(encoded_items.begin(), encoded_items.end());
    string result;
    Util::JoinStrings(encoded_items, "", &result);
    return result;
  }

  static void CompareEntries(const Entry *expected, size_t expected_size,
                             const vector<Entry> &actual) {
    const string expected_encoded = EncodeEntries(expected, expected_size);
    const string actual_encoded = EncodeEntries(&actual[0], actual.size());
    EXPECT_EQ(expected_encoded, actual_encoded);
  }

  static void LoadFromString(const string &contents,
                             UserDictionaryStorage *storage) {
    istringstream is(contents);
    CHECK(is.good());

    storage->Clear();
    UserDictionaryStorage::UserDictionary *dic
        = storage->add_dictionaries();
    CHECK(dic);

    string line;
    while (!getline(is, line).fail()) {
      if (line.empty() || line[0] == '#') {
        continue;
      }
      vector<string> fields;
      Util::SplitStringAllowEmpty(line, "\t", &fields);
      EXPECT_GE(fields.size(), 3) << line;
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      CHECK(entry);
      entry->set_key(fields[0]);
      entry->set_value(fields[1]);
      if (fields[2] == "verb") {
        entry->set_pos(user_dictionary::UserDictionary::WA_GROUP1_VERB);
      } else if (fields[2] == "noun") {
        entry->set_pos(user_dictionary::UserDictionary::NOUN);
      }
      if (fields.size() >= 4 && !fields[3].empty()) {
        entry->set_comment(fields[3]);
      }
    }
  }

  // Helper function to lookup comment string from |dic|.
  static string LookupComment(const UserDictionary& dic,
                              StringPiece key, StringPiece value) {
    string comment;
    dic.LookupComment(key, value, &comment);
    return comment;
  }

  scoped_ptr<SuppressionDictionary> suppression_dictionary_;

 private:
  mozc::usage_stats::scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(UserDictionaryTest, TestLookupPredictive) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
    { "start", "start", 200, 200 },
    { "started", "started", 210, 210 },
    { "starting", "starting", 100, 100 },
    { "starting", "starting", 220, 220 },
  };
  TestLookupPredictiveHelper(kExpected0, arraysize(kExpected0),
                             "start", *dic.get());

  // Another normal lookup operation.
  const Entry kExpected1[] = {
    { "stamp", "stamp", 100, 100 },
    { "stand", "stand", 200, 200 },
    { "standed", "standed", 210, 210 },
    { "standing", "standing", 220, 220 },
    { "star", "star", 100, 100 },
    { "start", "start", 200, 200 },
    { "started", "started", 210, 210 },
    { "starting", "starting", 100, 100 },
    { "starting", "starting", 220, 220 },
  };
  TestLookupPredictiveHelper(kExpected1, arraysize(kExpected1),
                             "st", *dic.get());

  // Invalid input values should be just ignored.
  TestLookupPredictiveHelper(NULL, 0, "", *dic.get());
  TestLookupPredictiveHelper(NULL, 0,
                             "\xE6\xB0\xB4\xE9\x9B\xB2",  // "水雲"
                             *dic.get());

  // Make a change to the dictionary file and load it again.
  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary1, &storage);
    dic->Load(storage);
  }

  // A normal lookup again.
  const Entry kExpected2[] = {
    { "end", "end", 200, 200 },
    { "ended", "ended", 210, 210 },
    { "ending", "ending", 220, 220 },
  };
  TestLookupPredictiveHelper(kExpected2, arraysize(kExpected2),
                             "end", *dic.get());

  // Entries in the dictionary before reloading cannot be looked up.
  TestLookupPredictiveHelper(NULL, 0, "start", *dic.get());
  TestLookupPredictiveHelper(NULL, 0, "st", *dic.get());
}

TEST_F(UserDictionaryTest, TestLookupPrefix) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
    { "star", "star", 100, 100 },
    { "start", "start", 200, 200 },
    { "started", "started", 210, 210 },
  };
  TestLookupPrefixHelper(kExpected0, arraysize(kExpected0),
                         "started", 7, *dic.get());

  // Another normal lookup operation.
  const Entry kExpected1[] = {
    { "star", "star", 100, 100 },
    { "start", "start", 200, 200 },
    { "starting", "starting", 100, 100 },
    { "starting", "starting", 220, 220 },
  };
  TestLookupPrefixHelper(kExpected1, arraysize(kExpected1),
                         "starting", 8, *dic.get());

  // Invalid input values should be just ignored.
  TestLookupPrefixHelper(NULL, 0, "", 0, *dic.get());
  TestLookupPrefixHelper(
      NULL, 0, "\xE6\xB0\xB4\xE9\x9B\xB2",  // "水雲"
      strlen("\xE6\xB0\xB4\xE9\x9B\xB2"), *dic.get());

  // Make a change to the dictionary file and load it again.
  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary1, &storage);
    dic->Load(storage);
  }

  // A normal lookup.
  const Entry kExpected2[] = {
    { "end", "end", 200, 200 },
    { "ending", "ending", 220, 220 },
  };
  TestLookupPrefixHelper(kExpected2, arraysize(kExpected2),
                                     "ending", 6, *dic.get());

  // Lookup for entries which are gone should returns empty result.
  TestLookupPrefixHelper(NULL, 0, "started", 7, *dic.get());
  TestLookupPrefixHelper(NULL, 0, "starting", 8, *dic.get());
}

TEST_F(UserDictionaryTest, TestLookupExact) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  // A normal lookup operation.
  const Entry kExpected0[] = {
    { "start", "start", 200, 200 },
  };
  TestLookupExactHelper(kExpected0, arraysize(kExpected0),
                        "start", 5, *dic.get());

  // Another normal lookup operation.
  const Entry kExpected1[] = {
    { "starting", "starting", 100, 100 },
    { "starting", "starting", 220, 220 },
  };
  TestLookupExactHelper(kExpected1, arraysize(kExpected1),
                        "starting", 8, *dic.get());

  // Invalid input values should be just ignored.
  TestLookupPrefixHelper(NULL, 0, "", 0, *dic.get());
  TestLookupPrefixHelper(NULL, 0, "\xE6\xB0\xB4\xE9\x9B\xB2",  // "水雲"
                         strlen("\xE6\xB0\xB4\xE9\x9B\xB2"), *dic.get());
}

TEST_F(UserDictionaryTest, TestLookupExactWithSuggestionOnlyWords) {
  scoped_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  // Create dictionary
  const string filename = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                             "suggestion_only_test.db");
  FileUtil::Unlink(filename);
  UserDictionaryStorage storage(filename);
  {
    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);

    // "名詞"
    UserDictionaryStorage::UserDictionaryEntry *entry =
        dic->add_entries();
    entry->set_key("key");
    entry->set_value("noun");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);

    // "サジェストのみ"
    entry = dic->add_entries();
    entry->set_key("key");
    entry->set_value("suggest_only");
    entry->set_pos(user_dictionary::UserDictionary::SUGGESTION_ONLY);

    user_dic->Load(storage);
  }

  // "suggestion_only" should not be looked up.
  const testing::MockUserPosManager user_pos_manager;
  const uint16 kNounId = user_pos_manager.GetPOSMatcher()->GetGeneralNounId();
  const Entry kExpected1[] = {{"key", "noun", kNounId, kNounId}};
  TestLookupExactHelper(kExpected1, arraysize(kExpected1),
                        "key", 3, *user_dic.get());
}

TEST_F(UserDictionaryTest, IncognitoModeTest) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  config.set_incognito_mode(true);
  config::ConfigHandler::SetConfig(config);

  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  TestLookupPrefixHelper(NULL, 0, "start", 4, *dic);
  TestLookupPredictiveHelper(NULL, 0, "s", *dic);

  config.set_incognito_mode(false);
  config::ConfigHandler::SetConfig(config);

  {
    EntryCollector collector;
    dic->LookupPrefix("start", false, &collector);
    EXPECT_FALSE(collector.entries().empty());
  }
  {
    EntryCollector collector;
    dic->LookupPredictive("s", false, &collector);
    EXPECT_FALSE(collector.entries().empty());
  }
}

TEST_F(UserDictionaryTest, AsyncLoadTest) {
  const string filename = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                             "async_load_test.db");
  FileUtil::Unlink(filename);

  // Create dictionary
  vector<string> keys;
  {
    UserDictionaryStorage storage(filename);

    EXPECT_FALSE(storage.Load());
    EXPECT_TRUE(storage.Lock());

    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key(GenRandomAlphabet(10));
      entry->set_value(GenRandomAlphabet(10));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
      entry->set_comment(GenRandomAlphabet(10));
      keys.push_back(entry->key());
    }
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  {
    scoped_ptr<UserDictionary> dic(CreateDictionary());
    // Wait for async reload called from the constructor.
    dic->WaitForReloader();
    dic->SetUserDictionaryName(filename);

    for (int i = 0; i < 32; ++i) {
      random_shuffle(keys.begin(), keys.end());
      dic->Reload();
      for (int i = 0; i < 1000; ++i) {
        CollectTokenCallback callback;
        dic->LookupPrefix(keys[i], false, &callback);
      }
    }
    dic->WaitForReloader();
  }
  FileUtil::Unlink(filename);
}

TEST_F(UserDictionaryTest, AddToAutoRegisteredDictionary) {
  const string filename = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                             "add_to_auto_registered.db");
  FileUtil::Unlink(filename);

  // Create dictionary
  {
    UserDictionaryStorage storage(filename);
    EXPECT_FALSE(storage.Load());
    EXPECT_TRUE(storage.Lock());
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  // Add entries.
  {
    scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
    dic->WaitForReloader();
    dic->SetUserDictionaryName(filename);
    for (int i = 0; i < 100; ++i) {
      EXPECT_TRUE(dic->AddToAutoRegisteredDictionary(
                      "key" + NumberUtil::SimpleItoa(i),
                      "value" + NumberUtil::SimpleItoa(i),
                      user_dictionary::UserDictionary::NOUN));
      dic->WaitForReloader();
    }
  }

  // Verify the contents.
  {
    UserDictionaryStorage storage(filename);
    EXPECT_TRUE(storage.Load());
    int index = 0;
    EXPECT_EQ(1, storage.dictionaries_size());
    EXPECT_EQ(100, storage.dictionaries(index).entries_size());
    for (int i = 0; i < 100; ++i) {
      EXPECT_EQ("key" + NumberUtil::SimpleItoa(i),
                storage.dictionaries(index).entries(i).key());
      EXPECT_EQ("value" + NumberUtil::SimpleItoa(i),
                storage.dictionaries(index).entries(i).value());
      EXPECT_EQ(user_dictionary::UserDictionary::NOUN,
                storage.dictionaries(index).entries(i).pos());
    }
  }

  FileUtil::Unlink(filename);

  // Create dictionary
  {
    UserDictionaryStorage storage(filename);
    EXPECT_FALSE(storage.Load());
    EXPECT_TRUE(storage.Lock());
    EXPECT_TRUE(storage.Save());
    EXPECT_TRUE(storage.UnLock());
  }

  // Add same entries.
  {
    scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
    dic->WaitForReloader();
    dic->SetUserDictionaryName(filename);
    EXPECT_TRUE(dic->AddToAutoRegisteredDictionary(
        "key", "value", user_dictionary::UserDictionary::NOUN));
    dic->WaitForReloader();
    // Duplicated one is not registered.
    EXPECT_FALSE(dic->AddToAutoRegisteredDictionary(
        "key", "value", user_dictionary::UserDictionary::NOUN));
    dic->WaitForReloader();
  }

  // Verify the contents.
  {
    UserDictionaryStorage storage(filename);
    EXPECT_TRUE(storage.Load());
    EXPECT_EQ(1, storage.dictionaries_size());
    EXPECT_EQ(1, storage.dictionaries(0).entries_size());
    EXPECT_EQ("key", storage.dictionaries(0).entries(0).key());
    EXPECT_EQ("value", storage.dictionaries(0).entries(0).value());
    EXPECT_EQ(user_dictionary::UserDictionary::NOUN,
              storage.dictionaries(0).entries(0).pos());
  }
}

TEST_F(UserDictionaryTest, TestSuppressionDictionary) {
  scoped_ptr<UserDictionary> user_dic(CreateDictionaryWithMockPos());
  user_dic->WaitForReloader();

  const string filename = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                             "suppression_test.db");
  FileUtil::Unlink(filename);

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("no_suppress_key" +
                     NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      entry->set_value("no_suppress_value" +
                       NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key(
          "suppress_key" + NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      entry->set_value(
          "suppress_value" + NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      // entry->set_pos("抑制単語");
      entry->set_pos(user_dictionary::UserDictionary::SUPPRESSION_WORD);
    }

    suppression_dictionary_->Lock();
    EXPECT_TRUE(suppression_dictionary_->IsLocked());
    user_dic->Load(storage);
    EXPECT_FALSE(suppression_dictionary_->IsLocked());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_TRUE(suppression_dictionary_->SuppressEntry(
          "suppress_key" + NumberUtil::SimpleItoa(static_cast<uint32>(j)),
          "suppress_value" + NumberUtil::SimpleItoa(static_cast<uint32>(j))));
    }
  }

  // Remove suppression entry
  {
    storage.Clear();
    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);
    for (size_t j = 0; j < 10000; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key(
          "no_suppress_key" + NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      entry->set_value(
          "no_suppress_value" + NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    suppression_dictionary_->Lock();
    user_dic->Load(storage);
    EXPECT_FALSE(suppression_dictionary_->IsLocked());

    for (size_t j = 0; j < 10; ++j) {
      EXPECT_FALSE(suppression_dictionary_->SuppressEntry(
          "suppress_key" + NumberUtil::SimpleItoa(static_cast<uint32>(j)),
          "suppress_value" + NumberUtil::SimpleItoa(static_cast<uint32>(j))));
    }
  }
  FileUtil::Unlink(filename);
}

TEST_F(UserDictionaryTest, TestSuggestionOnlyWord) {
  scoped_ptr<UserDictionary> user_dic(CreateDictionary());
  user_dic->WaitForReloader();

  const string filename = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                             "suggestion_only_test.db");
  FileUtil::Unlink(filename);

  UserDictionaryStorage storage(filename);

  // Create dictionary
  {
    uint64 id = 0;
    EXPECT_TRUE(storage.CreateDictionary("test", &id));
    UserDictionaryStorage::UserDictionary *dic =
        storage.mutable_dictionaries(0);

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("key" + NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      entry->set_value("default");
      // "名詞"
      entry->set_pos(user_dictionary::UserDictionary::NOUN);
    }

    for (size_t j = 0; j < 10; ++j) {
      UserDictionaryStorage::UserDictionaryEntry *entry =
          dic->add_entries();
      entry->set_key("key" + NumberUtil::SimpleItoa(static_cast<uint32>(j)));
      entry->set_value("suggest_only");
      // "サジェストのみ"
      entry->set_pos(user_dictionary::UserDictionary::SUGGESTION_ONLY);
    }

    user_dic->Load(storage);
  }

  {
    const char kKey[] = "key0123";
    CollectTokenCallback callback;
    user_dic->LookupPrefix(kKey, false, &callback);
    const vector<Token> &tokens = callback.tokens();
    for (size_t i = 0; i < tokens.size(); ++i) {
      EXPECT_EQ("default", tokens[i].value);
    }
  }
  {
    const char kKey[] = "key";
    CollectTokenCallback callback;
    user_dic->LookupPredictive(kKey, false, &callback);
    const vector<Token> &tokens = callback.tokens();
    for (size_t i = 0; i < tokens.size(); ++i) {
      EXPECT_TRUE(tokens[i].value == "suggest_only" ||
                  tokens[i].value == "default");
    }
  }

  FileUtil::Unlink(filename);
}

TEST_F(UserDictionaryTest, TestUsageStats) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();
  UserDictionaryStorage storage("");

  {
    UserDictionaryStorage::UserDictionary *dic1 = storage.add_dictionaries();
    CHECK(dic1);
    UserDictionaryStorage::UserDictionaryEntry *entry;
    entry = dic1->add_entries();
    CHECK(entry);
    entry->set_key("key1");
    entry->set_value("value1");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    entry = dic1->add_entries();
    CHECK(entry);
    entry->set_key("key2");
    entry->set_value("value2");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
  }
  {
    UserDictionaryStorage::UserDictionary *dic2 = storage.add_dictionaries();
    CHECK(dic2);
    UserDictionaryStorage::UserDictionaryEntry *entry;
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key3");
    entry->set_value("value3");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key4");
    entry->set_value("value4");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
    entry = dic2->add_entries();
    CHECK(entry);
    entry->set_key("key5");
    entry->set_value("value5");
    entry->set_pos(user_dictionary::UserDictionary::NOUN);
  }
  dic->Load(storage);

  EXPECT_INTEGER_STATS("UserRegisteredWord", 5);
}

TEST_F(UserDictionaryTest, LookupComment) {
  scoped_ptr<UserDictionary> dic(CreateDictionaryWithMockPos());
  // Wait for async reload called from the constructor.
  dic->WaitForReloader();

  {
    UserDictionaryStorage storage("");
    UserDictionaryTest::LoadFromString(kUserDictionary0, &storage);
    dic->Load(storage);
  }

  // Entry is in user dictionary but has no comment.
  string comment;
  comment = "prev comment";
  EXPECT_FALSE(dic->LookupComment("comment_key1", "comment_value2", &comment));
  EXPECT_EQ("prev comment", comment);

  // Usual case: single key-value pair with comment.
  EXPECT_TRUE(dic->LookupComment("comment_key2", "comment_value2", &comment));
  EXPECT_EQ("comment", comment);

  // There exist two entries having the same key, value and POS.  Since POS is
  // irrelevant to comment lookup, the first nonempty comment should be found.
  EXPECT_TRUE(dic->LookupComment("comment_key3", "comment_value3", &comment));
  EXPECT_EQ("comment1", comment);

  // White-space only comments should be cleared.
  EXPECT_FALSE(dic->LookupComment("comment_key4", "comment_value4", &comment));
  // The previous comment should remain.
  EXPECT_EQ("comment1", comment);

  // Comment should be found iff key and value match.
  EXPECT_TRUE(LookupComment(*dic, "comment_key", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key1", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key2", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key3", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "comment_key4", "mismatching_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value1").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value2").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value3").empty());
  EXPECT_TRUE(LookupComment(*dic, "mismatching_key", "comment_value4").empty());
}

}  // namespace
}  // namespace mozc
