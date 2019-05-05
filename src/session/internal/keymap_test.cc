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

// Keymap utils of Mozc interface.

#include "session/internal/keymap.h"
#include "session/internal/keymap-inl.h"

#include <memory>
#include <sstream>
#include <vector>

#include "base/config_file_stream.h"
#include "base/system_util.h"
#include "composer/key_parser.h"
#include "config/config_handler.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "session/internal/keymap_factory.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

namespace mozc {
namespace keymap {

class KeyMapTest : public testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }

  bool isInputModeXCommandSupported() const {
    return KeyMapManager::kInputModeXCommandSupported;
  }
};

TEST_F(KeyMapTest, AddRule) {
  KeyMap<PrecompositionState> keymap;
  commands::KeyEvent key_event;
  // 'a'
  key_event.set_key_code(97);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 0 is treated as null input
  key_event.set_key_code(0);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 1 to 31 should be rejected.
  key_event.set_key_code(1);
  EXPECT_FALSE(keymap.AddRule(key_event,
                              PrecompositionState::INSERT_CHARACTER));
  key_event.set_key_code(31);
  EXPECT_FALSE(keymap.AddRule(key_event,
                              PrecompositionState::INSERT_CHARACTER));

  // 32 (space) is also invalid input.
  key_event.set_key_code(32);
  EXPECT_FALSE(keymap.AddRule(key_event,
                              PrecompositionState::INSERT_CHARACTER));

  // 33 (!) is a valid input.
  key_event.set_key_code(33);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));

  // 8bit char is a valid input.
  key_event.set_key_code(127);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));
  key_event.set_key_code(255);
  EXPECT_TRUE(keymap.AddRule(key_event, PrecompositionState::INSERT_CHARACTER));
}

TEST_F(KeyMapTest, GetCommand) {
  {
    KeyMap<PrecompositionState> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_key_code(97);
    EXPECT_TRUE(keymap.AddRule(init_key_event,
                               PrecompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);

    key_event.Clear();
    key_event.set_key_code(98);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }
  {
    KeyMap<PrecompositionState> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_special_key(commands::KeyEvent::TEXT_INPUT);
    EXPECT_TRUE(keymap.AddRule(init_key_event,
                               PrecompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_string("hoge");
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);
  }
  {
    KeyMap<CompositionState> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_key_code(97);
    EXPECT_TRUE(keymap.AddRule(init_key_event,
                               CompositionState::INSERT_CHARACTER));

    commands::KeyEvent key_event;
    CompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(CompositionState::INSERT_CHARACTER, command);

    key_event.Clear();
    key_event.set_key_code(98);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }
  {
    KeyMap<CompositionState> keymap;
    commands::KeyEvent init_key_event;
    init_key_event.set_special_key(commands::KeyEvent::ENTER);
    EXPECT_TRUE(keymap.AddRule(init_key_event, CompositionState::COMMIT));

    init_key_event.Clear();
    init_key_event.set_special_key(commands::KeyEvent::DEL);
    init_key_event.add_modifier_keys(commands::KeyEvent::CTRL);
    init_key_event.add_modifier_keys(commands::KeyEvent::ALT);
    EXPECT_TRUE(keymap.AddRule(init_key_event, CompositionState::IME_OFF));

    commands::KeyEvent key_event;
    CompositionState::Commands command;

    // ENTER
    key_event.Clear();
    key_event.set_special_key(commands::KeyEvent::ENTER);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(CompositionState::COMMIT, command);

    // CTRL-ALT-DELETE
    key_event.Clear();
    key_event.set_special_key(commands::KeyEvent::DEL);
    key_event.add_modifier_keys(commands::KeyEvent::CTRL);
    key_event.add_modifier_keys(commands::KeyEvent::ALT);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(CompositionState::IME_OFF, command);
  }
}

TEST_F(KeyMapTest, GetCommandForKeyString) {
  KeyMap<PrecompositionState> keymap;

  // When a key event is not registered, GetCommand should return false.
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }

  // When a key event is not registered, GetCommand should return false even if
  // the key event has |key_string|. See also b/9684668
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    key_event.set_key_string("a");
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }

  // Special case for b/4170089. VK_PACKET on Windows will be encoded as
  // {
  //   key_code: (empty)
  //   key_string: (the Unicode string to be input)
  // }
  // We always treat such key events as INSERT_CHARACTER command.
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_string("a");
    EXPECT_FALSE(keymap.GetCommand(key_event, &command));
  }

  // After adding the rule of TEXT_INPUT -> INSERT_CHARACTER, the above cases
  // should return INSERT_CHARACTER.
  commands::KeyEvent text_input_key_event;
  text_input_key_event.set_special_key(commands::KeyEvent::TEXT_INPUT);
  keymap.AddRule(text_input_key_event, PrecompositionState::INSERT_CHARACTER);

  // key_code = 97, key_string = empty
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);
  }

  // key_code = 97, key_string = "a"
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_code(97);
    key_event.set_key_string("a");
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);
  }

  // key_code = empty, key_string = "a"
  {
    commands::KeyEvent key_event;
    PrecompositionState::Commands command;
    key_event.set_key_string("a");
    EXPECT_TRUE(keymap.GetCommand(key_event, &command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);
  }
}

TEST_F(KeyMapTest, GetCommandKeyStub) {
  KeyMap<PrecompositionState> keymap;
  commands::KeyEvent init_key_event;
  init_key_event.set_special_key(commands::KeyEvent::TEXT_INPUT);
  EXPECT_TRUE(keymap.AddRule(init_key_event,
                             PrecompositionState::INSERT_CHARACTER));

  PrecompositionState::Commands command;
  commands::KeyEvent key_event;
  key_event.set_key_code(97);
  EXPECT_TRUE(keymap.GetCommand(key_event, &command));
  EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, command);
}

TEST_F(KeyMapTest, GetKeyMapFileName) {
  EXPECT_STREQ("system://atok.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::ATOK));
  EXPECT_STREQ("system://mobile.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::MOBILE));
  EXPECT_STREQ("system://ms-ime.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::MSIME));
  EXPECT_STREQ("system://kotoeri.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::KOTOERI));
  EXPECT_STREQ("system://chromeos.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::CHROMEOS));
  EXPECT_STREQ("user://keymap.tsv",
               KeyMapManager::GetKeyMapFileName(config::Config::CUSTOM));
}

TEST_F(KeyMapTest, DefaultKeyBindings) {
  KeyMapManager manager;

  std::istringstream iss("", std::istringstream::in);
  EXPECT_TRUE(manager.LoadStream(&iss));

  {  // Check key bindings of TextInput.
    commands::KeyEvent key_event;
    KeyParser::ParseKey("TextInput", &key_event);

    PrecompositionState::Commands fund_command;
    EXPECT_TRUE(manager.GetCommandPrecomposition(key_event, &fund_command));
    EXPECT_EQ(PrecompositionState::INSERT_CHARACTER, fund_command);

    CompositionState::Commands composition_command;
    EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
    EXPECT_EQ(CompositionState::INSERT_CHARACTER, composition_command);

    ConversionState::Commands conv_command;
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::INSERT_CHARACTER, conv_command);
  }

  {  // Check key bindings of Shift.
    commands::KeyEvent key_event;
    KeyParser::ParseKey("Shift", &key_event);

    PrecompositionState::Commands fund_command;
    EXPECT_FALSE(manager.GetCommandPrecomposition(key_event, &fund_command));

    CompositionState::Commands composition_command;
    EXPECT_TRUE(manager.GetCommandComposition(key_event, &composition_command));
    EXPECT_EQ(CompositionState::INSERT_CHARACTER, composition_command);

    ConversionState::Commands conv_command;
    EXPECT_FALSE(manager.GetCommandConversion(key_event, &conv_command));
  }
}

TEST_F(KeyMapTest, LoadStreamWithErrors) {
  KeyMapManager manager;
  std::vector<string> errors;
  std::unique_ptr<std::istream> is(
      ConfigFileStream::LegacyOpen("system://atok.tsv"));
  EXPECT_TRUE(manager.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());

  errors.clear();
  is.reset(ConfigFileStream::LegacyOpen("system://ms-ime.tsv"));
  EXPECT_TRUE(manager.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());

  errors.clear();
  is.reset(ConfigFileStream::LegacyOpen("system://kotoeri.tsv"));
  EXPECT_TRUE(manager.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());

  errors.clear();
  is.reset(ConfigFileStream::LegacyOpen("system://mobile.tsv"));
  EXPECT_TRUE(manager.LoadStreamWithErrors(is.get(), &errors));
  EXPECT_TRUE(errors.empty());
}

TEST_F(KeyMapTest, GetName) {
  KeyMapManager manager;
  {
    // Direct
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandDirect(DirectInputState::IME_ON,
                                                 &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandDirect(DirectInputState::RECONVERT,
                                                 &name));
    EXPECT_EQ("Reconvert", name);
  }
  {
    // Precomposition
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::IME_OFF, &name));
    EXPECT_EQ("IMEOff", name);
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::IME_ON, &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::INSERT_CHARACTER, &name));
    EXPECT_EQ("InsertCharacter", name);
    EXPECT_TRUE(manager.GetNameFromCommandPrecomposition(
        PrecompositionState::RECONVERT, &name));
    EXPECT_EQ("Reconvert", name);
  }
  {
    // Composition
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandComposition(
        CompositionState::IME_OFF, &name));
    EXPECT_EQ("IMEOff", name);
    EXPECT_TRUE(manager.GetNameFromCommandComposition(
        CompositionState::IME_ON, &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandComposition(
        CompositionState::INSERT_CHARACTER, &name));
    EXPECT_EQ("InsertCharacter", name);
  }
  {
    // Conversion
    string name;
    EXPECT_TRUE(manager.GetNameFromCommandConversion(
        ConversionState::IME_OFF, &name));
    EXPECT_EQ("IMEOff", name);
    EXPECT_TRUE(manager.GetNameFromCommandConversion(
        ConversionState::IME_ON, &name));
    EXPECT_EQ("IMEOn", name);
    EXPECT_TRUE(manager.GetNameFromCommandConversion(
        ConversionState::INSERT_CHARACTER, &name));
    EXPECT_EQ("InsertCharacter", name);
  }
}

TEST_F(KeyMapTest, DirectModeDoesNotSupportInsertSpace) {
  // InsertSpace, InsertAlternateSpace, InsertHalfSpace, and InsertFullSpace
  // are not supported in direct mode.
  KeyMapManager manager;
  std::set<string> names;
  manager.GetAvailableCommandNameDirect(&names);

  // We cannot use EXPECT_EQ because of overload resolution here.
  EXPECT_TRUE(names.end() == names.find("InsertSpace"));
  EXPECT_TRUE(names.end() == names.find("InsertAlternateSpace"));
  EXPECT_TRUE(names.end() == names.find("InsertHalfSpace"));
  EXPECT_TRUE(names.end() == names.find("InsertFullSpace"));
}

TEST_F(KeyMapTest, ShiftTabToConvertPrev) {
  // http://b/2973471
  // Shift+TAB does not work on a suggestion window

  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // MSIME
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::CONVERT_PREV, conv_command);
  }

  {  // Kotoeri
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::KOTOERI);
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::CONVERT_PREV, conv_command);
  }

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);
    KeyParser::ParseKey("Shift Tab", &key_event);
    EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::CONVERT_PREV, conv_command);
  }
}

TEST_F(KeyMapTest, LaunchToolTest) {
  config::Config config;
  commands::KeyEvent key_event;
  PrecompositionState::Commands conv_command;

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);

    KeyParser::ParseKey("Ctrl F7", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &conv_command));
    EXPECT_EQ(PrecompositionState::LAUNCH_WORD_REGISTER_DIALOG, conv_command);

    KeyParser::ParseKey("Ctrl F12", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &conv_command));
    EXPECT_EQ(PrecompositionState::LAUNCH_CONFIG_DIALOG, conv_command);
  }

  // http://b/3432829
  // MS-IME does not have the key-binding "Ctrl F7" in precomposition mode.
  {
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

    KeyParser::ParseKey("Ctrl F7", &key_event);
    EXPECT_FALSE(manager->GetCommandPrecomposition(key_event, &conv_command));
  }
}

TEST_F(KeyMapTest, Undo) {
  PrecompositionState::Commands command;
  commands::KeyEvent key_event;

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);
    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(PrecompositionState::UNDO, command);
  }
  {  // MSIME
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(PrecompositionState::UNDO, command);
  }
  {  // KOTOERI
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::KOTOERI);

    KeyParser::ParseKey("Ctrl Backspace", &key_event);
    EXPECT_TRUE(manager->GetCommandPrecomposition(key_event, &command));
    EXPECT_EQ(PrecompositionState::UNDO, command);
  }
}

TEST_F(KeyMapTest, Reconvert) {
  DirectInputState::Commands direct_command;
  PrecompositionState::Commands precomposition_command;
  commands::KeyEvent key_event;

  {  // ATOK
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::ATOK);

    KeyParser::ParseKey("Shift Henkan", &key_event);
    EXPECT_TRUE(manager->GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(DirectInputState::RECONVERT, direct_command);
    EXPECT_TRUE(manager->GetCommandPrecomposition(
        key_event, &precomposition_command));
    EXPECT_EQ(PrecompositionState::RECONVERT, precomposition_command);
  }
  {  // MSIME
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

    KeyParser::ParseKey("Henkan", &key_event);
    EXPECT_TRUE(manager->GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(DirectInputState::RECONVERT, direct_command);
    EXPECT_TRUE(manager->GetCommandPrecomposition(
        key_event, &precomposition_command));
    EXPECT_EQ(PrecompositionState::RECONVERT, precomposition_command);
  }
  {  // KOTOERI
    KeyMapManager *manager =
        KeyMapFactory::GetKeyMapManager(config::Config::KOTOERI);

    KeyParser::ParseKey("Ctrl Shift r", &key_event);
    EXPECT_TRUE(manager->GetCommandDirect(key_event, &direct_command));
    EXPECT_EQ(DirectInputState::RECONVERT, direct_command);
    EXPECT_TRUE(manager->GetCommandPrecomposition(
        key_event, &precomposition_command));
    EXPECT_EQ(PrecompositionState::RECONVERT, precomposition_command);
  }
}

TEST_F(KeyMapTest, Initialize) {
  KeyMapManager manager;
  config::Config::SessionKeymap keymap_setting;
  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // ATOK
    keymap_setting = config::Config::ATOK;
    manager.Initialize(keymap_setting);
    KeyParser::ParseKey("Right", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::SEGMENT_WIDTH_EXPAND, conv_command);
  }
  {  // MSIME
    keymap_setting = config::Config::MSIME;
    manager.Initialize(keymap_setting);
    KeyParser::ParseKey("Right", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::SEGMENT_FOCUS_RIGHT, conv_command);
  }
}

TEST_F(KeyMapTest, AddCommand) {
  KeyMapManager manager;
  commands::KeyEvent key_event;
  const char kKeyEvent[] = "Ctrl Shift Insert";

  KeyParser::ParseKey(kKeyEvent, &key_event);

  {  // Add command
    CompositionState::Commands command;
    EXPECT_FALSE(manager.GetCommandComposition(key_event, &command));

    EXPECT_TRUE(manager.AddCommand("Composition", kKeyEvent, "Cancel"));

    EXPECT_TRUE(manager.GetCommandComposition(key_event, &command));
    EXPECT_EQ(CompositionState::CANCEL, command);
  }

  {  // Error detections
    EXPECT_FALSE(manager.AddCommand("void", kKeyEvent, "Cancel"));
    EXPECT_FALSE(manager.AddCommand("Composition", kKeyEvent, "Unknown"));
    EXPECT_FALSE(manager.AddCommand("Composition", "INVALID", "Cancel"));
  }
}

TEST_F(KeyMapTest, ZeroQuerySuggestion) {
  KeyMapManager manager;
  EXPECT_TRUE(manager.AddCommand("ZeroQuerySuggestion",
                                 "ESC", "Cancel"));
  EXPECT_TRUE(manager.AddCommand("ZeroQuerySuggestion",
                                 "Tab", "PredictAndConvert"));
  EXPECT_TRUE(manager.AddCommand("ZeroQuerySuggestion",
                                 "Shift Enter", "CommitFirstSuggestion"));
  // For fallback testing
  EXPECT_TRUE(manager.AddCommand("Precomposition", "Ctrl Backspace", "Revert"));

  commands::KeyEvent key_event;
  PrecompositionState::Commands command;

  KeyParser::ParseKey("ESC", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::CANCEL, command);

  KeyParser::ParseKey("Tab", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::PREDICT_AND_CONVERT, command);

  KeyParser::ParseKey("Shift Enter", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::COMMIT_FIRST_SUGGESTION, command);

  KeyParser::ParseKey("Ctrl Backspace", &key_event);
  EXPECT_TRUE(manager.GetCommandZeroQuerySuggestion(key_event, &command));
  EXPECT_EQ(PrecompositionState::REVERT, command);
}

TEST_F(KeyMapTest, CapsLock) {
  // MSIME
  KeyMapManager *manager =
      KeyMapFactory::GetKeyMapManager(config::Config::MSIME);
  commands::KeyEvent key_event;
  KeyParser::ParseKey("CAPS a", &key_event);

  ConversionState::Commands conv_command;
  EXPECT_TRUE(manager->GetCommandConversion(key_event, &conv_command));
  EXPECT_EQ(ConversionState::INSERT_CHARACTER, conv_command);
}

TEST_F(KeyMapTest, ShortcutKeysWithCapsLock_Issue5627459) {
  // MSIME
  KeyMapManager *manager =
      KeyMapFactory::GetKeyMapManager(config::Config::MSIME);

  commands::KeyEvent key_event;
  CompositionState::Commands composition_command;

  // "Ctrl CAPS H" means that Ctrl and H key are pressed w/o shift key.
  // See the description in command.proto.
  KeyParser::ParseKey("Ctrl CAPS H", &key_event);
  EXPECT_TRUE(manager->GetCommandComposition(key_event, &composition_command));
  EXPECT_EQ(CompositionState::BACKSPACE, composition_command);

  // "Ctrl CAPS h" means that Ctrl, Shift and H key are pressed.
  KeyParser::ParseKey("Ctrl CAPS h", &key_event);
  EXPECT_FALSE(manager->GetCommandComposition(key_event, &composition_command));
}

// InputModeX is not supported on MacOSX.
TEST_F(KeyMapTest, InputModeChangeIsNotEnabledOnChromeOs_Issue13947207) {
  if (!isInputModeXCommandSupported()) {
    return;
  }

  KeyMapManager manager;
  config::Config::SessionKeymap keymap_setting;
  commands::KeyEvent key_event;
  ConversionState::Commands conv_command;

  {  // MSIME
    keymap_setting = config::Config::MSIME;
    manager.Initialize(keymap_setting);
    KeyParser::ParseKey("Hiragana", &key_event);
    EXPECT_TRUE(manager.GetCommandConversion(key_event, &conv_command));
    EXPECT_EQ(ConversionState::INPUT_MODE_HIRAGANA, conv_command);
  }
  {  // CHROMEOS
    keymap_setting = config::Config::CHROMEOS;
    manager.Initialize(keymap_setting);
    KeyParser::ParseKey("Hiragana", &key_event);
    EXPECT_FALSE(manager.GetCommandConversion(key_event, &conv_command));
  }
}

}  // namespace keymap
}  // namespace mozc
