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

#include "session/session_handler.h"

#include <algorithm>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "base/clock_mock.h"
#include "base/port.h"
#include "base/util.h"
#include "config/config_handler.h"
#include "converter/converter_mock.h"
#include "engine/engine_builder_interface.h"
#include "engine/engine_stub.h"
#include "engine/mock_converter_engine.h"
#include "engine/mock_data_engine_factory.h"
#include "engine/user_data_manager_mock.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/generic_storage_manager.h"
#include "session/session_handler_test_util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats_testing_util.h"

DECLARE_int32(max_session_size);
DECLARE_int32(create_session_min_interval);
DECLARE_int32(last_command_timeout);
DECLARE_int32(last_create_session_timeout);

namespace mozc {

using mozc::session::testing::CleanUp;
using mozc::session::testing::CreateSession;
using mozc::session::testing::DeleteSession;
using mozc::session::testing::IsGoodSession;
using mozc::session::testing::SessionHandlerTestBase;

namespace {

// Used to test interaction between SessionHandler and EngineBuilder in engine
// reload event.
class MockEngineBuilder : public EngineBuilderInterface {
 public:
  enum class State {
    STOP,
    RUNNING,
    RELOAD_READY,
    INVALID_DATA,
  };

  void PrepareAsync(const EngineReloadRequest &request,
                    EngineReloadResponse *response) override {
    ++num_prepare_async_called_;
    response->set_status(state_ != State::RUNNING
                         ? EngineReloadResponse::ACCEPTED
                         : EngineReloadResponse::ALREADY_RUNNING);
  }

  bool HasResponse() const override {
    return state_ == State::RELOAD_READY || state_ == State::INVALID_DATA;
  }

  void GetResponse(EngineReloadResponse *response) const override {
    switch (state_) {
      case State::RELOAD_READY:
        response->set_status(EngineReloadResponse::RELOAD_READY);
        break;
      case State::INVALID_DATA:
        response->set_status(EngineReloadResponse::DATA_BROKEN);
        break;
      default:
        response->set_status(EngineReloadResponse::UNKNOWN_ERROR);
        break;
    }
  }

  std::unique_ptr<EngineInterface> BuildFromPreparedData() override {
    ++num_build_from_prepared_data_called_;
    return std::unique_ptr<EngineInterface>(new EngineStub());
  }

  void Clear() override {
    ++num_clear_called_;
    state_ = State::STOP;
  }

  State state() const { return state_; }
  void set_state(State state) { state_ = state; }
  int num_prepare_async_called() const { return num_prepare_async_called_; }
  int num_build_from_prepared_data_called() const {
    return num_build_from_prepared_data_called_;
  }
  int num_clear_called() const { return num_clear_called_; }

 private:
  State state_ = State::STOP;
  int num_prepare_async_called_ = 0;
  int num_build_from_prepared_data_called_ = 0;
  int num_clear_called_ = 0;
};

EngineReloadResponse::Status SendDummyEngineCommand(SessionHandler *handler) {
  commands::Command command;
  command.mutable_input()->set_type(
      commands::Input::SEND_ENGINE_RELOAD_REQUEST);
  auto *request = command.mutable_input()->mutable_engine_reload_request();
  request->set_engine_type(EngineReloadRequest::MOBILE);
  request->set_file_path("dummy");  // Dummy is OK for MockEngineBuilder.
  handler->EvalCommand(&command);
  return command.output().engine_reload_response().status();
}

}  // namespace

class SessionHandlerTest : public SessionHandlerTestBase {
 protected:
  void SetUp() override {
    SessionHandlerTestBase::SetUp();
    Clock::SetClockForUnitTest(nullptr);
    GenericStorageManagerFactory::SetGenericStorageManager(nullptr);
  }

  void TearDown() override {
    GenericStorageManagerFactory::SetGenericStorageManager(nullptr);
    Clock::SetClockForUnitTest(nullptr);
    SessionHandlerTestBase::TearDown();
  }

  static std::unique_ptr<Engine> CreateMockDataEngine() {
    return std::unique_ptr<Engine>(MockDataEngineFactory::Create());
  }
};

TEST_F(SessionHandlerTest, MaxSessionSizeTest) {
  uint32 expected_session_created_num = 0;
  const int32 interval_time = FLAGS_create_session_min_interval = 10;  // 10 sec
  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);

  // The oldest item is remvoed
  const size_t session_size = 3;
  FLAGS_max_session_size = static_cast<int32>(session_size);
  {
    SessionHandler handler(CreateMockDataEngine());

    // Create session_size + 1 sessions
    std::vector<uint64> ids;
    for (size_t i = 0; i <= session_size; ++i) {
      uint64 id = 0;
      EXPECT_TRUE(CreateSession(&handler, &id));
      ++expected_session_created_num;
      EXPECT_COUNT_STATS("SessionCreated", expected_session_created_num);
      ids.push_back(id);
      clock.PutClockForward(interval_time, 0);
    }

    for (int i = static_cast<int>(ids.size() - 1); i >= 0; --i) {
      if (i > 0) {   // this id is alive
        EXPECT_TRUE(IsGoodSession(&handler, ids[i]));
      } else {  // the first id shuold be removed
        EXPECT_FALSE(IsGoodSession(&handler, ids[i]));
      }
    }
  }

  FLAGS_max_session_size = static_cast<int32>(session_size);
  {
    SessionHandler handler(CreateMockDataEngine());

    // Create session_size sessions
    std::vector<uint64> ids;
    for (size_t i = 0; i < session_size; ++i) {
      uint64 id = 0;
      EXPECT_TRUE(CreateSession(&handler, &id));
      ++expected_session_created_num;
      EXPECT_COUNT_STATS("SessionCreated", expected_session_created_num);
      ids.push_back(id);
      clock.PutClockForward(interval_time, 0);
    }

    std::random_device rd;
    std::mt19937 urbg(rd());
    std::shuffle(ids.begin(), ids.end(), urbg);
    const uint64 oldest_id = ids[0];
    for (size_t i = 0; i < session_size; ++i) {
      EXPECT_TRUE(IsGoodSession(&handler, ids[i]));
    }

    // Create new session
    uint64 id = 0;
    EXPECT_TRUE(CreateSession(&handler, &id));
    ++expected_session_created_num;
    EXPECT_COUNT_STATS("SessionCreated", expected_session_created_num);

    // the oldest id no longer exists
    EXPECT_FALSE(IsGoodSession(&handler, oldest_id));
  }
}

TEST_F(SessionHandlerTest, CreateSessionMinInterval) {
  const int32 interval_time = FLAGS_create_session_min_interval = 10;  // 10 sec
  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64 id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));
  EXPECT_FALSE(CreateSession(&handler, &id));

  clock.PutClockForward(interval_time - 1, 0);
  EXPECT_FALSE(CreateSession(&handler, &id));

  clock.PutClockForward(1, 0);
  EXPECT_TRUE(CreateSession(&handler, &id));
}

TEST_F(SessionHandlerTest, LastCreateSessionTimeout) {
  const int32 timeout = FLAGS_last_create_session_timeout = 10;  // 10 sec
  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64 id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  clock.PutClockForward(timeout, 0);
  EXPECT_TRUE(CleanUp(&handler, id));

  // the session is removed by server
  EXPECT_FALSE(IsGoodSession(&handler, id));
}

TEST_F(SessionHandlerTest, LastCommandTimeout) {
  const int32 timeout = FLAGS_last_command_timeout = 10;  // 10 sec
  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);

  SessionHandler handler(CreateMockDataEngine());

  uint64 id = 0;
  EXPECT_TRUE(CreateSession(&handler, &id));

  EXPECT_TRUE(CleanUp(&handler, id));
  EXPECT_TRUE(IsGoodSession(&handler, id));

  clock.PutClockForward(timeout, 0);
  EXPECT_TRUE(CleanUp(&handler, id));
  EXPECT_FALSE(IsGoodSession(&handler, id));
}

TEST_F(SessionHandlerTest, ShutdownTest) {
  SessionHandler handler(CreateMockDataEngine());

  uint64 session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SHUTDOWN);
    // EvalCommand returns false since the session no longer exists.
    EXPECT_FALSE(handler.EvalCommand(&command));
    EXPECT_EQ(session_id, command.output().id());
  }

  {  // Any command should be rejected after shutdown.
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::NO_OPERATION);
    EXPECT_FALSE(handler.EvalCommand(&command));
  }

  EXPECT_COUNT_STATS("ShutDown", 1);
  // CreateSession and Shutdown.
  EXPECT_COUNT_STATS("SessionAllEvent", 2);
}

TEST_F(SessionHandlerTest, ClearHistoryTest) {
  SessionHandler handler(CreateMockDataEngine());

  uint64 session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::CLEAR_USER_HISTORY);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(session_id, command.output().id());
    EXPECT_COUNT_STATS("ClearUserHistory", 1);
  }

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::CLEAR_USER_PREDICTION);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(session_id, command.output().id());
    EXPECT_COUNT_STATS("ClearUserPrediction", 1);
  }

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::CLEAR_UNUSED_USER_PREDICTION);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(session_id, command.output().id());
    EXPECT_COUNT_STATS("ClearUnusedUserPrediction", 1);
  }

  // CreateSession and Clear{History|UserPrediction|UnusedUserPrediction}.
  EXPECT_COUNT_STATS("SessionAllEvent", 4);
}

TEST_F(SessionHandlerTest, ElapsedTimeTest) {
  SessionHandler handler(CreateMockDataEngine());

  uint64 id = 0;

  ClockMock clock(1000, 0);
  Clock::SetClockForUnitTest(&clock);
  EXPECT_TRUE(CreateSession(&handler, &id));
  EXPECT_TIMING_STATS("ElapsedTimeUSec", 0, 1, 0, 0);
}

TEST_F(SessionHandlerTest, ConfigTest) {
  config::Config config;
  config::ConfigHandler::GetStoredConfig(&config);
  config.set_incognito_mode(false);
  config::ConfigHandler::SetConfig(config);

  SessionHandler handler(CreateMockDataEngine());

  uint64 session_id = 0;
  EXPECT_TRUE(CreateSession(&handler, &session_id));

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::GET_CONFIG);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.input().id(), command.output().id());
    EXPECT_FALSE(command.output().config().incognito_mode());
  }

  {
    commands::Command command;
    commands::Input *input = command.mutable_input();
    input->set_id(session_id);
    input->set_type(commands::Input::SET_CONFIG);
    config.set_incognito_mode(true);
    input->mutable_config()->CopyFrom(config);
    EXPECT_TRUE(handler.EvalCommand(&command));
    EXPECT_EQ(command.input().id(), command.output().id());
    EXPECT_TRUE(command.output().config().incognito_mode());
    config::ConfigHandler::GetStoredConfig(&config);
    EXPECT_TRUE(config.incognito_mode());
  }

  EXPECT_COUNT_STATS("SetConfig", 1);
  // CreateSession, GetConfig and SetConfig.
  EXPECT_COUNT_STATS("SessionAllEvent", 3);
}

TEST_F(SessionHandlerTest, VerifySyncIsCalled) {
  // Tests if sync is called for the following input commands.
  commands::Input::CommandType command_types[] = {
    commands::Input::DELETE_SESSION,
    commands::Input::CLEANUP,
  };
  for (size_t i = 0; i < arraysize(command_types); ++i) {
    std::unique_ptr<MockConverterEngine> engine(new MockConverterEngine());

    // Set the mock user data manager to the converter mock created above. This
    // user_data_manager_mock is owned by the converter mock inside the engine
    // instance.
    UserDataManagerMock *user_data_mgr_mock = new UserDataManagerMock();
    engine->SetUserDataManager(user_data_mgr_mock);

    // Set up a session handler and a input command.
    SessionHandler handler(std::move(engine));
    commands::Command command;
    command.mutable_input()->set_id(0);
    command.mutable_input()->set_type(command_types[i]);

    // Check if Sync() is called after evaluating the command.
    EXPECT_EQ(0, user_data_mgr_mock->GetFunctionCallCount("Sync"));
    handler.EvalCommand(&command);
    EXPECT_EQ(1, user_data_mgr_mock->GetFunctionCallCount("Sync"));
  }
}

const char *kStorageTestData[] = {
  "angel", "bishop", "chariot", "dragon",
};

class MockStorage : public GenericStorageInterface {
 public:
  int insert_count;
  int clear_count;
  const char **insert_expect;

  MockStorage() : insert_count(0), clear_count(0) {}
  virtual ~MockStorage() {}

  virtual bool Insert(const string &key, const char *value) {
    EXPECT_EQ(string(insert_expect[insert_count]), key);
    EXPECT_EQ(string(insert_expect[insert_count]), string(value));
    ++insert_count;
    return true;
  }

  virtual const char *Lookup(const string &key) {
    return NULL;
  }

  virtual bool GetAllValues(std::vector<string> *values) {
    values->clear();
    for (size_t i = 0; i < arraysize(kStorageTestData); ++i) {
      values->push_back(kStorageTestData[i]);
    }
    return true;
  }

  virtual bool Clear() {
    ++clear_count;
    return true;
  }

  void SetInsertExpect(const char **expect) {
    insert_expect = expect;
  }
};

class MockStorageManager : public GenericStorageManagerInterface {
 public:
  virtual GenericStorageInterface *GetStorage(
     commands::GenericStorageEntry::StorageType storage_type) {
    return storage;
  }

  void SetStorage(MockStorage *newStorage) {
    storage = newStorage;
  }

 private:
  MockStorage *storage;
};

// Tests basic behavior of InsertToStorage and ReadAllFromStorage methods.
TEST_F(SessionHandlerTest, StorageTest) {
  // Inject mock objects.
  MockStorageManager storageManager;
  GenericStorageManagerFactory::SetGenericStorageManager(&storageManager);
  SessionHandler handler(CreateMockDataEngine());
  {
    // InsertToStorage
    MockStorage mock_storage;
    mock_storage.SetInsertExpect(kStorageTestData);
    storageManager.SetStorage(&mock_storage);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::INSERT_TO_STORAGE);
    commands::GenericStorageEntry *storage_entry =
        command.mutable_input()->mutable_storage_entry();
    storage_entry->set_type(commands::GenericStorageEntry::SYMBOL_HISTORY);
    storage_entry->mutable_key()->assign("dummy key");
    for (size_t i = 0; i < arraysize(kStorageTestData); ++i) {
      storage_entry->mutable_value()->Add()->assign(kStorageTestData[i]);
    }
    EXPECT_TRUE(handler.InsertToStorage(&command));
    EXPECT_EQ(arraysize(kStorageTestData), mock_storage.insert_count);
  }
  {
    // ReadAllFromStorage
    MockStorage mock_storage;
    storageManager.SetStorage(&mock_storage);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::READ_ALL_FROM_STORAGE);
    commands::GenericStorageEntry *storage_entry =
        command.mutable_input()->mutable_storage_entry();
    storage_entry->set_type(commands::GenericStorageEntry::EMOTICON_HISTORY);
    EXPECT_TRUE(handler.ReadAllFromStorage(&command));
    EXPECT_EQ(
        commands::GenericStorageEntry::EMOTICON_HISTORY,
        command.output().storage_entry().type());
    EXPECT_EQ(
        arraysize(kStorageTestData),
        command.output().storage_entry().value().size());
  }
  {
    // Clear
    MockStorage mock_storage;
    storageManager.SetStorage(&mock_storage);
    commands::Command command;
    command.mutable_input()->set_type(commands::Input::CLEAR_STORAGE);
    commands::GenericStorageEntry *storage_entry =
        command.mutable_input()->mutable_storage_entry();
    storage_entry->set_type(commands::GenericStorageEntry::EMOTICON_HISTORY);
    EXPECT_TRUE(handler.ClearStorage(&command));
    EXPECT_EQ(
        commands::GenericStorageEntry::EMOTICON_HISTORY,
        command.output().storage_entry().type());
    EXPECT_EQ(1, mock_storage.clear_count);
  }
}

TEST_F(SessionHandlerTest, EmojiUsageStatsTest) {
  SessionHandler handler(CreateMockDataEngine());

  commands::Command command;
  command.mutable_input()->set_type(commands::Input::INSERT_TO_STORAGE);
  commands::GenericStorageEntry *storage_entry =
      command.mutable_input()->mutable_storage_entry();
  storage_entry->set_type(commands::GenericStorageEntry::EMOJI_HISTORY);
  storage_entry->mutable_key()->assign("dummy key");

  // Carrier emoji "BLACK SUN WITH RAYS"
  storage_entry->mutable_value()->Clear();
  storage_entry->mutable_value()->Add()->assign("\xF3\xBE\x80\x80");
  EXPECT_TRUE(handler.EvalCommand(&command));
  EXPECT_COUNT_STATS("CommitCarrierEmoji", 1);
  EXPECT_COUNT_STATS("CommitUnicodeEmoji", 0);

  storage_entry->mutable_value()->Clear();
  // Carrier emoji "BLACK SUN WITH RAYS"
  storage_entry->mutable_value()->Add()->assign("\xF3\xBE\x80\x80");
  // Carrier emoji "GOOGLE"
  storage_entry->mutable_value()->Add()->assign("\xF3\xBE\xBA\xA0");
  // Unicode emoji "BLACK SUN WITH RAYS"
  storage_entry->mutable_value()->Add()->assign("☀");
  // Unicode emoji "RABBIT FACE"
  storage_entry->mutable_value()->Add()->assign("🐰");
  EXPECT_TRUE(handler.EvalCommand(&command));
  EXPECT_COUNT_STATS("CommitCarrierEmoji", 3);
  EXPECT_COUNT_STATS("CommitUnicodeEmoji", 2);
}

// Tests the interaction with EngineBuilderInterface for successful Engine
// reload event.
TEST_F(SessionHandlerTest, EngineReload_SuccessfulScenario) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(
      std::unique_ptr<EngineStub>(new EngineStub()),
      std::unique_ptr<MockEngineBuilder>(engine_builder));

  // Session handler receives reload request when engine builder is not running.
  // EngineBuilderInterface::PrepareAsync() should be called once.
  engine_builder->set_state(MockEngineBuilder::State::STOP);
  ASSERT_EQ(EngineReloadResponse::ACCEPTED, SendDummyEngineCommand(&handler));
  EXPECT_EQ(1, engine_builder->num_prepare_async_called());

  // Emulate the state after successful data load.
  engine_builder->set_state(MockEngineBuilder::State::RELOAD_READY);

  // A new engine should be built on create session event because the session
  // handler currently holds no session.
  uint64 id = 0;
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(1, engine_builder->num_build_from_prepared_data_called());
  EXPECT_EQ(1, engine_builder->num_clear_called());
}

// Tests the interaction with EngineBuilderInterface in the situation where
// async data load is already running.
TEST_F(SessionHandlerTest, EngineReload_AlreadyRunning) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(
      std::unique_ptr<EngineStub>(new EngineStub()),
      std::unique_ptr<MockEngineBuilder>(engine_builder));

  // Emulate the state where async data load is running.
  engine_builder->set_state(MockEngineBuilder::State::RUNNING);

  // Session handler receives reload request when engine builder is running.
  ASSERT_EQ(EngineReloadResponse::ALREADY_RUNNING,
            SendDummyEngineCommand(&handler));
  EXPECT_EQ(1, engine_builder->num_prepare_async_called());

  // BuildFromPreparedData() shouldn't be called on create session event when
  // async data load is running.
  uint64 id = 0;
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(0, engine_builder->num_build_from_prepared_data_called());
  EXPECT_EQ(0, engine_builder->num_clear_called());
}

// Tests the interaction with EngineBuilderInterface in the situation where
// requested data is broken.
TEST_F(SessionHandlerTest, EngineReload_InvalidData) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(
      std::unique_ptr<EngineStub>(new EngineStub()),
      std::unique_ptr<MockEngineBuilder>(engine_builder));

  // Emulate the state where requested data is broken.
  engine_builder->set_state(MockEngineBuilder::State::INVALID_DATA);

  // A new engine is not built but the builder should be cleared for next reload
  // request.
  uint64 id = 0;
  ASSERT_TRUE(CreateSession(&handler, &id));
  EXPECT_EQ(0, engine_builder->num_build_from_prepared_data_called());
  EXPECT_EQ(1, engine_builder->num_clear_called());
}

// Tests the interaction with EngineBuilderInterface in the situation where
// sessions exist in create session event.
TEST_F(SessionHandlerTest, EngineReload_SessionExists) {
  MockEngineBuilder *engine_builder = new MockEngineBuilder();
  SessionHandler handler(
      std::unique_ptr<EngineStub>(new EngineStub()),
      std::unique_ptr<MockEngineBuilder>(engine_builder));

  // A session is created before data is loaded.
  engine_builder->set_state(MockEngineBuilder::State::STOP);
  uint64 id1 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id1));
  EXPECT_EQ(0, engine_builder->num_build_from_prepared_data_called());
  EXPECT_EQ(0, engine_builder->num_clear_called());

  // Emulate the state where async data load is complete.
  engine_builder->set_state(MockEngineBuilder::State::RELOAD_READY);

  // Another session is created.  Since the handler already holds one session
  // (id1), engine reload should not happen.
  uint64 id2 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id2));
  EXPECT_EQ(0, engine_builder->num_build_from_prepared_data_called());
  EXPECT_EQ(0, engine_builder->num_clear_called());

  // All the sessions were deleted.
  ASSERT_TRUE(DeleteSession(&handler, id1));
  ASSERT_TRUE(DeleteSession(&handler, id2));

  // A new session is created.  Since the handler holds no session, engine is
  // reloaded at this timing.
  uint64 id3 = 0;
  ASSERT_TRUE(CreateSession(&handler, &id3));
  EXPECT_EQ(1, engine_builder->num_build_from_prepared_data_called());
  EXPECT_EQ(1, engine_builder->num_clear_called());
}

}  // namespace mozc
