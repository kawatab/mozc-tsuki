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

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_H_

#include <memory>
#include <string>
#include <vector>

#include "base/port.h"
#include "base/string_piece.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_pos_interface.h"
#include "protocol/user_dictionary_storage.pb.h"

namespace mozc {

class ReaderWriterMutex;

namespace dictionary {

class UserDictionary : public DictionaryInterface {
 public:
  UserDictionary(const UserPOSInterface *user_pos,
                 POSMatcher pos_matcher,
                 SuppressionDictionary *suppression_dictionary);
  ~UserDictionary() override;

  bool HasKey(StringPiece key) const override;
  bool HasValue(StringPiece value) const override;

  // Lookup methods don't support kana modifier insensitive lookup, i.e.,
  // Callback::OnActualKey() is never called.
  void LookupPredictive(StringPiece key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override;
  void LookupPrefix(StringPiece key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override;
  void LookupExact(StringPiece key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override;
  void LookupReverse(StringPiece str,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override;

  // Looks up a user comment from a pair of key and value.  When (key, value)
  // doesn't exist in this dictionary or user comment is empty, bool is
  // returned and string is kept as-is.
  bool LookupComment(StringPiece key, StringPiece value,
                     const ConversionRequest &conversion_request,
                     string *comment) const override;

  // Loads dictionary from UserDictionaryStorage.
  // mainly for unittesting
  bool Load(const user_dictionary::UserDictionaryStorage &storage);

  // Reloads dictionary asynchronously
  bool Reload() override;

  // Waits until reloader finishes
  void WaitForReloader();

  // Adds new word to auto registered dictionary and reload asynchronously.
  // Note that this method will not guarantee that
  // new word is added successfully, since the actual
  // dictionary modification is done by other thread.
  // Also, this method should be called by the main converter thread which
  // is executed synchronously with user input.
  bool AddToAutoRegisteredDictionary(
      const string &key, const string &value,
      const ConversionRequest &conversion_request,
      user_dictionary::UserDictionary::PosType pos);

  // Sets user dicitonary filename for unittesting
  static void SetUserDictionaryName(const string &filename);

 private:
  class TokensIndex;
  class UserDictionaryReloader;

  // Swaps internal tokens index to |new_tokens|.
  void Swap(TokensIndex *new_tokens);

  std::unique_ptr<UserDictionaryReloader> reloader_;
  std::unique_ptr<const UserPOSInterface> user_pos_;
  const POSMatcher pos_matcher_;
  SuppressionDictionary *suppression_dictionary_;
  TokensIndex *tokens_;
  mutable std::unique_ptr<ReaderWriterMutex> mutex_;

  friend class UserDictionaryTest;
  DISALLOW_COPY_AND_ASSIGN(UserDictionary);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_H_
