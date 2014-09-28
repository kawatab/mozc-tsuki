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

// UserDicUtil provides various utility functions related to the user
// dictionary.

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_UTIL_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_UTIL_H_

#include <string>
#include <vector>
#include "base/port.h"
#include "dictionary/user_dictionary_storage.pb.h"

namespace mozc {

class UserPOSInterface;

// TODO(hidehiko): Move this class into user_dictionary namespace.
class UserDictionaryUtil {
 public:
  // Following methods return limits of dictionary/entry size.
  static size_t max_dictionary_size();
  static size_t max_entry_size();

  // Returns true if all characters in the given string is a legitimate
  // character for reading.
  static bool IsValidReading(const string &reading);

  // Performs varirous kinds of character normalization such as
  // katakana-> hiragana and full-width ascii -> half width
  // ascii. Identity of reading of a word should be defined by the
  // output of this function.
  static void NormalizeReading(const string &input, string *output);

  // Returns true if all fields of the given data is properly set and
  // have a legitimate value. It checks for an empty string, an
  // invalid character and so on. If the function returns false, we
  // shouldn't accept the data being passed into the dictionary.
  // TODO(hidehikoo): Replace this method by the following ValidateEntry.
  static bool IsValidEntry(
      const UserPOSInterface &user_pos,
      const user_dictionary::UserDictionary::Entry &entry);

  // Returns the error status of the validity for the given entry.
  // The validation process is as follows:
  // - Checks the reading
  //   - if it isn't empty
  //   - if it doesn't exceed the max length
  //   - if it doesn't contain invalid character
  // - Checks the word
  //   - if it isn't empty
  //   - if it doesn't exceed the max length
  //   - if it doesn't contain invalid character
  // - Checks the comment
  //   - if it isn't exceed the max length
  //   - if it doesn't contain invalid character
  // - Checks if a valid pos type is set.
  static user_dictionary::UserDictionaryCommandStatus::Status ValidateEntry(
      const user_dictionary::UserDictionary::Entry &entry);

  // Sanitizes a dictionary entry so that it's acceptable to the
  // class. A user of the class may want this function to make sure an
  // error want happen before calling AddEntry() and other
  // methods. Return true if the entry is changed.
  static bool SanitizeEntry(user_dictionary::UserDictionary::Entry *entry);

  // Helper function for SanitizeEntry
  // "max_size" is the maximum allowed size of str. If str size exceeds
  // "max_size", remaining part is truncated by this function.
  static bool Sanitize(string *str, size_t max_size);

  // Returns the error status of the validity for the given dictionary name.
  static user_dictionary::UserDictionaryCommandStatus::Status
  ValidateDictionaryName(const user_dictionary::UserDictionaryStorage &storage,
                         const string &dictionary_name);

  // Returns true if the given storage hits the limit for the number of
  // dictionaries.
  static bool IsStorageFull(
      const user_dictionary::UserDictionaryStorage &storage);

  // Returns true if the given dictionary hits the limit for the number of
  // entries.
  static bool IsDictionaryFull(
      const user_dictionary::UserDictionary &dictionary);

  // Returns UserDictionary with the given id, or NULL if not found.
  static const user_dictionary::UserDictionary *GetUserDictionaryById(
      const user_dictionary::UserDictionaryStorage &storage,
      uint64 dictionary_id);
  static user_dictionary::UserDictionary *GetMutableUserDictionaryById(
      user_dictionary::UserDictionaryStorage *storage,
      uint64 dictionary_id);

  // Returns the index of the dictionary with the given dictionary_id
  // in the storage, or -1 if not found.
  static int GetUserDictionaryIndexById(
      const user_dictionary::UserDictionaryStorage &storage,
      uint64 dictionary_id);

  // Returns the file name of UserDictionary.
  static string GetUserDictionaryFileName();

  // Returns the string representation of PosType, or NULL if the given
  // pos is invalid.
  // For historicall reason, the pos was represented in Japanese characters.
  static const char* GetStringPosType(
      user_dictionary::UserDictionary::PosType pos_type);

  // Returns the string representation of PosType, or NULL if the given
  // pos is invalid.
  static user_dictionary::UserDictionary::PosType ToPosType(
      const char *string_pos_type);

  // Tries to resolve the unknown fields in UserDictionary.
  // This is introduced for the change of protobuf refactoring.
  static bool ResolveUnknownFieldSet(
      user_dictionary::UserDictionaryStorage *storage);

  // To keep a way to re-install old stable version (1.5 or earlier),
  // we temporarily fill the legacy (deprecated) pos field in string format
  // on desktop version.
  static void FillDesktopDeprecatedPosField(
      user_dictionary::UserDictionaryStorage *storage);

  // Generates a new dictionary id, i.e. id which is not in the storage.
  static uint64 CreateNewDictionaryId(
      const user_dictionary::UserDictionaryStorage &storage);

  // Creates dictionary with the given name.
  static user_dictionary::UserDictionaryCommandStatus::Status CreateDictionary(
      user_dictionary::UserDictionaryStorage *storage,
      const string &dictionary_name,
      uint64 *new_dictionary_id);

  // Deletes dictionary specified by the given dictionary_id.
  // If the deleted_dictionary is not NULL, the pointer to the
  // delete dictionary is stored into it. In other words,
  // caller has responsibility to actual deletion of the instance.
  // Returns true if succeeded, otherwise false.
  static bool DeleteDictionary(
      user_dictionary::UserDictionaryStorage *storage,
      uint64 dictionary_id,
      int *original_index,
      user_dictionary::UserDictionary **deleted_dictionary);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UserDictionaryUtil);
};
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_UTIL_H_
