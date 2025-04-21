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

// Session manager of Mozc server.

#ifndef MOZC_SESSION_SESSION_HANDLER_H_
#define MOZC_SESSION_SESSION_HANDLER_H_

#include <cstdint>
#include <memory>

#include "composer/table.h"
#include "dictionary/user_dictionary_session_handler.h"
#include "engine/engine_builder_interface.h"
#include "engine/engine_interface.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/common.h"
#include "session/internal/keymap.h"
#include "session/session_handler_interface.h"
#include "session/session_interface.h"
#include "session/session_observer_handler.h"
#include "storage/lru_cache.h"
#include "testing/gunit_prod.h"  // for FRIEND_TEST()
#include "absl/random/random.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

#ifndef MOZC_DISABLE_SESSION_WATCHDOG
#include "session/session_watch_dog.h"
#endif  // MOZC_DISABLE_SESSION_WATCHDOG

namespace mozc {

class SessionHandler : public SessionHandlerInterface {
 public:
  explicit SessionHandler(std::unique_ptr<EngineInterface> engine);
  SessionHandler(std::unique_ptr<EngineInterface> engine,
                 std::unique_ptr<EngineBuilderInterface> engine_builder);
  SessionHandler(const SessionHandler &) = delete;
  SessionHandler &operator=(const SessionHandler &) = delete;
  ~SessionHandler() override;

  // Returns true if SessionHandle is available.
  bool IsAvailable() const override;

  bool EvalCommand(commands::Command *command) override;

  // Starts watch dog timer to cleanup sessions.
  bool StartWatchDog() override;

  // NewSession returns new Session.
  // Client needs to delete it properly
  session::SessionInterface *NewSession();

  void AddObserver(session::SessionObserverInterface *observer) override;
  absl::string_view GetDataVersion() const override {
    return engine_->GetDataVersion();
  }

  const EngineInterface &engine() const { return *engine_; }

 private:
  FRIEND_TEST(SessionHandlerTest, StorageTest);
  FRIEND_TEST(SessionHandlerTest, KeyMapTest);

  using SessionMap =
      mozc::storage::LruCache<SessionID, session::SessionInterface *>;
  using SessionElement = SessionMap::Element;

  void Init(std::unique_ptr<EngineInterface> engine,
            std::unique_ptr<EngineBuilderInterface> engine_builder);

  // Updates the config, if the |command| contains the config.
  void MaybeUpdateConfig(commands::Command *command);

  bool CreateSession(commands::Command *command);
  bool DeleteSession(commands::Command *command);
  bool TestSendKey(commands::Command *command);
  bool SendKey(commands::Command *command);
  bool SendCommand(commands::Command *command);
  // Syncs internal data to local file system and wait for finish.
  bool SyncData(commands::Command *command);
  bool ClearUserHistory(commands::Command *command);
  bool ClearUserPrediction(commands::Command *command);
  bool ClearUnusedUserPrediction(commands::Command *command);
  bool Shutdown(commands::Command *command);
  // Reloads all the sessions.
  // Before that, UpdateSessions() is called to update them.
  bool Reload(commands::Command *command);
  // Reloads and waits for reloader finish.
  bool ReloadAndWait(commands::Command *command);
  bool GetConfig(commands::Command *command);
  bool SetConfig(commands::Command *command);
  // Updates all the sessions by UpdateSessions() with given |request|.
  bool SetRequest(commands::Command *command);
  // Sets the given config, request, and delivertive information
  // to all the sessions.
  // Then updates config_ and request_.
  // This method doesn't reload the sessions.
  void UpdateSessions(const config::Config &config,
                      const commands::Request &request);

  bool Cleanup(commands::Command *command);
  bool SendUserDictionaryCommand(commands::Command *command);
  bool SendEngineReloadRequest(commands::Command *command);
  bool NoOperation(commands::Command *command);
  bool CheckSpelling(commands::Command *command);
  bool ReloadSpellChecker(commands::Command *command);

  SessionID CreateNewSessionID();
  bool DeleteSessionID(SessionID id);

  std::unique_ptr<SessionMap> session_map_;
#ifndef MOZC_DISABLE_SESSION_WATCHDOG
  std::unique_ptr<SessionWatchDog> session_watch_dog_;
#endif  // MOZC_DISABLE_SESSION_WATCHDOG
  bool is_available_ = false;
  uint32_t max_session_size_ = 0;
  absl::Time last_session_empty_time_ = absl::InfinitePast();
  absl::Time last_cleanup_time_ = absl::InfinitePast();
  absl::Time last_create_session_time_ = absl::InfinitePast();

  std::unique_ptr<EngineInterface> engine_;
  std::unique_ptr<EngineBuilderInterface> engine_builder_;
  std::unique_ptr<session::SessionObserverHandler> observer_handler_;
  std::unique_ptr<user_dictionary::UserDictionarySessionHandler>
      user_dictionary_session_handler_;
  std::unique_ptr<composer::TableManager> table_manager_;
  std::unique_ptr<const commands::Request> request_;
  std::unique_ptr<const config::Config> config_;
  std::unique_ptr<keymap::KeyMapManager> key_map_manager_;

  absl::BitGen bitgen_;
};

}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_HANDLER_H_
