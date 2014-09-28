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

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_SESSION_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_SESSION_H_

#include <deque>

#include "base/scoped_ptr.h"
#include "base/port.h"
#include "dictionary/user_dictionary_storage.pb.h"

namespace mozc {

class UserDictionaryStorage;

namespace user_dictionary {

// Session instance to edit UserDictionaryStorage.
// TODO(hidehiko): Replace mozc::UserDictionaryStorage by this class.
class UserDictionarySession {
 public:
  // An interface to implement the undo operation.
  class UndoCommand;

  explicit UserDictionarySession(const string &filepath);
  ~UserDictionarySession();

  const UserDictionaryStorage &storage() const;

  // This method is introduced for backword compatibility to make the change
  // step-by-step.
  // TODO(hidehiko): remove this method when the refactoring is done.
  mozc::UserDictionaryStorage *mutable_storage();

  // Sets the default dictionary name.
  UserDictionaryCommandStatus::Status SetDefaultDictionaryName(
      const string &dictionary_name);

  // Loads the data from local storage.
  UserDictionaryCommandStatus::Status Load();

  // Loads the data from local storage.
  // If the result is empty (regardless of the command is successfully done
  // or not), creates an empty dictionary in the storage with the default name.
  UserDictionaryCommandStatus::Status LoadWithEnsuringNonEmptyStorage();

  // Saves the data to local stroage.
  UserDictionaryCommandStatus::Status Save();

  // Undoes the last operation.
  // Returns true if succeeded, otherwise false.
  UserDictionaryCommandStatus::Status Undo();

  // Creates a new dictionary.
  UserDictionaryCommandStatus::Status CreateDictionary(
      const string &dictionary_name, uint64 *new_dictionary_id);

  // Deletes the dictionary of the given dictionary_id.
  UserDictionaryCommandStatus::Status DeleteDictionary(uint64 dictionary_id);

  // Deletes the dictionary of the given dictionary_id.
  // If the dictionary gets empty as the result of deletion, creates an
  // empty dictionary in the storage with the default name.
  UserDictionaryCommandStatus::Status
  DeleteDictionaryWithEnsuringNonEmptyStorage(uint64 dictionary_id);

  // Renames the dictionary of the given dictionary_id to dictionary_name.
  UserDictionaryCommandStatus::Status RenameDictionary(
      uint64 dictionary_id, const string &dictionary_name);

  // Adds an entry with given key, value and pos_type to the dictionary
  // specified by the dicitonary_id.
  UserDictionaryCommandStatus::Status AddEntry(
      uint64 dictionary_id, const UserDictionary::Entry &entry);

  // Edits the entry at "index" in the dictionary specified by dictionary_id
  // to the given key, value and pos_type.
  UserDictionaryCommandStatus::Status EditEntry(
      uint64 dictionary_id, int index, const UserDictionary::Entry &entry);

  // Deletes the entries in the dictionary specified by dictionary_id.
  UserDictionaryCommandStatus::Status DeleteEntry(
      uint64 dictionary_id, const vector<int> &index_list);

  // Imports entries from the text data into the dictionary with dictionary_id.
  UserDictionaryCommandStatus::Status ImportFromString(
      uint64 dictionary_id, const string &data);

  // Imports entries from the text data into a newly created dictionary.
  UserDictionaryCommandStatus::Status ImportToNewDictionaryFromString(
      const string &dictionary_name, const string &data,
      uint64 *new_dictionary_id);

  // Returns true if the session has undo-able history.
  bool has_undo_history() const { return !undo_history_.empty(); }

 private:
  bool EnsureNonEmptyStorage();
  UserDictionaryCommandStatus::Status LoadInternal(
      bool ensure_non_empty_storage);
  UserDictionaryCommandStatus::Status DeleteDictionaryInternal(
      uint64 dictionary_id, bool ensure_non_empty_storage);
  UserDictionaryCommandStatus::Status ImportFromStringInternal(
      UserDictionary *dictionary, const string &data);

  void ClearUndoHistory();
  void AddUndoCommand(UndoCommand *undo_command);

  scoped_ptr<mozc::UserDictionaryStorage> storage_;
  string default_dictionary_name_;
  deque<UndoCommand*> undo_history_;

  DISALLOW_COPY_AND_ASSIGN(UserDictionarySession);
};

}  // namespace user_dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_SESSION_H_
