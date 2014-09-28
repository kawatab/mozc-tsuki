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

// UserDicStorageInterface provides interface for accessing data
// storage of the user dictionary. Subclasses determine how to save
// dictionary data on a disk.

// The followings are not responsibility of UserDicStorageInterface
// and supposed to be performed by its client.
//
// (1) Validation of input values.
//   Subclasses that implement the interface of
//   UserDicStorageInterface are supposed to perform only minimal
//   validation of input value, for example a subclass that saves
//   dictionary data in a tab-separated text file usually doesn't
//   accept input with a tab or new line character and should check
//   input for them. However, UserDicStorageInterface and its
//   subclasses don't care about any more complicated
//   application-level validity of data like an acceptable POS set,
//   character encoding and so on. The class takes input value as it
//   is.
//
// (2) Duplicate entry elimination.
//   UserDicStorageInterface treats an entry in it with a unique
//   integer key. This means it doesn't take into account any actual
//   attribute of the entry when distinguishing it from another. If
//   any kind of duplicate elimination is necessary, it should be done
//   before the value is passed to the class.
//
// (3) Importing a dictionary file of Mozc or third party IMEs.
//   UserDicStorageInterface provides CreateDictionary() and
//   AddEntry(). Clients of the class can import an external
//   dictionary file using the two member functions.

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_STORAGE_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_STORAGE_H_

#include <string>
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "dictionary/user_dictionary_storage.pb.h"

namespace mozc {

class Mutex;
class ProcessMutex;

// Inherit from ProtocolBuffer
// TODO(hidehiko): Get rid of this implementation.
class UserDictionaryStorage : public user_dictionary::UserDictionaryStorage {
 public:
  typedef user_dictionary::UserDictionary UserDictionary;
  typedef user_dictionary::UserDictionary::Entry UserDictionaryEntry;

  enum UserDictionaryStorageErrorType {
    USER_DICTIONARY_STORAGE_NO_ERROR = 0,  // default
    FILE_NOT_EXISTS,
    BROKEN_FILE,
    SYNC_FAILURE,
    TOO_BIG_FILE_BYTES,
    INVALID_DICTIONARY_ID,
    INVALID_CHARACTERS_IN_DICTIONARY_NAME,
    EMPTY_DICTIONARY_NAME,
    DUPLICATED_DICTIONARY_NAME,
    TOO_LONG_DICTIONARY_NAME,
    TOO_MANY_DICTIONARIES,
    TOO_MANY_ENTRIES,
    EXPORT_FAILURE,
    UNKNOWN_ERROR,
    ERROR_TYPE_SIZE
  };

  explicit UserDictionaryStorage(const string &filename);
  virtual ~UserDictionaryStorage();

  // return the filename of user dictionary
  const string &filename() const;

  // Return true if data tied with this object already
  // exists. Otherwise, it means that the space for the data is used
  // for the first time.
  bool Exists() const;

  // Load user dictionary from the file.
  bool Load();

  // Loads user dictionary from the file. Usually, it should be able to
  // read both files in the older format, whose pos is numbered '3',
  // and in the newer format, whose pos is numbered '5' in enum format.
  // Load() declared above handles to fill the gap actually. So in most cases
  // what clients of this class need is just invoke Load().
  // However, there are some special cases that a client doesn't want to
  // fill the gap automatically. For such cases, this class provides the
  // method to do it.
  // TODO(hidehiko,peria): Remove this method when we get rid of supporting
  //   older format in sync.
  bool LoadWithoutMigration();

  // Serialzie user dictionary to local file.
  // Need to call Lock() the dictionary before calling Save().
  bool Save();

  // Lock the dictionary so that other processes/threads cannot
  // execute mutable operations on this dictionary.
  bool Lock();

  // release the lock
  bool UnLock();

  // Export a dictionary to a file in TSV format.
  bool ExportDictionary(uint64 dic_id, const string &file_name);

  // Create a new dictionary with a specified name. Returns the id of
  // the new instance via new_dic_id.

  bool CreateDictionary(const string &dic_name, uint64 *new_dic_id);

  // Create a copy of an existing dictionary giving it a specified
  // name. Returns the id of the new dictionary via new_dic_id.
  bool CopyDictionary(uint64 dic_id, const string &dic_name,
                      uint64 *new_dic_id);

  // Delete a dictionary.
  bool DeleteDictionary(uint64 dic_id);

  // Rename a dictionary.
  bool RenameDictionary(uint64 dic_id, const string &dic_name);

  // return the index of "dic_id"
  // return -1 if no dictionary is found.
  int GetUserDictionaryIndex(uint64 dic_id) const;

  // return mutable UserDictionary corresponding to dic_id
  UserDictionary *GetUserDictionary(uint64 dic_id);

  // Searches a dictionary from a dictionary name, and the dictionary id is
  // stored in "dic_id".
  // Returns false if the name is not found.
  bool GetUserDictionaryId(const string &dic_name, uint64 *dic_id);

  // return last error type.
  // You can obtain the reason of the error of dictionary operation.
  UserDictionaryStorageErrorType GetLastError() const;

  // Add new entry to the auto registered dictionary.
  bool AddToAutoRegisteredDictionary(const string &key,
                                     const string &value,
                                     UserDictionary::PosType pos);

  // Converts syncable dictionaries to unsyncable dictionaries.
  // The name of default sync dictionary is renamed to locale-independent name
  // like other unsyncable dictionaries.
  // This method deletes syncable dictionaries which are marked as removed or
  // don't have any dictionary entries.
  // Returns true if this method converts some dictionaries.
  bool ConvertSyncDictionariesToNormalDictionaries();

  // return the number of dictionaries with "synclbe" being true.
  static int CountSyncableDictionaries(
      const user_dictionary::UserDictionaryStorage &storage);

  // maxium number of dictionaries this storage can hold
  static size_t max_dictionary_size();

  // maximum number of entries one dictionary can hold
  static size_t max_entry_size();

  static string default_sync_dictionary_name();

 private:
  // Load the data from |file_name_|. This method migrates older file format
  // based on given flags.
  bool LoadAndMigrateDictionaries(bool run_migration);

  // Return true if this object can accept the given dictionary name.
  // This changes the internal state.
  bool IsValidDictionaryName(const string &name);

  // Load the data from file_name actually.
  bool LoadInternal(bool run_migration);

  string file_name_;
  bool locked_;
  UserDictionaryStorageErrorType last_error_type_;
  scoped_ptr<Mutex> local_mutex_;
  scoped_ptr<ProcessMutex> process_mutex_;
};
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_STORAGE_H_
