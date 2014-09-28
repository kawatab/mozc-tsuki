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

#include <windows.h>
#include <ime.h>
#include <msctf.h>

#include <string>
#include <memory>

#include "base/logging.h"
#include "base/system_util.h"
#include "base/version.h"
#include "client/client_interface.h"
#include "config/config_handler.h"
#include "ipc/ipc_mock.h"
#include "session/commands.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"
#include "win32/base/keyevent_handler.h"

namespace mozc {
namespace win32 {
namespace {

using commands::Context;
using commands::Output;
using std::unique_ptr;

class TestableKeyEventHandler : public KeyEventHandler {
 public:
  // Change access rights
  using KeyEventHandler::HandleKey;
  using KeyEventHandler::ConvertToKeyEvent;
  using KeyEventHandler::UnlockKanaLock;
  using KeyEventHandler::MaybeSpawnTool;
};

const BYTE kPressed = 0x80;
const BYTE kToggled = 0x01;
LPARAM CreateLParam(uint16 repeat_count,
                    uint8 scan_code,
                    bool is_extended_key,
                    bool has_context_code,
                    bool is_previous_state_down,
                    bool is_in_transition_state) {
  DWORD value = 0;
  value |= repeat_count;
  value |= (static_cast<DWORD>(scan_code) << 16);
  value |= (is_extended_key ? (1 << 24) : 0);
  value |= (has_context_code ? (1 << 29) : 0);
  value |= (is_previous_state_down ? (1 << 30) : 0);
  value |= (is_in_transition_state ? (1 << 31) : 0);
  const LPARAM param = static_cast<LPARAM>(value);
#if defined(_M_X64)
  // In x64 environment, upper DWORD will be filled with 0.
  EXPECT_EQ(0, param & 0xffffffff00000000);
#endif  // _M_X64
  return param;
}

class TestServerLauncher : public client::ServerLauncherInterface {
 public:
  explicit TestServerLauncher(IPCClientFactoryMock *factory)
      : factory_(factory),
        start_server_result_(false),
        start_server_called_(false),
        server_protocol_version_(IPC_PROTOCOL_VERSION) {}

  virtual void Ready() {}
  virtual void Wait() {}
  virtual void Error() {}

  virtual bool StartServer(client::ClientInterface *client) {
    if (!response_.empty()) {
      factory_->SetMockResponse(response_);
    }
    factory_->SetServerProtocolVersion(server_protocol_version_);
    start_server_called_ = true;
    return start_server_result_;
  }

  virtual bool ForceTerminateServer(const string &name) {
    return true;
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

  virtual void set_restricted(bool restricted) {}

  virtual void set_suppress_error_dialog(bool suppress) {}

  virtual void set_server_program(const string &server_path) {}

  virtual const string &server_program() const {
    static const string path;
    return path;
  }

  void set_start_server_result(const bool result) {
    start_server_result_ = result;
  }

  void set_server_protocol_version(uint32 server_protocol_version) {
    server_protocol_version_ = server_protocol_version;
  }

  uint32 set_server_protocol_version() const {
    return server_protocol_version_;
  }

  void set_mock_after_start_server(const Output &mock_output) {
    mock_output.SerializeToString(&response_);
  }

 private:
  IPCClientFactoryMock *factory_;
  bool start_server_result_;
  bool start_server_called_;
  uint32 server_protocol_version_;
  string response_;
  map<int, int> error_map_;
};

class KeyboardMock : public Win32KeyboardInterface {
 public:
  explicit KeyboardMock(bool initial_kana_lock_state) {
    if (initial_kana_lock_state) {
      key_state_.SetState(VK_KANA, kPressed);
    }
  }
  bool kana_locked() const {
    return ((key_state_.GetState(VK_KANA) & kPressed) == kPressed);
  }
  virtual bool IsKanaLocked(const KeyboardStatus & /*keyboard_state*/) {
    return kana_locked();
  }
  virtual bool SetKeyboardState(const KeyboardStatus &keyboard_state) {
    key_state_ = keyboard_state;
    return true;
  }
  virtual bool GetKeyboardState(KeyboardStatus *keyboard_state) {
    *keyboard_state = key_state_;
    return true;
  }
  virtual bool AsyncIsKeyPressed(int virtual_key) {
    return key_state_.IsPressed(virtual_key);
  }
  virtual int ToUnicode(UINT wVirtKey, UINT wScanCode,
      const BYTE *lpKeyState, LPWSTR pwszBuff, int cchBuff, UINT wFlags) {
    // We use a mock class in case the Japanese keyboard layout is not
    // available on this system.  This emulator class should work well in most
    // cases.  It returns an unicode character (if any) as if Japanese keyboard
    // layout was currently active.
    return JapaneseKeyboardLayoutEmulator::ToUnicode(
        wVirtKey, wScanCode, lpKeyState, pwszBuff, cchBuff, wFlags);
  }
  virtual UINT SendInput(const vector<INPUT> &input) {
    // Not implemented.
    return 0;
  }

 private:
  KeyboardStatus key_state_;
  DISALLOW_COPY_AND_ASSIGN(KeyboardMock);
};

class MockState {
 public:
  MockState()
      : client_(nullptr),
        launcher_(nullptr) {}
  explicit MockState(const Output &mock_response)
      : client_(client::ClientFactory::NewClient()),
        launcher_(nullptr) {
    client_factory_.SetConnection(true);
    client_factory_.SetResult(true);
    client_factory_.SetServerProductVersion(Version::GetMozcVersion());
    client_factory_.SetMockResponse(mock_response.SerializeAsString());
    client_->SetIPCClientFactory(&client_factory_);

    // |launcher| will be deleted by the |client|
    launcher_ = new TestServerLauncher(&client_factory_);
    client_->SetServerLauncher(launcher_);
    launcher_->set_start_server_result(true);
  }

  client::ClientInterface * mutable_client() {
    return client_.get();
  }

  bool GetGeneratedRequest(mozc::commands::Input *input) {
    return input->ParseFromString(client_factory_.GetGeneratedRequest());
  }

  bool start_server_called() {
    return launcher_->start_server_called();
  }

 private:
  IPCClientFactoryMock client_factory_;
  unique_ptr<client::ClientInterface> client_;
  TestServerLauncher *launcher_;
  DISALLOW_COPY_AND_ASSIGN(MockState);
};

}  // namespace

class KeyEventHandlerTest : public testing::Test {
 protected:
  KeyEventHandlerTest() {}
  virtual ~KeyEventHandlerTest() {}
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    mozc::config::ConfigHandler::GetDefaultConfig(&default_config_);
    mozc::config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    mozc::config::ConfigHandler::SetConfig(default_config_);
  }

  vector<KeyInformation> GetDefaultDirectModeKeys() const {
    return KeyInfoUtil::ExtractSortedDirectModeKeys(default_config_);
  }

  vector<KeyInformation> GetDirectModeKeysCtrlJToEnableIME() const {
    config::Config config;
    config.CopyFrom(default_config_);

    const char custom_keymap_table[] =
        "status\tkey\tcommand\n"
        "DirectInput\tCtrl j\tIMEOn\n";

    config.set_session_keymap(mozc::config::Config::CUSTOM);
    config.set_custom_keymap_table(custom_keymap_table);

    return KeyInfoUtil::ExtractSortedDirectModeKeys(config);
  }

  vector<KeyInformation> GetDirectModeKeysCtrlBackslashToEnableIME() const {
    config::Config config;
    config.CopyFrom(default_config_);

    const char custom_keymap_table[] =
        "status\tkey\tcommand\n"
        "DirectInput\tCtrl \\\tIMEOn\n";

    config.set_session_keymap(mozc::config::Config::CUSTOM);
    config.set_custom_keymap_table(custom_keymap_table);

    return KeyInfoUtil::ExtractSortedDirectModeKeys(config);
  }

  mozc::config::Config default_config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyEventHandlerTest);
};

TEST_F(KeyEventHandlerTest, HankakuZenkakuTest) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  Output output;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // "Hankaku/Zenkaku"
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_DBE_DBCSCHAR, kPressed);

    const VirtualKey virtual_key =
        VirtualKey::FromVirtualKey(VK_DBE_DBCSCHAR);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.open = false;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;

    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard,
        &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_FALSE(actual_input.key().activated());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::HANKAKU, actual_input.key().special_key());
  }
}

TEST_F(KeyEventHandlerTest, ClearKanaLockInAlphanumericMode) {
  // Call UnlockKanaLockIfNeeded just after the IME starts to handle key event
  // because there might be no chance to unlock an unexpected Kana-Lock except
  // for the key event handler in some tricky cases.

  const bool kKanaLocked = true;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  EXPECT_TRUE(keyboard.kana_locked());

  InputState next_state;
  Output output;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // "Escape"
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_ESCAPE, kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_ESCAPE);
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x01,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x00010001, lparam.lparam());

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.open = false;

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_FALSE(result.should_be_eaten);
    EXPECT_FALSE(result.should_be_sent_to_server);
    EXPECT_FALSE(next_state.open);
    EXPECT_FALSE(mock.start_server_called());
    EXPECT_FALSE(keyboard.kana_locked());
  }
}

TEST_F(KeyEventHandlerTest, ClearKanaLockEvenWhenIMEIsDisabled) {
  // Even in the safe mode such as logon screen, it would be better to clear
  // kana-lock in some cases.  This helps users to input their password as
  // expected except that they used half-width katakana for their password.

  const bool kKanaLocked = true;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);
  EXPECT_TRUE(keyboard.kana_locked());

  InputState next_state;
  Output output;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = true;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // "A"
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('A', kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('A');
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x1e,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x1e0001, lparam.lparam());

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.open = false;

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_FALSE(result.should_be_eaten);
    EXPECT_FALSE(result.should_be_sent_to_server);
    EXPECT_FALSE(next_state.open);
    EXPECT_FALSE(mock.start_server_called());
    EXPECT_FALSE(keyboard.kana_locked());
  }
}

TEST_F(KeyEventHandlerTest, CustomActivationKeyTest) {
  // We might want to allow users to use their preferred key combinations
  // to open/close IME.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  // Use Ctrl+J to turn on IME.
  behavior.direct_mode_keys = GetDirectModeKeysCtrlJToEnableIME();

  Context context;

  // Ctrl+J
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('J');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('J', kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = false;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('j', actual_input.key().key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_FALSE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

// A user can assign CTRL+\ to enable IME.  See b/3033135 for details.
TEST_F(KeyEventHandlerTest, Issue3033135_VK_OEM_102) {
  // We might want to allow users to use their preferred key combinations
  // to open/close IME.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDirectModeKeysCtrlBackslashToEnableIME();

  Context context;

  // Ctrl+\ (VK_OEM_102; Backslash in 106/109 Japanese Keyboard)
  {
    const VirtualKey virtual_key =
        VirtualKey::FromVirtualKey(VK_OEM_102);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_OEM_102, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = false;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('\\', actual_input.key().key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_FALSE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

// A user can assign CTRL+\ to enable IME.  See b/3033135 for details.
TEST_F(KeyEventHandlerTest, Issue3033135_VK_OEM_5) {
  // We might want to allow users to use their preferred key combinations
  // to open/close IME.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDirectModeKeysCtrlBackslashToEnableIME();

  Context context;

  // Ctrl+\ (VK_OEM_5; Yen in 106/109 Japanese Keyboard)
  {
    const VirtualKey virtual_key =
        VirtualKey::FromVirtualKey(VK_OEM_5);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_OEM_5, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = false;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('\\', actual_input.key().key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_FALSE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCtrlH) {
  // When a user presses an alphabet key and a control key, keyboard-layout
  // drivers produce a control code (0x01,...,0x20), to which the session
  // server assigns its own code.  To avoid conflicts between a control code
  // and one internally-used by the session server, we should decompose a
  // control code into a tuple of an ASCII alphabet and a modifier key.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Ctrl+H should be sent to the server as 'h' + |KeyEvent::CTRL|.
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('H');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('H', kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('h', actual_input.key().key_code());  // must be non-capitalized.
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCtrlShiftH) {
  // This is an exception of a key handling rule of the Windows client where
  // VK_SHIFT and VK_CONTROL are pressed.  The Windows client expects the server
  // never eats a key when Control and Shift is pressed except that the key is
  // VK_A, ..., or, VK_Z, or other special keys defined in Mozc protocol such as
  // backspace or space.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Ctrl+Shift+H should be sent to the server as
  // 'h' + |KeyEvent::CTRL| + |KeyEvent::Shift|.
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('H');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('H', kPressed);
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('h', actual_input.key().key_code());  // must be non-capitalized.
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(2, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_EQ(commands::KeyEvent::SHIFT, actual_input.key().modifier_keys(1));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCapsH) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // [CapsLock] h should be sent to the server as 'H' + |KeyEvent::Caps|.
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('H');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('H', kPressed);
    keyboard_status.SetState(VK_CAPITAL, kToggled);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('H', actual_input.key().key_code());  // must be capitalized.
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CAPS, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCapsShiftH) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // [CapsLock] Shift+H should be sent to the server as
  // 'h' + |KeyEvent::Caps|.
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('H');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('H', kPressed);
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_CAPITAL, kToggled);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('h', actual_input.key().key_code());  // must be non-capitalized.
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CAPS, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCapsCtrlH) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // [CapsLock] Ctrl+H should be sent to the server as
  // 'H' + |KeyEvent::CTRL| + |KeyEvent::Caps|.
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('H');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('H', kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);
    keyboard_status.SetState(VK_CAPITAL, kToggled);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('H', actual_input.key().key_code());  // must be capitalized.
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(2, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_EQ(commands::KeyEvent::CAPS, actual_input.key().modifier_keys(1));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCapsShiftCtrlH) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // [CapsLock] Ctrl+Shift+H should be sent to the server as
  // 'h' + |KeyEvent::CTRL| + |KeyEvent::Shift| + |KeyEvent::Caps|.
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('H');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('H', kPressed);
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);
    keyboard_status.SetState(VK_CAPITAL, kToggled);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
      initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('h', actual_input.key().key_code());  // must be non-capitalized.
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(3, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_EQ(commands::KeyEvent::SHIFT, actual_input.key().modifier_keys(1));
    EXPECT_EQ(commands::KeyEvent::CAPS, actual_input.key().modifier_keys(2));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCtrlHat) {
  // When a user presses some keys with control key, keyboard-layout
  // drivers may not produce any character but the server expects a key event.
  // For example, suppose that the Mozc keybindings includes Ctrl+^.
  // On 106/109 Japanese keyboard, you can actually use this key combination
  // as VK_OEM_7 + VK_CONTROL.  On 101/104 English keyboard, however,
  // should we interpret VK_6 + VK_SHIFT + VK_CONTROL as Ctrl+^ ?
  // As a temporal solution to be consistent with the GUI tool, the Windows
  // client expects the server never eats a key when Control and Shift is
  // pressed except that the key is VK_A, ..., or, VK_Z, or other special keys
  // defined in Mozc protocol such as backspace or space.
  // TODO(komatsu): Clarify the expected algorithm for the client.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Ctrl+^ should be sent to the server as '^' + |KeyEvent::CTRL|.
  {
    // '^' on 106/109 Japanese keyboard.
    const VirtualKey virtual_key =
        VirtualKey::FromVirtualKey(VK_OEM_7);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_OEM_7, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('^', actual_input.key().key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCtrlShift7) {
  // As commented in KeyEventHandlerTest::HandleCtrlHat, the Windows
  // client expects the server never eats a key when Control and Shift is
  // pressed except that the key is VK_A, ..., or, VK_Z, or other special keys
  // defined in Mozc protocol such as backspace or space, which means that
  // VK_7 + VK_SHIFT + VK_CONTROL on 106/109 Japanese keyboard will not be
  // sent to the server as Ctrl+'\'' nor Ctrl+Shift+'7' even though
  // Ctrl+'\'' is available on 101/104 English keyboard.
  // TODO(komatsu): Clarify the expected algorithm for the client.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(false);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // VK_7 + VK_SHIFT + VK_CONTROL must not be sent to the server as
  // '\'' + |KeyEvent::CTRL| nor '7' + |KeyEvent::CTRL| + |KeyEvent::SHIFT|.
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('7');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('7', kPressed);
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_FALSE(result.should_be_eaten);
    EXPECT_FALSE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_FALSE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
}

TEST_F(KeyEventHandlerTest, HandleCtrlShiftSpace) {
  // This is an exception of a key handling rule of the Windows client where
  // VK_SHIFT and VK_CONTROL are pressed.  The Windows client expects the
  // server may eat a special key when Control and Shift is pressed.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // VK_SPACE + VK_SHIFT + VK_CONTROL must be sent to the server as
  // |KeyEvent::SPACE| + |KeyEvent::CTRL| + |KeyEvent::SHIFT|
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_SPACE);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SPACE, kPressed);
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(2, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_EQ(commands::KeyEvent::SHIFT, actual_input.key().modifier_keys(1));
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::SPACE, actual_input.key().special_key());
  }
}

TEST_F(KeyEventHandlerTest, HandleCtrlShiftBackspace) {
  // This is an exception of a key handling rule of the Windows client where
  // VK_SHIFT and VK_CONTROL are pressed.  The Windows client expects the
  // server may eat a special key when Control and Shift is pressed.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // VK_BACK + VK_SHIFT + VK_CONTROL must be sent to the server as
  // |KeyEvent::BACKSPACE| + |KeyEvent::CTRL| + |KeyEvent::SHIFT|
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_BACK);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_BACK, kPressed);
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_CONTROL, kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(2, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_EQ(commands::KeyEvent::SHIFT, actual_input.key().modifier_keys(1));
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::BACKSPACE, actual_input.key().special_key());
  }
}

TEST_F(KeyEventHandlerTest, Issue2903247_KeyUpShouldNotBeEaten) {
  // In general, key up event should not be eaten by the IME.
  // See b/2903247 for details.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Release 'F6'
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_F6, kPressed);

    const VirtualKey last_keydown_virtual_key =
        VirtualKey::FromVirtualKey(VK_F6);
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_F6);
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,  // repeat_count
        0x40,    // scan_code
        false,   // is_extended_key,
        false,   // has_context_code,
        true,    // is_previous_state_down,
        true));  // is_in_transition_state
    EXPECT_EQ(0xc0400001, lparam.lparam());

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;
    initial_state.last_down_key = last_keydown_virtual_key;

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_FALSE(result.should_be_eaten);
    EXPECT_FALSE(result.should_be_sent_to_server);
  }
}

TEST_F(KeyEventHandlerTest, ProtocolAnomaly_ModiferKeyMayBeSentOnKeyUp) {
  // Currently, the Mozc server expects the client to send key-up events in
  // some special cases.  See comments in ImeCore::ImeProcessKey for details.
  // Unfortunately, current implementation does not take some tricky key
  // sequences such as b/2899541 into account.
  // TODO(yukawa): Fix b/2899541 and add unit tests.
  // TODO(yukawa): File this issue as a protocol bug so that we can improve
  // the Mozc protocol later.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press Shift
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_SHIFT);
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x2a,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x002a0001, lparam.lparam());

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_FALSE(result.should_be_eaten);
    EXPECT_FALSE(result.should_be_sent_to_server);
    EXPECT_FALSE(mock.start_server_called());
  }

  // Release Shift
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);

    const VirtualKey previous_virtual_key =
        VirtualKey::FromVirtualKey(VK_SHIFT);
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_SHIFT);
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,  // repeat_count
        0x2a,    // scan_code
        false,   // is_extended_key,
        false,   // has_context_code,
        false,   // is_previous_state_down,
        true));  // is_in_transition_state
    EXPECT_EQ(0x802a0001, lparam.lparam());

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;
    initial_state.last_down_key = previous_virtual_key;

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::TEST_SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    // Interestingly we have to set SHIFT modifier in spite of the Shift key
    // has been just released.
    EXPECT_EQ(commands::KeyEvent::SHIFT, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest,
       ProtocolAnomaly_ModifierShiftShouldBeRemovedForPrintableChar) {
  // Currently, the Mozc server expects the client remove Shift modifier if
  // the key generates any printable character.
  // TODO(yukawa): File this issue as a protocol bug so that we can improve
  // the Mozc protocol later.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press 'Shift+A'
  {
    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState('A', kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('A');
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x1e,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x1e0001, lparam.lparam());

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::TEST_SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('A', actual_input.key().key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    // Interestingly, Mozc client is required not to set Shift here.
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest,
       ProtocolAnomaly_ModifierKeysShouldBeRemovedAsForSomeSpecialKeys) {
  // Currently, the Mozc server expects the client remove all modifiers as for
  // some special keys such as VK_DBE_KATAKANA.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::FULL_KATAKANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::FULL_KATAKANA);
  mock_output.mutable_status()->set_comeback_mode(commands::FULL_KATAKANA);


  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press 'Shift+Katakana'
  {
    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_DBE_KATAKANA, kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_DBE_KATAKANA);
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x70,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        true,     // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x40700001, lparam.lparam());

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    // This is one of force activation keys.
    EXPECT_TRUE(mock.start_server_called());

    // Should be Full-Katakana
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN |
              IME_CMODE_KATAKANA, next_state.logical_conversion_mode);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::TEST_SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_input_style());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    // Interestingly, Mozc client is required not to set Shift here.
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::KATAKANA, actual_input.key().special_key());
  }
}

TEST_F(KeyEventHandlerTest,
       ProtocolAnomaly_KeyCodeIsFullWidthHiraganaWhenKanaLockIsEnabled) {
  // Currently, the Mozc client is required to do extra work for Kana-Input.
  // The client should set |key_code()| as if Kana-lock was disabled.
  // TODO(yukawa): File this issue as a protocol bug so that we can improve
  // the Mozc protocol later.

  const bool kKanaLocked = true;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press 'A' with Kana-lock
  {
    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    KeyboardStatus keyboard_status;
    keyboard_status.SetState('A', kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('A');
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x1e,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x1e0001, lparam.lparam());

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::TEST_SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('a', actual_input.key().key_code());
    EXPECT_TRUE(actual_input.key().has_key_string());
    // "あ"
    EXPECT_EQ("\xE3\x81\xA1", actual_input.key().key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest,
       CheckKeyCodeWhenAlphabeticalKeyIsPressedWithCtrl) {
  // When a user presses an alphabet key and a control key, keyboard-layout
  // drivers produce a control code (0x01,...,0x20), to which the session
  // server assigns its own code.  To avoid conflicts between a control code
  // and one internally-used by the session server, we should decompose a
  // control code into a tuple of an ASCII alphabet and a modifier key.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press 'Ctrl+A'
  {
    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_CONTROL, kPressed);
    keyboard_status.SetState('A', kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('A');
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x1e,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x1e0001, lparam.lparam());

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::TEST_SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('a', actual_input.key().key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest,
       CheckKeyCodeWhenAlphabeticalKeyIsPressedWithCtrlInKanaMode) {
  // When a user presses an alphabet key and a control key, keyboard-layout
  // drivers produce a control code (0x01,...,0x20), to which the session
  // server assigns its own code.  This should not be passed to the server
  // as a Kana-input character. See b/9684668.

  const bool kKanaLocked = true;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press 'Ctrl+A'
  {
    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_CONTROL, kPressed);
    keyboard_status.SetState('A', kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('A');
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x1e,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x1e0001, lparam.lparam());

    Output output;
    result = TestableKeyEventHandler::ImeProcessKey(
        virtual_key, lparam.GetScanCode(), lparam.IsKeyDownInImeProcessKey(),
        keyboard_status, behavior, initial_state, context,
        mock.mutable_client(), &keyboard, &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::TEST_SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('a', actual_input.key().key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::CTRL, actual_input.key().modifier_keys(0));
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}


TEST_F(KeyEventHandlerTest,
       Issue2801503_ModeChangeWhenIMEIsGoingToBeTurnedOff) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::DIRECT);
  mock_output.mutable_status()->set_activated(false);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;

  Context context;

  // Press 'Hankaku/Zenkaku' to close IME.
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_DBE_DBCSCHAR, kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_DBE_DBCSCHAR);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;

    InputState initial_state;
    // Assume that the temporal half-alphanumeric is on-going.
    initial_state.logical_conversion_mode = IME_CMODE_ALPHANUMERIC;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    // IME will be turned off.
    EXPECT_FALSE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    // Next conversion status is determined by mock_output.status() instead of
    // mock_output.mode(), which is unfortunately |commands::DIRECT| in this
    // case.  (This was the main reason why http://b/2801503 happened)
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
  }
}

TEST_F(KeyEventHandlerTest, Issue3029665_KanaLocked_WO) {
  const bool kKanaLocked = true;

  Output mock_output;
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);
  EXPECT_TRUE(keyboard.kana_locked());

  InputState next_state;
  Output output;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // "を"
  {
    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('0');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState('0', kPressed);

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard,
        &next_state, &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_TRUE(actual_input.key().has_key_code());
    EXPECT_EQ('0', actual_input.key().key_code());
    EXPECT_TRUE(actual_input.key().has_key_string());
    // "を"
    EXPECT_EQ("\343\202\222", actual_input.key().key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest,
       Issue3109571_ShiftHenkanShouldBeValid) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;

  Context context;

  // Press 'Shift + Henkan'
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_CONVERT, kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_CONVERT);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard,
        &next_state, &output);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::SHIFT, actual_input.key().modifier_keys(0));
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::HENKAN, actual_input.key().special_key());
  }
}

TEST_F(KeyEventHandlerTest,
       Issue3109571_ShiftMuhenkanShouldBeValid) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;

  Context context;

  // Press 'Shift + Muhenkan'
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState(VK_NONCONVERT, kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_NONCONVERT);
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard,
        &next_state, &output);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(1, actual_input.key().modifier_keys_size());
    EXPECT_EQ(commands::KeyEvent::SHIFT, actual_input.key().modifier_keys(0));
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::MUHENKAN, actual_input.key().special_key());
  }
}

TEST_F(KeyEventHandlerTest, Issue7098463_HideSuggestWindow) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HIRAGANA);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;

  Context context;
  context.set_suppress_suggestion(true);

  // Press 'A'
  {
    KeyboardStatus keyboard_status;
    keyboard_status.SetState('A', kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('A');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard,
        &next_state, &output);
  }
  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_context());
    EXPECT_TRUE(actual_input.context().suppress_suggestion());
  }
}

TEST(SimpleImeKeyEventHandlerTest, ToggleInputStyleByRomanKey) {
  const bool kKeyDown = true;
  const bool kKeyUp = false;

  const VirtualKey key_VK_DBE_ROMAN = VirtualKey::FromVirtualKey(VK_DBE_ROMAN);
  const VirtualKey key_VK_DBE_NOROMAN =
      VirtualKey::FromVirtualKey(VK_DBE_NOROMAN);

  // If you hit Alt+Hiragana/Katakana when VK_DBE_ROMAN has been pressed,
  // you will receive key events in the following order.
  //    VK_DBE_ROMAN     Up
  //    VK_DBE_NOROMAN   Down
  // If you hit Alt+Hiragana/Katakana when VK_DBE_NOROMAN has been pressed,
  // you will receive key events in the following order.
  //    VK_DBE_NOROMAN   Up
  //    VK_DBE_ROMAN     Down

  // Here, we make sure if a key down message flip the input style when the IME
  // is turned on.

  // [Roman -> Kana] by VK_DBE_NOROMAN when IME is ON
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = false;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyUp, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyDown, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);
  }

  // [Kana -> Roman] by VK_DBE_NOROMAN when IME is ON
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = true;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyUp, state,
        &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyDown, state,
        &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);
  }

  // [Roman -> Kana] by VK_DBE_ROMAN when IME is ON
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = false;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyUp, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyDown, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);
  }

  // [Kana -> Roman] by VK_DBE_ROMAN when IME is ON
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = true;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyUp, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyDown, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);
  }

  // Here, we make sure if a key down message flip the input style when the IME
  // is turned off.

  // [Roman -> Roman] by VK_DBE_NOROMAN when IME is off
  {
    InputState state;
    state.open = false;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = false;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyUp, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyDown, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);
  }

  // [Kana -> Kana] by VK_DBE_NOROMAN when IME is off
  {
    InputState state;
    state.open = false;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = true;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyUp, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyDown, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);
  }

  // [Roman -> Roman] by VK_DBE_ROMAN when IME is off
  {
    InputState state;
    state.open = false;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = false;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyUp, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyDown, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);
  }

  // [Kana -> Kana] by VK_DBE_ROMAN when IME is off
  {
    InputState state;
    state.open = false;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = true;
    behavior.use_romaji_key_to_toggle_input_style = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyUp, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyDown, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);
  }

  // [Roman -> Roman] by VK_DBE_NOROMAN when
  // |behavior.use_romaji_key_to_toggle_input_style| is false
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = false;
    behavior.use_romaji_key_to_toggle_input_style = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyUp, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyDown, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);
  }

  // [Kana -> Kana] by VK_DBE_NOROMAN when
  // |behavior.use_romaji_key_to_toggle_input_style| is false
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = true;
    behavior.use_romaji_key_to_toggle_input_style = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyUp, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyDown, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);
  }

  // [Roman -> Roman] by VK_DBE_ROMAN when
  // |behavior.use_romaji_key_to_toggle_input_style| is false
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = false;
    behavior.use_romaji_key_to_toggle_input_style = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyUp, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyDown, state, &behavior);
    EXPECT_FALSE(behavior.prefer_kana_input);
  }

  // [Kana -> Kana] by VK_DBE_ROMAN when
  // |behavior.use_romaji_key_to_toggle_input_style| is false
  {
    InputState state;
    state.open = true;
    // conversion status will not be cared about.
    state.logical_conversion_mode = 0;

    InputBehavior behavior;

    behavior.prefer_kana_input = true;
    behavior.use_romaji_key_to_toggle_input_style = false;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_NOROMAN, kKeyUp, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);

    behavior.prefer_kana_input = true;
    TestableKeyEventHandler::UpdateBehaviorInImeProcessKey(
        key_VK_DBE_ROMAN, kKeyDown, state, &behavior);
    EXPECT_TRUE(behavior.prefer_kana_input);
  }
}

TEST_F(KeyEventHandlerTest, Issue3504241_VKPacketAsRawInput) {
  // To fix b/3504241, VK_PACKET must be supported.

  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Release VK_PACKET ('あ')
  {
    KeyboardStatus keyboard_status;

    const wchar_t kHiraganaA = L'\u3042';
    const VirtualKey virtual_key = VirtualKey::FromCombinedVirtualKey(
        (static_cast<DWORD>(kHiraganaA) << 16) | VK_PACKET);
    const VirtualKey last_keydown_virtual_key =
        VirtualKey::FromVirtualKey(VK_ESCAPE);

    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    // VK_PACKET will be handled by the server.
    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_TRUE(actual_input.key().has_key_string());
    // "あ"
    EXPECT_EQ("\343\201\202", actual_input.key().key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_FALSE(actual_input.key().has_special_key());
  }
}

TEST_F(KeyEventHandlerTest, CapsLock) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press VK_CAPITAL
  {
    KeyboardStatus keyboard_status;

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_CAPITAL);

    const BYTE scan_code = 0;  // will be ignored in this test;
    const bool is_key_down = true;

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    // VK_PACKET will be handled by the server.
    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);
  }

  {
    commands::Input actual_input;
    EXPECT_TRUE(mock.GetGeneratedRequest(&actual_input));
    EXPECT_EQ(commands::Input::SEND_KEY, actual_input.type());
    EXPECT_TRUE(actual_input.has_key());
    EXPECT_FALSE(actual_input.key().has_key_code());
    EXPECT_FALSE(actual_input.key().has_key_string());
    EXPECT_TRUE(actual_input.key().has_activated());
    EXPECT_TRUE(actual_input.key().activated());
    EXPECT_TRUE(actual_input.key().has_mode());
    EXPECT_EQ(commands::HIRAGANA, actual_input.key().mode());
    EXPECT_FALSE(actual_input.key().has_modifiers());
    EXPECT_EQ(0, actual_input.key().modifier_keys_size());
    EXPECT_TRUE(actual_input.key().has_special_key());
    EXPECT_EQ(commands::KeyEvent::CAPS_LOCK, actual_input.key().special_key());
  }
}

// In IMM32 mode, the OS handles VK_KANJI to activate IME. So we must not send
// it to the server. Otherwise, IME On/Off flipping happens twice and a user
// cannot activate IME by VK_KANJI.
TEST_F(KeyEventHandlerTest, KanjiKey_Issue7970379) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;
  behavior.direct_mode_keys = GetDefaultDirectModeKeys();

  Context context;

  // Press VK_KANJI
  {
    KeyboardStatus keyboard_status;

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey(VK_KANJI);

    const BYTE scan_code = 0;  // will be ignored in this test;
    const bool is_key_down = true;

    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    // VK_KANJI must not be handled by the server.
    EXPECT_TRUE(result.succeeded);
    EXPECT_FALSE(result.should_be_eaten);
    EXPECT_FALSE(result.should_be_sent_to_server);
  }
}

// Temporal alphanumeric mode will be stored into |visible_conversion_mode|.
TEST_F(KeyEventHandlerTest, Issue8524269_ComebackMode) {
  const bool kKanaLocked = false;

  Output mock_output;
  mock_output.set_consumed(true);
  mock_output.set_mode(commands::HALF_ASCII);
  mock_output.mutable_status()->set_activated(true);
  mock_output.mutable_status()->set_mode(commands::HALF_ASCII);
  mock_output.mutable_status()->set_comeback_mode(commands::HIRAGANA);

  MockState mock(mock_output);
  KeyboardMock keyboard(kKanaLocked);

  InputState next_state;
  KeyEventHandlerResult result;

  InputBehavior behavior;
  behavior.prefer_kana_input = kKanaLocked;
  behavior.disabled = false;

  Context context;

  // Press 'Shift+A'
  {
    InputState initial_state;
    initial_state.logical_conversion_mode =
        IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN;
    initial_state.visible_conversion_mode =
        initial_state.logical_conversion_mode;
    initial_state.open = true;

    KeyboardStatus keyboard_status;
    keyboard_status.SetState(VK_SHIFT, kPressed);
    keyboard_status.SetState('A', kPressed);

    const VirtualKey virtual_key = VirtualKey::FromVirtualKey('A');
    const BYTE scan_code = 0;  // will be ignored in this test
    const bool is_key_down = true;
    const LParamKeyInfo lparam(CreateLParam(
        0x0001,   // repeat_count
        0x1e,     // scan_code
        false,    // is_extended_key,
        false,    // has_context_code,
        false,    // is_previous_state_down,
        false));  // is_in_transition_state
    EXPECT_EQ(0x1e0001, lparam.lparam());

    Output output;
    result = TestableKeyEventHandler::ImeToAsciiEx(
        virtual_key, scan_code, is_key_down, keyboard_status, behavior,
        initial_state, context, mock.mutable_client(), &keyboard, &next_state,
        &output);

    EXPECT_TRUE(result.succeeded);
    EXPECT_TRUE(result.should_be_eaten);
    EXPECT_TRUE(result.should_be_sent_to_server);

    EXPECT_TRUE(next_state.open);
    EXPECT_TRUE(mock.start_server_called());
    EXPECT_EQ(IME_CMODE_NATIVE | IME_CMODE_FULLSHAPE | IME_CMODE_ROMAN,
              next_state.logical_conversion_mode);
    // Visible mode should be half alphanumeric.
    EXPECT_EQ(IME_CMODE_ALPHANUMERIC | IME_CMODE_ROMAN,
              next_state.visible_conversion_mode);
  }
}

}  // namespace win32
}  // namespace mozc
