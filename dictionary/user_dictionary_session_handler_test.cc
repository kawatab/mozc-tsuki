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

#include "dictionary/user_dictionary_session_handler.h"

#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/protobuf/protobuf.h"
#include "base/protobuf/repeated_field.h"
#include "base/system_util.h"
#include "dictionary/user_dictionary_storage.pb.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/testing_util.h"

DECLARE_string(test_tmpdir);

using ::mozc::protobuf::RepeatedPtrField;
using ::mozc::user_dictionary::UserDictionary;
using ::mozc::user_dictionary::UserDictionaryCommand;
using ::mozc::user_dictionary::UserDictionaryCommandStatus;
using ::mozc::user_dictionary::UserDictionarySessionHandler;

namespace mozc {

namespace {
// "きょうと\t京都\t名詞\n"
// "!おおさか\t大阪\t地名\n"
// "\n"
// "#とうきょう\t東京\t地名\tコメント\n"
// "すずき\t鈴木\t人名\n";
const char kDictionaryData[] =
    "\xE3\x81\x8D\xE3\x82\x87\xE3\x81\x86\xE3\x81\xA8\t"
    "\xE4\xBA\xAC\xE9\x83\xBD\t\xE5\x90\x8D\xE8\xA9\x9E\n"
    "\xE3\x81\x8A\xE3\x81\x8A\xE3\x81\x95\xE3\x81\x8B\t"
    "\xE5\xA4\xA7\xE9\x98\xAA\t\xE5\x9C\xB0\xE5\x90\x8D\n"
    "\xE3\x81\xA8\xE3\x81\x86\xE3\x81\x8D\xE3\x82\x87\xE3"
    "\x81\x86\t\xE6\x9D\xB1\xE4\xBA\xAC\t\xE5\x9C\xB0\xE5"
    "\x90\x8D\t\xE3\x82\xB3\xE3\x83\xA1\xE3\x83\xB3\xE3\x83\x88\n"
    "\xE3\x81\x99\xE3\x81\x9A\xE3\x81\x8D\t\xE9\x88\xB4"
    "\xE6\x9C\xA8\t\xE4\xBA\xBA\xE5\x90\x8D\n";

// 0 means invalid dictionary id.
// c.f., UserDictionaryUtil::CreateNewDictionaryId()
const uint64 kInvalidDictionaryId = 0;
}  // namespace

class UserDictionarySessionHandlerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    original_user_profile_directory_ = SystemUtil::GetUserProfileDirectory();
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    FileUtil::Unlink(GetUserDictionaryFile());

    handler_.reset(new UserDictionarySessionHandler);
    command_.reset(new UserDictionaryCommand);
    status_.reset(new UserDictionaryCommandStatus);

    handler_->set_dictionary_path(GetUserDictionaryFile());
  }

  virtual void TearDown() {
    FileUtil::Unlink(GetUserDictionaryFile());
    SystemUtil::SetUserProfileDirectory(original_user_profile_directory_);
  }

  void Clear() {
    command_->Clear();
    status_->Clear();
  }

  static string GetUserDictionaryFile() {
    return FileUtil::JoinPath(FLAGS_test_tmpdir, "test.db");
  }

  uint64 CreateSession() {
    Clear();
    command_->set_type(UserDictionaryCommand::CREATE_SESSION);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
              status_->status());
    EXPECT_TRUE(status_->has_session_id());
    EXPECT_NE(0, status_->session_id());
    return status_->session_id();
  }

  void DeleteSession(uint64 session_id) {
    Clear();
    command_->set_type(UserDictionaryCommand::DELETE_SESSION);
    command_->set_session_id(session_id);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
              status_->status());
  }

  uint64 CreateUserDictionary(uint64 session_id, const string &name) {
    Clear();
    command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
    command_->set_session_id(session_id);
    command_->set_dictionary_name(name);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
              status_->status());
    EXPECT_TRUE(status_->has_dictionary_id());
    return status_->dictionary_id();
  }

  void AddUserDictionaryEntry(
      uint64 session_id, uint64 dictionary_id, const string &key,
      const string &value, UserDictionary::PosType pos, const string &comment) {
    Clear();
    command_->set_type(UserDictionaryCommand::ADD_ENTRY);
    command_->set_session_id(session_id);
    command_->set_dictionary_id(dictionary_id);
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key(key);
    entry->set_value(value);
    entry->set_pos(pos);
    entry->set_comment(comment);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
              status_->status());
  }

  RepeatedPtrField<UserDictionary::Entry> GetAllUserDictionaryEntries(
      uint64 session_id, uint64 dictionary_id) {
    vector<int> indices;
    const uint32 entries_size =
        GetUserDictionaryEntrySize(session_id, dictionary_id);
    for (uint32 i = 0; i < entries_size; ++i) {
      indices.push_back(i);
    }
    return GetUserDictionaryEntries(session_id, dictionary_id, indices);
  }

  RepeatedPtrField<UserDictionary::Entry> GetUserDictionaryEntries(
      uint64 session_id, uint64 dictionary_id, const vector<int> &indices) {
    Clear();
    command_->set_type(UserDictionaryCommand::GET_ENTRIES);
    command_->set_session_id(session_id);
    command_->set_dictionary_id(dictionary_id);
    for (size_t i = 0; i < indices.size(); ++i) {
      command_->add_entry_index(indices[i]);
    }
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
              status_->status());
    EXPECT_EQ(indices.size(), status_->entries_size());
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
              status_->status());
    return status_->entries();
  }

  uint32 GetUserDictionaryEntrySize(uint64 session_id, uint64 dictionary_id) {
    Clear();
    command_->set_type(UserDictionaryCommand::GET_ENTRY_SIZE);
    command_->set_session_id(session_id);
    command_->set_dictionary_id(dictionary_id);
    EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
    EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
              status_->status());
    EXPECT_TRUE(status_->has_entry_size());
    return status_->entry_size();
  }

  scoped_ptr<UserDictionarySessionHandler> handler_;
  scoped_ptr<UserDictionaryCommand> command_;
  scoped_ptr<UserDictionaryCommandStatus> status_;

 private:
  string original_user_profile_directory_;
};

TEST_F(UserDictionarySessionHandlerTest, InvalidCommand) {
  ASSERT_FALSE(handler_->Evaluate(*command_, status_.get()));

  // We cannot test setting invalid id, because it'll just fail to cast
  // (i.e. assertion error) in debug build.
}

TEST_F(UserDictionarySessionHandlerTest, NoOperation) {
  const uint64 session_id = CreateSession();

  Clear();
  command_->set_type(UserDictionaryCommand::NO_OPERATION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::NO_OPERATION);
  command_->set_session_id(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: UNKNOWN_SESSION_ID", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::NO_OPERATION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Delete the session.
  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ClearStorage) {
#ifdef __native_client__
  EXPECT_PROTO_EQ("status: UNKNOWN_ERROR", *status_);
#else
  const string &user_dictionary_file = GetUserDictionaryFile();
  // Touch the file.
  {
    OutputFileStream output(user_dictionary_file.c_str());
  }
  ASSERT_TRUE(FileUtil::FileExists(user_dictionary_file));

  command_->set_type(UserDictionaryCommand::CLEAR_STORAGE);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));

  // Should never fail.
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // The file should be removed.
  EXPECT_FALSE(FileUtil::FileExists(user_dictionary_file));
#endif  // __native_client__
}

TEST_F(UserDictionarySessionHandlerTest, CreateDeleteSession) {
  const uint64 session_id = CreateSession();

  // Without session_id, the command should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::INVALID_ARGUMENT, status_->status());

  // Test for invalid session id.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::UNKNOWN_SESSION_ID, status_->status());

  // Test for valid session.
  DeleteSession(session_id);

  // Test deletion twice should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::UNKNOWN_SESSION_ID, status_->status());
}

TEST_F(UserDictionarySessionHandlerTest, CreateTwice) {
  const uint64 session_id1 = CreateSession();
  const uint64 session_id2 = CreateSession();
  ASSERT_NE(session_id1, session_id2);

  // Here, the first session is lost, so trying to delete it should fail
  // with unknown id error, and deletion of the second session should
  // success.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_SESSION);
  command_->set_session_id(session_id1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::UNKNOWN_SESSION_ID, status_->status());

  DeleteSession(session_id2);
}

TEST_F(UserDictionarySessionHandlerTest, LoadAndSave) {
  const uint64 session_id = CreateSession();

  // First of all, create a dictionary named "dictionary".
  CreateUserDictionary(session_id, "dictionary");

  // Save the current storage.
  Clear();
  command_->set_type(UserDictionaryCommand::SAVE);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // Create another dictionary.
  CreateUserDictionary(session_id, "dictionary2");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary\" >\n"
                   "  dictionaries: < name: \"dictionary2\" >\n"
                   ">",
                   *status_);

  // Load the data to the storage. So the storage content should be reverted
  // to the saved one.
  Clear();
  command_->set_type(UserDictionaryCommand::LOAD);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"dictionary\" >\n"
      ">",
      *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, LoadWithEnsuringNonEmptyStorage) {
  const uint64 session_id = CreateSession();

  Clear();
  command_->set_type(UserDictionaryCommand::SET_DEFAULT_DICTIONARY_NAME);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("abcde");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // Load the data to the storage. It should be failed as there should be no
  // file yet. Regardless of the failure, a new dictionary should be
  // created.
  Clear();
  command_->set_type(UserDictionaryCommand::LOAD);
  command_->set_session_id(session_id);
  command_->set_ensure_non_empty_storage(true);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: FILE_NOT_FOUND", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ(
      "status: USER_DICTIONARY_COMMAND_SUCCESS\n"
      "storage: <\n"
      "  dictionaries: < name: \"abcde\" >\n"
      ">",
      *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, Undo) {
  const uint64 session_id = CreateSession();

  // At first, the session shouldn't be undoable.
  Clear();
  command_->set_type(UserDictionaryCommand::CHECK_UNDOABILITY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: NO_UNDO_HISTORY", *status_);

  // The first undo without any preceding operation should fail.
  Clear();
  command_->set_type(UserDictionaryCommand::UNDO);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: NO_UNDO_HISTORY", *status_);

  // Create a dictionary.
  CreateUserDictionary(session_id, "dictionary");

  // Now the session should be undoable.
  Clear();
  command_->set_type(UserDictionaryCommand::CHECK_UNDOABILITY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  // And then undo. This time, the command should succeed.
  Clear();
  command_->set_type(UserDictionaryCommand::UNDO);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, GetEntries) {
  const uint64 session_id = CreateSession();
  const uint64 dictionary_id = CreateUserDictionary(session_id, "dictionary");

  AddUserDictionaryEntry(session_id, dictionary_id,
                         "key1", "value1", UserDictionary::NOUN, "comment1");
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "key2", "value2", UserDictionary::NOUN, "comment2");
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "key3", "value3", UserDictionary::SYMBOL, "comment3");
  ASSERT_EQ(3, GetUserDictionaryEntrySize(session_id, dictionary_id));

  vector<int> indices;
  indices.push_back(0);
  indices.push_back(2);
  GetUserDictionaryEntries(session_id, dictionary_id, indices);
  EXPECT_PROTO_PEQ("entries: <\n"
                   "  key: \"key1\"\n"
                   "  value: \"value1\"\n"
                   "  pos: NOUN\n"
                   "  comment: \"comment1\"\n"
                   ">"
                   "entries: <\n"
                   "  key: \"key3\"\n"
                   "  value: \"value3\"\n"
                   "  pos: SYMBOL\n"
                   "  comment: \"comment3\"\n"
                   ">",
                   *status_);

  // Invalid dictionary ID
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(kInvalidDictionaryId);
  command_->add_entry_index(0);
  EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::UNKNOWN_DICTIONARY_ID,
            status_->status());

  // Invalid entry index
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  command_->add_entry_index(3);
  EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE,
            status_->status());

  // Invalid entry index
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  command_->add_entry_index(-1);
  EXPECT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::ENTRY_INDEX_OUT_OF_RANGE,
            status_->status());

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, DictionaryEdit) {
  const uint64 session_id = CreateSession();

  // Create a dictionary named "dictionary".
  CreateUserDictionary(session_id, "dictionary");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary\" >\n"
                   ">",
                   *status_);

  // Create another dictionary named "dictionary2".
  CreateUserDictionary(session_id, "dictionary2");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary\" >\n"
                   "  dictionaries: < name: \"dictionary2\" >\n"
                   ">",
                   *status_);
  const uint64 dictionary_id1 = status_->storage().dictionaries(0).id();
  const uint64 dictionary_id2 = status_->storage().dictionaries(1).id();

  // Dictionary creation without name should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::CREATE_DICTIONARY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Then rename the second dictionary to "dictionary3".
  Clear();
  command_->set_type(UserDictionaryCommand::RENAME_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id2);
  command_->set_dictionary_name("dictionary3");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary\" >\n"
                   "  dictionaries: < name: \"dictionary3\" >\n"
                   ">",
                   *status_);
  EXPECT_EQ(dictionary_id1, status_->storage().dictionaries(0).id());
  EXPECT_EQ(dictionary_id2, status_->storage().dictionaries(1).id());

  // Dictionary renaming without dictionary_id or new name should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::RENAME_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id2);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::RENAME_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("new dictionary name");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Then delete the first dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"dictionary3\" >\n"
                   ">",
                   *status_);
  EXPECT_EQ(dictionary_id2, status_->storage().dictionaries(0).id());

  // Dictionary deletion without dictionary id should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_DICTIONARY);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Then delete the dictionary again with the ensure_non_empty_dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::SET_DEFAULT_DICTIONARY_NAME);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("abcde");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_DICTIONARY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id2);
  command_->set_ensure_non_empty_storage(true);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::GET_USER_DICTIONARY_NAME_LIST);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage: <\n"
                   "  dictionaries: < name: \"abcde\" >\n"
                   ">",
                   *status_);
  EXPECT_NE(dictionary_id2, status_->storage().dictionaries(0).id());

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, AddEntry) {
  const uint64 session_id = CreateSession();
  const uint64 dictionary_id = CreateUserDictionary(session_id, "dictionary");
  ASSERT_EQ(0, GetUserDictionaryEntrySize(session_id, dictionary_id));

  // Add an entry.
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading", "word", UserDictionary::NOUN, "");
  ASSERT_EQ(1, GetUserDictionaryEntrySize(session_id, dictionary_id));
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ("entries: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">\n",
                   *status_);

  // AddEntry without dictionary_id or entry should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::ADD_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::ADD_ENTRY);
  command_->set_session_id(session_id);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading");
    entry->set_value("word");
    entry->set_pos(UserDictionary::NOUN);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, EditEntry) {
  const uint64 session_id = CreateSession();
  const uint64 dictionary_id = CreateUserDictionary(session_id, "dictionary");
  ASSERT_EQ(0, GetUserDictionaryEntrySize(session_id, dictionary_id));

  // Add an entry.
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading", "word", UserDictionary::NOUN, "");
  ASSERT_EQ(1, GetUserDictionaryEntrySize(session_id, dictionary_id));

  // Add another entry.
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading2", "word2", UserDictionary::NOUN, "");
  ASSERT_EQ(2, GetUserDictionaryEntrySize(session_id, dictionary_id));
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ("entries: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">\n"
                   "entries: <\n"
                   "  key: \"reading2\"\n"
                   "  value: \"word2\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);

  ASSERT_EQ(2, GetUserDictionaryEntrySize(session_id, dictionary_id));
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ("entries: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">"
                   "entries: <\n"
                   "  key: \"reading3\"\n"
                   "  value: \"word3\"\n"
                   "  pos: PREFIX\n"
                   ">",
                   *status_);

  // EditEntry without dictionary_id or entry should be failed.
  // Also, the number of entry_index should exactly equals to '1'.
  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->add_entry_index(1);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::EDIT_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(0);
  command_->add_entry_index(1);
  {
    UserDictionary::Entry *entry = command_->mutable_entry();
    entry->set_key("reading3");
    entry->set_value("word3");
    entry->set_pos(UserDictionary::PREFIX);
  }
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, DeleteEntry) {
  const uint64 session_id = CreateSession();
  const uint64 dictionary_id = CreateUserDictionary(session_id, "dictionary");
  ASSERT_EQ(0, GetUserDictionaryEntrySize(session_id, dictionary_id));

  // Add entries.
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading", "word", UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading2", "word2", UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading3", "word3", UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading4", "word4", UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id,
                         "reading5", "word5", UserDictionary::NOUN, "");
  ASSERT_EQ(5, GetUserDictionaryEntrySize(session_id, dictionary_id));

  // Delete the second and fourth entries.
  Clear();
  command_->set_type(UserDictionaryCommand::DELETE_ENTRY);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->add_entry_index(1);
  command_->add_entry_index(3);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_EQ(UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS,
            status_->status());
  ASSERT_EQ(3, GetUserDictionaryEntrySize(session_id, dictionary_id));
  GetAllUserDictionaryEntries(session_id, dictionary_id);
  EXPECT_PROTO_PEQ("entries: <\n"
                   "  key: \"reading\"\n"
                   "  value: \"word\"\n"
                   "  pos: NOUN\n"
                   ">"
                   "entries: <\n"
                   "  key: \"reading3\"\n"
                   "  value: \"word3\"\n"
                   "  pos: NOUN\n"
                   ">"
                   "entries: <\n"
                   "  key: \"reading5\"\n"
                   "  value: \"word5\"\n"
                   "  pos: NOUN\n"
                   ">",
                   *status_);

  // Entry deletion without dictionary_id or entry_index should be failed.
  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->add_entry_index(0);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);
  ASSERT_EQ(3, GetUserDictionaryEntrySize(session_id, dictionary_id));

  Clear();
  command_->set_type(UserDictionaryCommand::GET_ENTRIES);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);
  ASSERT_EQ(3, GetUserDictionaryEntrySize(session_id, dictionary_id));

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ImportData1) {
  const uint64 session_id = CreateSession();

  // First of all, create a dictionary named "dictionary".
  const uint64 dictionary_id = CreateUserDictionary(session_id, "dictionary");

  // Import data to the dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_dictionary_id());
  EXPECT_EQ(dictionary_id, status_->dictionary_id());

  // Make sure the size of the data.
  ASSERT_EQ(4, GetUserDictionaryEntrySize(session_id, dictionary_id));

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ImportData2) {
  const uint64 session_id = CreateSession();

  // Import data to a new dictionary.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("user dictionary");
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS", *status_);
  ASSERT_TRUE(status_->has_dictionary_id());
  const uint64 dictionary_id = status_->dictionary_id();

  // Make sure the size of the data.
  ASSERT_EQ(4, GetUserDictionaryEntrySize(session_id, dictionary_id));

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, ImportDataFailure) {
  const uint64 session_id = CreateSession();
  const uint64 dictionary_id = CreateUserDictionary(session_id, "dictionary");

  // Fail if the data is missing.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_name("user dictionary");
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_dictionary_id(dictionary_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  // Fail if neither dictionary_name nor dictionary_id is set.
  Clear();
  command_->set_type(UserDictionaryCommand::IMPORT_DATA);
  command_->set_session_id(session_id);
  command_->set_data(kDictionaryData);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_EQ("status: INVALID_ARGUMENT", *status_);

  DeleteSession(session_id);
}

TEST_F(UserDictionarySessionHandlerTest, GetStorage) {
  const uint64 session_id = CreateSession();
  const uint64 dictionary_id1 = CreateUserDictionary(session_id, "dictionary1");

  AddUserDictionaryEntry(session_id, dictionary_id1,
                         "reading1_1", "word1_1", UserDictionary::NOUN, "");
  AddUserDictionaryEntry(session_id, dictionary_id1,
                         "reading1_2", "word1_2", UserDictionary::NOUN, "");

  // Create a dictionary named "dictionary2".
  const uint64 dictionary_id2 = CreateUserDictionary(session_id, "dictionary2");

  AddUserDictionaryEntry(session_id, dictionary_id2,
                         "reading2_1", "word2_1", UserDictionary::NOUN, "");

  Clear();
  command_->set_type(UserDictionaryCommand::GET_STORAGE);
  command_->set_session_id(session_id);
  ASSERT_TRUE(handler_->Evaluate(*command_, status_.get()));
  EXPECT_PROTO_PEQ("status: USER_DICTIONARY_COMMAND_SUCCESS\n"
                   "storage <\n"
                   "  dictionaries <\n"
                   "    name: \"dictionary1\"\n"
                   "    entries <\n"
                   "      key: \"reading1_1\"\n"
                   "      value: \"word1_1\"\n"
                   "      comment: \"\"\n"
                   "      pos: NOUN\n"
                   "    >\n"
                   "    entries <\n"
                   "      key: \"reading1_2\"\n"
                   "      value: \"word1_2\"\n"
                   "      comment: \"\"\n"
                   "      pos: NOUN\n"
                   "    >\n"
                   "  >\n"
                   "  dictionaries <\n"
                   "    name: \"dictionary2\"\n"
                   "    entries <\n"
                   "      key: \"reading2_1\"\n"
                   "      value: \"word2_1\"\n"
                   "      comment: \"\"\n"
                   "      pos: NOUN\n"
                   "    >\n"
                   "  >\n"
                   ">\n",
                   *status_);

  DeleteSession(session_id);
}
}  // namespace mozc
