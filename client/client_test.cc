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

#include "client/client.h"

#include <map>
#include <string>

#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/util.h"
#include "base/version.h"
#include "ipc/ipc_mock.h"
#include "session/commands.pb.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace client {

namespace {
const char kPrecedingText[] = "preceding_text";
const char kFollowingText[] = "following_text";
const bool kSuppressSuggestion = true;

const string UpdateVersion(int diff) {
  vector<string> tokens;
  Util::SplitStringUsing(Version::GetMozcVersion(), ".", &tokens);
  EXPECT_EQ(tokens.size(), 4);
  char buf[64];
  snprintf(buf, sizeof(buf), "%d", NumberUtil::SimpleAtoi(tokens[3]) + diff);
  tokens[3] = buf;
  string output;
  Util::JoinStrings(tokens, ".", &output);
  return output;
}
}  // namespace

class TestServerLauncher : public ServerLauncherInterface {
 public:
  explicit TestServerLauncher(IPCClientFactoryMock *factory)
      : factory_(factory),
        start_server_result_(false),
        start_server_called_(false),
        force_terminate_server_result_(false),
        force_terminate_server_called_(false),
        server_protocol_version_(IPC_PROTOCOL_VERSION) {}

  virtual void Ready() {}
  virtual void Wait() {}
  virtual void Error() {}

  virtual bool StartServer(ClientInterface *client) {
    if (!response_.empty()) {
      factory_->SetMockResponse(response_);
    }
    if (!product_version_after_start_server_.empty()) {
      factory_->SetServerProductVersion(product_version_after_start_server_);
    }
    factory_->SetServerProtocolVersion(server_protocol_version_);
    start_server_called_ = true;
    return start_server_result_;
  }

  virtual bool ForceTerminateServer(const string &name) {
    force_terminate_server_called_ = true;
    return force_terminate_server_result_;
  }

  virtual bool WaitServer(uint32 pid) {
    return true;
  }

  virtual void OnFatal(ServerLauncherInterface::ServerErrorType type) {
    LOG(ERROR) << static_cast<int>(type);
    error_map_[static_cast<int>(type)]++;
  }

  int error_count(ServerLauncherInterface::ServerErrorType type) {
    return error_map_[static_cast<int>(type)];
  }

  bool start_server_called() const {
    return start_server_called_;
  }

  void set_start_server_called(bool start_server_called) {
    start_server_called_ = start_server_called;
  }

  bool force_terminate_server_called() const {
    return force_terminate_server_called_;
  }

  void set_force_terminate_server_called(bool force_terminate_server_called) {
    force_terminate_server_called_ = force_terminate_server_called;
  }

  void set_server_program(const string &server_path) {
  }

  virtual const string &server_program() const {
    static const string path;
    return path;
  }

  void set_restricted(bool restricted) {}

  void set_suppress_error_dialog(bool suppress) {}

  void set_start_server_result(const bool result) {
    start_server_result_ = result;
  }

  void set_force_terminate_server_result(const bool result) {
    force_terminate_server_result_ = result;
  }

  void set_server_protocol_version(uint32 server_protocol_version) {
    server_protocol_version_ = server_protocol_version;
  }

  void set_mock_after_start_server(const commands::Output &mock_output) {
    mock_output.SerializeToString(&response_);
  }

  void set_product_version_after_start_server(const string &version) {
    product_version_after_start_server_ = version;
  }

 private:
  IPCClientFactoryMock *factory_;
  bool start_server_result_;
  bool start_server_called_;
  bool force_terminate_server_result_;
  bool force_terminate_server_called_;
  uint32 server_protocol_version_;
  string response_;
  string product_version_after_start_server_;
  map<int, int> error_map_;
};

class ClientTest : public testing::Test {
 protected:
  ClientTest() : version_diff_(0) {}

  virtual void SetUp() {
    client_factory_.reset(new IPCClientFactoryMock);
    client_.reset(new Client);
    client_->SetIPCClientFactory(client_factory_.get());

    server_launcher_ = new TestServerLauncher(client_factory_.get());
    client_->SetServerLauncher(server_launcher_);
  }

  virtual void TearDown() {
    client_.reset();
    client_factory_.reset();
  }

  void SetMockOutput(const commands::Output &mock_output) {
    string response;
    mock_output.SerializeToString(&response);
    client_factory_->SetMockResponse(response);
  }

  void GetGeneratedInput(commands::Input *input) {
    input->ParseFromString(client_factory_->GetGeneratedRequest());
    if (input->type() != commands::Input::CREATE_SESSION) {
      ASSERT_TRUE(input->has_id());
    }
  }

  void SetupProductVersion(int version_diff) {
    version_diff_ = version_diff;
  }

  bool SetupConnection(const int id) {
    client_factory_->SetConnection(true);
    client_factory_->SetResult(true);
    if (version_diff_ == 0) {
      client_factory_->SetServerProductVersion(Version::GetMozcVersion());
    } else {
      client_factory_->SetServerProductVersion(UpdateVersion(version_diff_));
    }
    server_launcher_->set_start_server_result(true);

    // TODO(komatsu): Due to the limitation of the testing mock,
    // EnsureConnection should be explicitly called before calling
    // SendKey.  Fix the testing mock.
    commands::Output mock_output;
    mock_output.set_id(id);
    SetMockOutput(mock_output);
    return client_->EnsureConnection();
  }

  scoped_ptr<IPCClientFactoryMock> client_factory_;
  scoped_ptr<Client> client_;
  TestServerLauncher *server_launcher_;
  int version_diff_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientTest);
};

TEST_F(ClientTest, ConnectionError) {
  client_factory_->SetConnection(false);
  server_launcher_->set_start_server_result(false);
  EXPECT_FALSE(client_->EnsureConnection());

  commands::KeyEvent key;
  commands::Output output;
  EXPECT_FALSE(client_->SendKey(key, &output));

  key.Clear();
  output.Clear();
  EXPECT_FALSE(client_->TestSendKey(key, &output));

  commands::SessionCommand command;
  output.Clear();
  EXPECT_FALSE(client_->SendCommand(command, &output));
}

TEST_F(ClientTest, SendKey) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::SEND_KEY, input.type());
}

TEST_F(ClientTest, SendKeyWithContext) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Context context;
  context.set_preceding_text(kPrecedingText);
  context.set_following_text(kFollowingText);
  context.set_suppress_suggestion(kSuppressSuggestion);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKeyWithContext(key_event, context, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::SEND_KEY, input.type());
  EXPECT_EQ(kPrecedingText, input.context().preceding_text());
  EXPECT_EQ(kFollowingText, input.context().following_text());
  EXPECT_EQ(kSuppressSuggestion, input.context().suppress_suggestion());
}

TEST_F(ClientTest, TestSendKey) {
  const int mock_id = 512;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->TestSendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::TEST_SEND_KEY, input.type());
}


TEST_F(ClientTest, TestSendKeyWithContext) {
  const int mock_id = 512;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Context context;
  context.set_preceding_text(kPrecedingText);
  context.set_following_text(kFollowingText);
  context.set_suppress_suggestion(kSuppressSuggestion);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->TestSendKeyWithContext(key_event, context, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::TEST_SEND_KEY, input.type());
  EXPECT_EQ(kPrecedingText, input.context().preceding_text());
  EXPECT_EQ(kFollowingText, input.context().following_text());
  EXPECT_EQ(kSuppressSuggestion, input.context().suppress_suggestion());
}

TEST_F(ClientTest, SendCommand) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SUBMIT);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendCommand(session_command, &output));

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::SEND_COMMAND, input.type());
}

TEST_F(ClientTest, SendCommandWithContext) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::SessionCommand session_command;
  session_command.set_type(commands::SessionCommand::SUBMIT);

  commands::Context context;
  context.set_preceding_text(kPrecedingText);
  context.set_following_text(kFollowingText);
  context.set_suppress_suggestion(kSuppressSuggestion);

  commands::Output mock_output;
  mock_output.Clear();
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendCommandWithContext(session_command,
                                              context,
                                              &output));

  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_EQ(mock_id, input.id());
  EXPECT_EQ(commands::Input::SEND_COMMAND, input.type());
  EXPECT_EQ(kPrecedingText, input.context().preceding_text());
  EXPECT_EQ(kFollowingText, input.context().following_text());
  EXPECT_EQ(kSuppressSuggestion, input.context().suppress_suggestion());
}

TEST_F(ClientTest, SetConfig) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  config::Config config;
  EXPECT_TRUE(client_->SetConfig(config));
}

TEST_F(ClientTest, GetConfig) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.mutable_config()->set_verbose_level(2);
  mock_output.mutable_config()->set_incognito_mode(true);
  SetMockOutput(mock_output);

  config::Config config;
  EXPECT_TRUE(client_->GetConfig(&config));

  EXPECT_EQ(2, config.verbose_level());
  EXPECT_EQ(true, config.incognito_mode());
}

TEST_F(ClientTest, EnableCascadingWindow) {
  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  client_->NoOperation();
  commands::Input input;
  GetGeneratedInput(&input);
  EXPECT_FALSE(input.has_config());

  client_->EnableCascadingWindow(false);
  client_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  ASSERT_TRUE(input.config().has_use_cascading_window());
  EXPECT_FALSE(input.config().use_cascading_window());

  client_->EnableCascadingWindow(true);
  client_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  ASSERT_TRUE(input.config().has_use_cascading_window());
  EXPECT_TRUE(input.config().use_cascading_window());

  client_->NoOperation();
  GetGeneratedInput(&input);
  ASSERT_TRUE(input.has_config());
  EXPECT_TRUE(input.config().has_use_cascading_window());
}

TEST_F(ClientTest, VersionMismatch) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  // Suddenly, connects to a different server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION + 1);
  commands::Output output;
  EXPECT_FALSE(client_->SendKey(key_event, &output));
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count
            (ServerLauncherInterface::SERVER_VERSION_MISMATCH));
}

TEST_F(ClientTest, ProtocolUpdate) {
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_force_terminate_server_called(false);
  server_launcher_->set_force_terminate_server_result(true);
  server_launcher_->set_start_server_called(false);

  // Now connecting to an old server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // after start server, protocol version becomes the same
  server_launcher_->set_server_protocol_version(IPC_PROTOCOL_VERSION);

  EXPECT_TRUE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
  EXPECT_TRUE(server_launcher_->force_terminate_server_called());
}

TEST_F(ClientTest, ProtocolUpdateFailSameBinary) {
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_force_terminate_server_called(false);
  server_launcher_->set_force_terminate_server_result(true);
  server_launcher_->set_start_server_called(false);

  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is updated after restart the server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // even after server reboot, protocol version is old
  server_launcher_->set_server_protocol_version(IPC_PROTOCOL_VERSION - 1);
  server_launcher_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
  EXPECT_TRUE(server_launcher_->force_terminate_server_called());
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count
            (ServerLauncherInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(ClientTest, ProtocolUpdateFailOnTerminate) {
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_force_terminate_server_called(false);
  server_launcher_->set_force_terminate_server_result(false);
  server_launcher_->set_start_server_called(false);

  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is updated after restart the server
  client_factory_->SetServerProtocolVersion(IPC_PROTOCOL_VERSION - 1);
  // even after server reboot, protocol version is old
  server_launcher_->set_server_protocol_version(IPC_PROTOCOL_VERSION);
  server_launcher_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(client_->EnsureSession());
  EXPECT_FALSE(server_launcher_->start_server_called());
  EXPECT_TRUE(server_launcher_->force_terminate_server_called());
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count
            (ServerLauncherInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(ClientTest, ServerUpdate) {
  SetupProductVersion(-1);  // old version
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  LOG(ERROR) << Version::GetMozcVersion();

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_start_server_called(false);
  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is updated after restart the server
  server_launcher_->set_product_version_after_start_server(
      Version::GetMozcVersion());
  EXPECT_TRUE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
}

TEST_F(ClientTest, ServerUpdateToNewer) {
  SetupProductVersion(1);  // new version
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  LOG(ERROR) << Version::GetMozcVersion();

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());
  server_launcher_->set_start_server_called(false);
  EXPECT_TRUE(client_->EnsureSession());
  EXPECT_FALSE(server_launcher_->start_server_called());
}

TEST_F(ClientTest, ServerUpdateFail) {
  SetupProductVersion(-1);  // old
  server_launcher_->set_start_server_result(true);

  const int mock_id = 0;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->EnsureConnection());

  server_launcher_->set_start_server_called(false);
  EXPECT_FALSE(server_launcher_->start_server_called());

  // version is not updated after restart the server
  server_launcher_->set_mock_after_start_server(mock_output);
  EXPECT_FALSE(client_->EnsureSession());
  EXPECT_TRUE(server_launcher_->start_server_called());
  EXPECT_FALSE(client_->EnsureConnection());
  EXPECT_EQ(1, server_launcher_->error_count
            (ServerLauncherInterface::SERVER_BROKEN_MESSAGE));
}

TEST_F(ClientTest, TranslateProtoBufToMozcToolArgTest) {
  commands::Output output;
  string mode = "";

  // If no value is set, we expect to return false
  EXPECT_FALSE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ("", mode);

  // If NO_TOOL is set, we  expect to return false
  output.set_launch_tool_mode(commands::Output::NO_TOOL);
  EXPECT_FALSE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ("", mode);

  output.set_launch_tool_mode(commands::Output::CONFIG_DIALOG);
  EXPECT_TRUE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ("config_dialog", mode);

  output.set_launch_tool_mode(commands::Output::DICTIONARY_TOOL);
  EXPECT_TRUE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ("dictionary_tool", mode);

  output.set_launch_tool_mode(commands::Output::WORD_REGISTER_DIALOG);
  EXPECT_TRUE(client::Client::TranslateProtoBufToMozcToolArg(output, &mode));
  EXPECT_EQ("word_register_dialog", mode);
}

class SessionPlaybackTestServerLauncher : public ServerLauncherInterface {
 public:
  explicit SessionPlaybackTestServerLauncher(IPCClientFactoryMock *factory)
      : factory_(factory),
        start_server_result_(false),
        start_server_called_(false),
        force_terminate_server_result_(false),
        force_terminate_server_called_(false),
        server_protocol_version_(IPC_PROTOCOL_VERSION) {}

  virtual void Ready() {}
  virtual void Wait() {}
  virtual void Error() {}

  virtual bool StartServer(ClientInterface *client) {
    if (!response_.empty()) {
      factory_->SetMockResponse(response_);
    }
    if (!product_version_after_start_server_.empty()) {
      factory_->SetServerProductVersion(product_version_after_start_server_);
    }
    factory_->SetServerProtocolVersion(server_protocol_version_);
    start_server_called_ = true;
    return start_server_result_;
  }

  virtual bool ForceTerminateServer(const string &name) {
    force_terminate_server_called_ = true;
    return force_terminate_server_result_;
  }

  virtual bool WaitServer(uint32 pid) {
    return true;
  }

  virtual void OnFatal(ServerLauncherInterface::ServerErrorType type) {
  }

  void set_server_program(const string &server_path) {}

  void set_restricted(bool restricted) {}

  void set_suppress_error_dialog(bool suppress) {}

  void set_start_server_result(const bool result) {
    start_server_result_ = result;
  }


  virtual const string &server_program() const {
    static const string path;
    return path;
  }

 private:
  IPCClientFactoryMock *factory_;
  bool start_server_result_;
  bool start_server_called_;
  bool force_terminate_server_result_;
  bool force_terminate_server_called_;
  uint32 server_protocol_version_;
  string response_;
  string product_version_after_start_server_;
  map<int, int> error_map_;
};

class SessionPlaybackTest : public testing::Test {
 protected:
  SessionPlaybackTest() {}
  virtual ~SessionPlaybackTest() {}

  virtual void SetUp() {
    ipc_client_factory_.reset(new IPCClientFactoryMock);
    ipc_client_.reset(reinterpret_cast<IPCClientMock *>(
        ipc_client_factory_->NewClient("")));
    client_.reset(new Client);
    client_->SetIPCClientFactory(ipc_client_factory_.get());
    server_launcher_ = new SessionPlaybackTestServerLauncher(
        ipc_client_factory_.get());
    client_->SetServerLauncher(server_launcher_);
  }

  virtual void TearDown() {
    client_.reset();
    ipc_client_factory_.reset();
  }

  bool SetupConnection(const int id) {
    ipc_client_factory_->SetConnection(true);
    ipc_client_factory_->SetResult(true);
    ipc_client_factory_->SetServerProductVersion(Version::GetMozcVersion());
    server_launcher_->set_start_server_result(true);

    // TODO(komatsu): Due to the limitation of the testing mock,
    // EnsureConnection should be explicitly called before calling
    // SendKey.  Fix the testing mock.
    commands::Output mock_output;
    mock_output.set_id(id);
    SetMockOutput(mock_output);
    return client_->EnsureConnection();
  }

  void SetMockOutput(const commands::Output &mock_output) {
    string response;
    mock_output.SerializeToString(&response);
    ipc_client_factory_->SetMockResponse(response);
  }

  scoped_ptr<IPCClientFactoryMock> ipc_client_factory_;
  scoped_ptr<IPCClientMock> ipc_client_;
  scoped_ptr<Client> client_;
  SessionPlaybackTestServerLauncher *server_launcher_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionPlaybackTest);
};

// b/2797557
TEST_F(SessionPlaybackTest, PushAndResetHistoryWithNoModeTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  vector<commands::Input> history;
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(1, history.size());

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  EXPECT_FALSE(mock_output.has_mode());
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  // history should be reset.
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(0, history.size());
}

// b/2797557
TEST_F(SessionPlaybackTest, PushAndResetHistoryWithModeTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);
  key_event.set_mode(commands::HIRAGANA);
  key_event.set_activated(true);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HIRAGANA);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(commands::HIRAGANA, output.mode());

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(commands::HIRAGANA, output.mode());

  vector<commands::Input> history;
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(2, history.size());

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  client_->GetHistoryInputs(&history);
#ifdef OS_MACOSX
  // history is reset, but initializer should be added because the last mode
  // is not DIRECT.
  // TODO(team): fix b/10250883 to remove this special treatment.
  EXPECT_EQ(1, history.size());
  // Implicit IMEOn key must be added. See b/2797557 and b/>10250883.
  EXPECT_EQ(commands::Input::SEND_KEY, history[0].type());
  EXPECT_EQ(commands::KeyEvent::ON, history[0].key().special_key());
  EXPECT_EQ(commands::HIRAGANA, history[0].key().mode());
#else
  // history is reset, but initializer is not required.
  EXPECT_EQ(0, history.size());
#endif
}

// b/2797557
TEST_F(SessionPlaybackTest, PushAndResetHistoryWithDirectTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::DIRECT);
  SetMockOutput(mock_output);

  commands::Output output;
  // Send key twice
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(commands::DIRECT, output.mode());

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(commands::DIRECT, output.mode());

  vector<commands::Input> history;
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(2, history.size());

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  // history is reset, and initializer should not be added.
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(0, history.size());
}

TEST_F(SessionPlaybackTest, PlaybackHistoryTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  // On Windows, mode initializer should be added if the output contains mode.
  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  vector<commands::Input> history;
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(2, history.size());

  // Invalid id
  const int new_id = 456;
  mock_output.set_id(new_id);
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));

#ifndef DEBUG
  // PlaybackHistory and push history
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(3, history.size());
#else
  // PlaybackHistory, dump history(including reset), and add last input
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(1, history.size());
#endif
}

// b/2797557
TEST_F(SessionPlaybackTest, SetModeInitializerTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HIRAGANA);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  mock_output.set_mode(commands::DIRECT);
  SetMockOutput(mock_output);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(commands::DIRECT, output.mode());

  mock_output.set_mode(commands::FULL_KATAKANA);
  SetMockOutput(mock_output);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  EXPECT_TRUE(output.has_mode());
  EXPECT_EQ(commands::FULL_KATAKANA, output.mode());

  vector<commands::Input> history;
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(3, history.size());

  mock_output.Clear();
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  mock_output.mutable_result()->set_type(commands::Result::STRING);
  mock_output.mutable_result()->set_value("output");
  SetMockOutput(mock_output);
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());
  client_->GetHistoryInputs(&history);
#ifdef OS_MACOSX
  // history is reset, but initializer should be added.
  // TODO(team): fix b/10250883 to remove this special treatment.
  EXPECT_EQ(1, history.size());
  EXPECT_EQ(commands::Input::SEND_KEY, history[0].type());
  EXPECT_EQ(commands::KeyEvent::ON, history[0].key().special_key());
  EXPECT_EQ(commands::FULL_KATAKANA, history[0].key().mode());
#else
  // history is reset, but initializer is not required.
  EXPECT_EQ(0, history.size());
#endif
}

TEST_F(SessionPlaybackTest, ConsumedTest) {
  const int mock_id = 123;
  EXPECT_TRUE(SetupConnection(mock_id));

  commands::KeyEvent key_event;
  key_event.set_special_key(commands::KeyEvent::ENTER);

  commands::Output mock_output;
  mock_output.set_id(mock_id);
  mock_output.set_consumed(true);
  SetMockOutput(mock_output);

  commands::Output output;
  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  vector<commands::Input> history;
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(2, history.size());

  mock_output.set_consumed(false);
  SetMockOutput(mock_output);

  EXPECT_TRUE(client_->SendKey(key_event, &output));
  EXPECT_EQ(mock_output.consumed(), output.consumed());

  // Do not push unconsumed input
  client_->GetHistoryInputs(&history);
  EXPECT_EQ(2, history.size());
}
}  // namespace client
}  // namespace mozc
