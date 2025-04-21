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

#ifndef MOZC_DICTIONARY_USER_DICTIONARY_SESSION_HANDLER_H_
#define MOZC_DICTIONARY_USER_DICTIONARY_SESSION_HANDLER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/strings/assign.h"
#include "dictionary/user_dictionary_session.h"
#include "dictionary/user_dictionary_util.h"
#include "protocol/user_dictionary_storage.pb.h"
#include "absl/random/random.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace user_dictionary {

// Interface between UserDictionarySession and protocol buffers.
class UserDictionarySessionHandler {
 public:
  UserDictionarySessionHandler() = default;
  UserDictionarySessionHandler(const UserDictionarySessionHandler &) = delete;
  UserDictionarySessionHandler &operator=(
      const UserDictionarySessionHandler &) = delete;

  bool Evaluate(const UserDictionaryCommand &command,
                UserDictionaryCommandStatus *status);

  void NoOperation(const UserDictionaryCommand &command,
                   UserDictionaryCommandStatus *status);
  void ClearStorage(const UserDictionaryCommand &command,
                    UserDictionaryCommandStatus *status);

  void CreateSession(const UserDictionaryCommand &command,
                     UserDictionaryCommandStatus *status);
  void DeleteSession(const UserDictionaryCommand &command,
                     UserDictionaryCommandStatus *status);

  void SetDefaultDictionaryName(const UserDictionaryCommand &command,
                                UserDictionaryCommandStatus *status);

  void CheckUndoability(const UserDictionaryCommand &command,
                        UserDictionaryCommandStatus *status);
  void Undo(const UserDictionaryCommand &command,
            UserDictionaryCommandStatus *status);

  void Load(const UserDictionaryCommand &command,
            UserDictionaryCommandStatus *status);
  void Save(const UserDictionaryCommand &command,
            UserDictionaryCommandStatus *status);

  void GetUserDictionaryNameList(const UserDictionaryCommand &command,
                                 UserDictionaryCommandStatus *status);
  void GetEntrySize(const UserDictionaryCommand &command,
                    UserDictionaryCommandStatus *status);
  void GetEntries(const UserDictionaryCommand &command,
                  UserDictionaryCommandStatus *status);

  void CheckNewDictionaryAvailability(const UserDictionaryCommand &command,
                                      UserDictionaryCommandStatus *status);
  void CreateDictionary(const UserDictionaryCommand &command,
                        UserDictionaryCommandStatus *status);
  void DeleteDictionary(const UserDictionaryCommand &command,
                        UserDictionaryCommandStatus *status);
  void RenameDictionary(const UserDictionaryCommand &command,
                        UserDictionaryCommandStatus *status);

  void CheckNewEntryAvailability(const UserDictionaryCommand &command,
                                 UserDictionaryCommandStatus *status);
  void AddEntry(const UserDictionaryCommand &command,
                UserDictionaryCommandStatus *status);
  void EditEntry(const UserDictionaryCommand &command,
                 UserDictionaryCommandStatus *status);
  void DeleteEntry(const UserDictionaryCommand &command,
                   UserDictionaryCommandStatus *status);

  void ImportData(const UserDictionaryCommand &command,
                  UserDictionaryCommandStatus *status);

  void GetStorage(const UserDictionaryCommand &command,
                  UserDictionaryCommandStatus *status);

  void set_dictionary_path(absl::string_view dictionary_path) {
    strings::Assign(dictionary_path_, dictionary_path);
  }

 private:
  static constexpr uint64_t kInvalidSessionId = 0;

  UserDictionarySession *GetSession(const UserDictionaryCommand &command,
                                    UserDictionaryCommandStatus *status);
  uint64_t CreateNewSessionId() const;

  // As an interface, this class can hold multiple sessions,
  // but currently only one latest session is held.
  // (From the different point of view, this is LRU with max capacity '1'.)
  uint64_t session_id_ = kInvalidSessionId;
  std::unique_ptr<UserDictionarySession> session_;
  std::string dictionary_path_ =
      UserDictionaryUtil::GetUserDictionaryFileName();
  mutable absl::BitGen bitgen_;
};

}  // namespace user_dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_USER_DICTIONARY_SESSION_HANDLER_H_
