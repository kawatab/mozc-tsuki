// Copyright 2010-2021, Google Inc.
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
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_pos_interface.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace dictionary {

class UserDictionary : public DictionaryInterface {
 public:
  UserDictionary(std::unique_ptr<const UserPosInterface> user_pos,
                 PosMatcher pos_matcher,
                 SuppressionDictionary *suppression_dictionary);

  UserDictionary(const UserDictionary &) = delete;
  UserDictionary &operator=(const UserDictionary &) = delete;

  ~UserDictionary() override;

  bool HasKey(absl::string_view key) const override;
  bool HasValue(absl::string_view value) const override;

  // Lookup methods don't support kana modifier insensitive lookup, i.e.,
  // Callback::OnActualKey() is never called.
  void LookupPredictive(absl::string_view key,
                        const ConversionRequest &conversion_request,
                        Callback *callback) const override;
  void LookupPrefix(absl::string_view key,
                    const ConversionRequest &conversion_request,
                    Callback *callback) const override;
  void LookupExact(absl::string_view key,
                   const ConversionRequest &conversion_request,
                   Callback *callback) const override;
  void LookupReverse(absl::string_view key,
                     const ConversionRequest &conversion_request,
                     Callback *callback) const override;

  // Looks up a user comment from a pair of key and value.  When (key, value)
  // doesn't exist in this dictionary or user comment is empty, bool is
  // returned and string is kept as-is.
  bool LookupComment(absl::string_view key, absl::string_view value,
                     const ConversionRequest &conversion_request,
                     std::string *comment) const override;

  // Loads dictionary from UserDictionaryStorage.
  // mainly for unittesting
  bool Load(const user_dictionary::UserDictionaryStorage &storage);

  // Reloads dictionary asynchronously
  bool Reload() override;

  // Waits until reloader finishes
  void WaitForReloader();

  // Gets the user POS list.
  std::vector<std::string> GetPosList() const;

  // Sets user dicitonary filename for unittesting
  static void SetUserDictionaryName(const std::string &filename);

  enum RequestType { PREFIX, PREDICTIVE, EXACT };

  // Populates Token from UserToken.
  // This method sets the actual cost and rewrites POS id depending
  // on the POS and attribute.
  void PopulateTokenFromUserPosToken(
      const UserPosInterface::Token &user_pos_token, RequestType request_type,
      Token *token) const;

 private:
  class TokensIndex;
  class UserDictionaryReloader;

  // Swaps internal tokens index to |new_tokens|.
  void Swap(TokensIndex *new_tokens);

  std::unique_ptr<UserDictionaryReloader> reloader_;
  std::unique_ptr<const UserPosInterface> user_pos_;
  const PosMatcher pos_matcher_;
  SuppressionDictionary *suppression_dictionary_;
  TokensIndex *tokens_;
  mutable absl::Mutex mutex_;

  friend class UserDictionaryTest;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_H_
