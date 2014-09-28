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

// Keymap utils of Mozc interface.

#include "session/internal/keymap.h"

#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "base/config_file_stream.h"
#include "base/file_stream.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "session/commands.pb.h"
#include "session/internal/keymap-inl.h"
#include "session/key_event_util.h"
#include "session/key_parser.h"

namespace mozc {
namespace keymap {
namespace {
static const char kMSIMEKeyMapFile[] = "system://ms-ime.tsv";
static const char kATOKKeyMapFile[] = "system://atok.tsv";
static const char kKotoeriKeyMapFile[] = "system://kotoeri.tsv";
static const char kCustomKeyMapFile[] = "user://keymap.tsv";
static const char kMobileKeyMapFile[] = "system://mobile.tsv";

#if defined(OS_MACOSX)
const bool kInputModeXCommandSupported = false;
#else
const bool kInputModeXCommandSupported = true;
#endif

}  // anonymous namespace

KeyMapManager::KeyMapManager()
    : keymap_(config::Config::NONE) {
  InitCommandData();
  ReloadWithKeymap(GET_CONFIG(session_keymap));
}

KeyMapManager::~KeyMapManager() {}

bool KeyMapManager::ReloadWithKeymap(
    const config::Config::SessionKeymap new_keymap) {
  // If the current keymap is the same with the new keymap and not
  // CUSTOM, do nothing.
  if (new_keymap == keymap_ && new_keymap != config::Config::CUSTOM) {
    return true;
  }

  keymap_ = new_keymap;
  const char *keymap_file = GetKeyMapFileName(new_keymap);

  // Clear the previous keymaps.
  keymap_direct_.Clear();
  keymap_precomposition_.Clear();
  keymap_composition_.Clear();
  keymap_conversion_.Clear();
  keymap_zero_query_suggestion_.Clear();
  keymap_suggestion_.Clear();
  keymap_prediction_.Clear();

  if (new_keymap == config::Config::CUSTOM) {
    const string &custom_keymap_table = GET_CONFIG(custom_keymap_table);
    if (custom_keymap_table.empty()) {
      LOG(WARNING) << "custom_keymap_table is empty. use default setting";
      const char *default_keymapfile = GetKeyMapFileName(GetDefaultKeyMap());
      return LoadFile(default_keymapfile);
    }
#ifndef NO_LOGGING
    // make a copy of keymap file just for debugging
    const string filename = ConfigFileStream::GetFileName(keymap_file);
    OutputFileStream ofs(filename.c_str());
    if (ofs) {
      ofs << "# This is a copy of keymap table for debugging." << endl;
      ofs << "# Nothing happens when you edit this file manually." << endl;
      ofs << custom_keymap_table;
    }
#endif
    istringstream ifs(custom_keymap_table);
    return LoadStream(&ifs);
  }

  if (keymap_file != NULL && LoadFile(keymap_file)) {
    return true;
  }

  const char *default_keymapfile = GetKeyMapFileName(GetDefaultKeyMap());
  return LoadFile(default_keymapfile);
}

// static
const char *KeyMapManager::GetKeyMapFileName(
    const config::Config::SessionKeymap keymap) {
  switch (keymap) {
    case config::Config::ATOK:
      return kATOKKeyMapFile;
    case config::Config::MOBILE:
      return kMobileKeyMapFile;
    case config::Config::MSIME:
      return kMSIMEKeyMapFile;
    case config::Config::KOTOERI:
      return kKotoeriKeyMapFile;
    case config::Config::CUSTOM:
      return kCustomKeyMapFile;
    case config::Config::NONE:
    default:
      // should not appear here.
      LOG(ERROR) << "Keymap type: " << keymap
                 << " appeared at key map initialization.";
      const config::Config::SessionKeymap default_keymap = GetDefaultKeyMap();
      DCHECK(default_keymap == config::Config::ATOK ||
             default_keymap == config::Config::MOBILE ||
             default_keymap == config::Config::MSIME ||
             default_keymap == config::Config::KOTOERI ||
             default_keymap == config::Config::CUSTOM);
      // should never make loop.
      return GetKeyMapFileName(default_keymap);
  }
}

// static
config::Config::SessionKeymap KeyMapManager::GetDefaultKeyMap() {
#ifdef OS_MACOSX
  return config::Config::KOTOERI;
#else  // OS_MACOSX
  return config::Config::MSIME;
#endif  // OS_MACOSX
}

bool KeyMapManager::LoadFile(const char *filename) {
  scoped_ptr<istream> ifs(ConfigFileStream::LegacyOpen(filename));
  if (ifs.get() == NULL) {
    LOG(WARNING) << "cannot load keymap table: " << filename;
    return false;
  }
  return LoadStream(ifs.get());
}

bool KeyMapManager::LoadStream(istream *ifs) {
  vector<string> errors;
  return LoadStreamWithErrors(ifs, &errors);
}

bool KeyMapManager::LoadStreamWithErrors(istream *ifs, vector<string> *errors) {
  string line;
  getline(*ifs, line);  // Skip the first line.
  while (!ifs->eof()) {
    getline(*ifs, line);
    Util::ChopReturns(&line);

    if (line.empty() || line[0] == '#') {  // Skip empty or comment line.
      continue;
    }

    vector<string> rules;
    Util::SplitStringUsing(line, "\t", &rules);
    if (rules.size() != 3) {
      LOG(ERROR) << "Invalid format: " << line;
      continue;
    }

    if (!AddCommand(rules[0], rules[1], rules[2])) {
      errors->push_back(line);
      LOG(ERROR) << "Unknown command: " << line;
    }
  }

  commands::KeyEvent key_event;
  KeyParser::ParseKey("TextInput", &key_event);
  keymap_precomposition_.AddRule(key_event,
                                 PrecompositionState::INSERT_CHARACTER);
  keymap_composition_.AddRule(key_event, CompositionState::INSERT_CHARACTER);
  keymap_conversion_.AddRule(key_event, ConversionState::INSERT_CHARACTER);

  key_event.Clear();
  KeyParser::ParseKey("Shift", &key_event);
  keymap_composition_.AddRule(key_event, CompositionState::INSERT_CHARACTER);
  return true;
}

bool KeyMapManager::AddCommand(const string &state_name,
                               const string &key_event_name,
                               const string &command_name) {
#ifdef NO_LOGGING  // means RELEASE BUILD
  // On the release build, we do not support the ReportBug
  // commands.  Note, true is returned as the arguments are
  // interpreted properly.
  if (command_name == "ReportBug") {
    return true;
  }
#endif  // NO_LOGGING

  commands::KeyEvent key_event;
  if (!KeyParser::ParseKey(key_event_name, &key_event)) {
    return false;
  }

  if (state_name == "DirectInput" || state_name == "Direct") {
    DirectInputState::Commands command;
    if (!ParseCommandDirect(command_name, &command)) {
      return false;
    }

    keymap_direct_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Precomposition") {
    PrecompositionState::Commands command;
    if (!ParseCommandPrecomposition(command_name, &command)) {
      return false;
    }

    keymap_precomposition_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Composition") {
    CompositionState::Commands command;
    if (!ParseCommandComposition(command_name, &command)) {
      return false;
    }

    keymap_composition_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Conversion") {
    ConversionState::Commands command;
    if (!ParseCommandConversion(command_name, &command)) {
      return false;
    }

    keymap_conversion_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "ZeroQuerySuggestion") {
    PrecompositionState::Commands command;
    if (!ParseCommandPrecomposition(command_name, &command)) {
      return false;
    }

    keymap_zero_query_suggestion_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Suggestion") {
    CompositionState::Commands command;
    if (!ParseCommandComposition(command_name, &command)) {
      return false;
    }

    keymap_suggestion_.AddRule(key_event, command);
    return true;
  }

  if (state_name == "Prediction") {
    ConversionState::Commands command;
    if (!ParseCommandConversion(command_name, &command)) {
      return false;
    }

    keymap_prediction_.AddRule(key_event, command);
    return true;
  }

  return false;
}

namespace {
template<typename T> bool GetNameInternal(
    const map<T, string> &reverse_command_map, T command, string *name) {
  DCHECK(name);
  typename map<T, string>::const_iterator iter =
      reverse_command_map.find(command);
  if (iter == reverse_command_map.end()) {
    return false;
  } else {
    *name = iter->second;
    return true;
  }
}
}  // namespace

bool KeyMapManager::GetNameFromCommandDirect(
    DirectInputState::Commands command, string *name) const {
  return GetNameInternal<DirectInputState::Commands>(
      reverse_command_direct_map_, command, name);
}

bool KeyMapManager::GetNameFromCommandPrecomposition(
    PrecompositionState::Commands command, string *name) const {
  return GetNameInternal<PrecompositionState::Commands>(
      reverse_command_precomposition_map_, command, name);
}

bool KeyMapManager::GetNameFromCommandComposition(
    CompositionState::Commands command, string *name) const {
  return GetNameInternal<CompositionState::Commands>(
      reverse_command_composition_map_, command, name);
}

bool KeyMapManager::GetNameFromCommandConversion(
    ConversionState::Commands command, string *name) const {
  return GetNameInternal<ConversionState::Commands>(
      reverse_command_conversion_map_, command, name);
}

void KeyMapManager::RegisterDirectCommand(
    const string &command_string, DirectInputState::Commands command) {
  command_direct_map_[command_string] = command;
  reverse_command_direct_map_[command] = command_string;
}

void KeyMapManager::RegisterPrecompositionCommand(
    const string &command_string, PrecompositionState::Commands command) {
  command_precomposition_map_[command_string] = command;
  reverse_command_precomposition_map_[command] = command_string;
}

void KeyMapManager::RegisterCompositionCommand(
    const string &command_string, CompositionState::Commands command) {
  command_composition_map_[command_string] = command;
  reverse_command_composition_map_[command] = command_string;
}

void KeyMapManager::RegisterConversionCommand(
    const string &command_string, ConversionState::Commands command) {
  command_conversion_map_[command_string] = command;
  reverse_command_conversion_map_[command] = command_string;
}

void KeyMapManager::InitCommandData() {
  RegisterDirectCommand("IMEOn", DirectInputState::IME_ON);
  // Support InputMode command only on Windows for now.
  // TODO(toshiyuki): delete #ifdef when we support them on Mac, and
  // activate SessionTest.InputModeConsumedForTestSendKey.
#ifdef OS_WIN
  RegisterDirectCommand("InputModeHiragana",
                        DirectInputState::INPUT_MODE_HIRAGANA);
  RegisterDirectCommand("InputModeFullKatakana",
                        DirectInputState::INPUT_MODE_FULL_KATAKANA);
  RegisterDirectCommand("InputModeHalfKatakana",
                        DirectInputState::INPUT_MODE_HALF_KATAKANA);
  RegisterDirectCommand("InputModeFullAlphanumeric",
                        DirectInputState::INPUT_MODE_FULL_ALPHANUMERIC);
  RegisterDirectCommand("InputModeHalfAlphanumeric",
                        DirectInputState::INPUT_MODE_HALF_ALPHANUMERIC);
#else
  RegisterDirectCommand("InputModeHiragana",
                        DirectInputState::NONE);
  RegisterDirectCommand("InputModeFullKatakana",
                        DirectInputState::NONE);
  RegisterDirectCommand("InputModeHalfKatakana",
                        DirectInputState::NONE);
  RegisterDirectCommand("InputModeFullAlphanumeric",
                        DirectInputState::NONE);
  RegisterDirectCommand("InputModeHalfAlphanumeric",
                        DirectInputState::NONE);
#endif  // OS_WIN
  RegisterDirectCommand("Reconvert",
                        DirectInputState::RECONVERT);

  // Precomposition
  RegisterPrecompositionCommand("IMEOff", PrecompositionState::IME_OFF);
  RegisterPrecompositionCommand("IMEOn", PrecompositionState::IME_ON);
  RegisterPrecompositionCommand("InsertCharacter",
                                PrecompositionState::INSERT_CHARACTER);
  RegisterPrecompositionCommand("InsertSpace",
                                PrecompositionState::INSERT_SPACE);
  RegisterPrecompositionCommand("InsertAlternateSpace",
                                PrecompositionState::INSERT_ALTERNATE_SPACE);
  RegisterPrecompositionCommand("InsertHalfSpace",
                                PrecompositionState::INSERT_HALF_SPACE);
  RegisterPrecompositionCommand("InsertFullSpace",
                                PrecompositionState::INSERT_FULL_SPACE);
  RegisterPrecompositionCommand("ToggleAlphanumericMode",
                                PrecompositionState::TOGGLE_ALPHANUMERIC_MODE);
  if (kInputModeXCommandSupported) {
    RegisterPrecompositionCommand("InputModeHiragana",
                                  PrecompositionState::INPUT_MODE_HIRAGANA);
    RegisterPrecompositionCommand(
        "InputModeFullKatakana",
        PrecompositionState::INPUT_MODE_FULL_KATAKANA);
    RegisterPrecompositionCommand(
        "InputModeHalfKatakana",
        PrecompositionState::INPUT_MODE_HALF_KATAKANA);
    RegisterPrecompositionCommand(
        "InputModeFullAlphanumeric",
        PrecompositionState::INPUT_MODE_FULL_ALPHANUMERIC);
    RegisterPrecompositionCommand(
        "InputModeHalfAlphanumeric",
        PrecompositionState::INPUT_MODE_HALF_ALPHANUMERIC);
    RegisterPrecompositionCommand(
        "InputModeSwitchKanaType",
        PrecompositionState::INPUT_MODE_SWITCH_KANA_TYPE);
  } else {
    RegisterPrecompositionCommand("InputModeHiragana",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeFullKatakana",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeHalfKatakana",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeFullAlphanumeric",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeHalfAlphanumeric",
                                  PrecompositionState::NONE);
    RegisterPrecompositionCommand("InputModeSwitchKanaType",
                                  PrecompositionState::NONE);
  }

  RegisterPrecompositionCommand("LaunchConfigDialog",
                                PrecompositionState::LAUNCH_CONFIG_DIALOG);
  RegisterPrecompositionCommand("LaunchDictionaryTool",
                                PrecompositionState::LAUNCH_DICTIONARY_TOOL);
  RegisterPrecompositionCommand(
      "LaunchWordRegisterDialog",
      PrecompositionState::LAUNCH_WORD_REGISTER_DIALOG);

  RegisterPrecompositionCommand("Revert", PrecompositionState::REVERT);
  RegisterPrecompositionCommand("Undo", PrecompositionState::UNDO);
  RegisterPrecompositionCommand("Reconvert", PrecompositionState::RECONVERT);

  RegisterPrecompositionCommand("Cancel", PrecompositionState::CANCEL);
  RegisterPrecompositionCommand("CancelAndIMEOff",
                                PrecompositionState::CANCEL_AND_IME_OFF);
  RegisterPrecompositionCommand("CommitFirstSuggestion",
                                PrecompositionState::COMMIT_FIRST_SUGGESTION);
  RegisterPrecompositionCommand("PredictAndConvert",
                                PrecompositionState::PREDICT_AND_CONVERT);

  // Composition
  RegisterCompositionCommand("IMEOff", CompositionState::IME_OFF);
  RegisterCompositionCommand("IMEOn", CompositionState::IME_ON);
  RegisterCompositionCommand("InsertCharacter",
                             CompositionState::INSERT_CHARACTER);
  RegisterCompositionCommand("Delete", CompositionState::DEL);
  RegisterCompositionCommand("Backspace", CompositionState::BACKSPACE);
  RegisterCompositionCommand("InsertSpace",
                             CompositionState::INSERT_SPACE);
  RegisterCompositionCommand("InsertAlternateSpace",
                             CompositionState::INSERT_ALTERNATE_SPACE);
  RegisterCompositionCommand("InsertHalfSpace",
                             CompositionState::INSERT_HALF_SPACE);
  RegisterCompositionCommand("InsertFullSpace",
                             CompositionState::INSERT_FULL_SPACE);
  RegisterCompositionCommand("Cancel", CompositionState::CANCEL);
  RegisterCompositionCommand("CancelAndIMEOff",
                             CompositionState::CANCEL_AND_IME_OFF);
  RegisterCompositionCommand("Undo", CompositionState::UNDO);
  RegisterCompositionCommand("MoveCursorLeft",
                             CompositionState::MOVE_CURSOR_LEFT);
  RegisterCompositionCommand("MoveCursorRight",
                             CompositionState::MOVE_CURSOR_RIGHT);
  RegisterCompositionCommand("MoveCursorToBeginning",
                             CompositionState::MOVE_CURSOR_TO_BEGINNING);
  RegisterCompositionCommand("MoveCursorToEnd",
                             CompositionState::MOVE_MOVE_CURSOR_TO_END);
  RegisterCompositionCommand("Commit", CompositionState::COMMIT);
  RegisterCompositionCommand("CommitFirstSuggestion",
                             CompositionState::COMMIT_FIRST_SUGGESTION);
  RegisterCompositionCommand("Convert", CompositionState::CONVERT);
  RegisterCompositionCommand("ConvertWithoutHistory",
                             CompositionState::CONVERT_WITHOUT_HISTORY);
  RegisterCompositionCommand("PredictAndConvert",
                             CompositionState::PREDICT_AND_CONVERT);
  RegisterCompositionCommand("ConvertToHiragana",
                             CompositionState::CONVERT_TO_HIRAGANA);
  RegisterCompositionCommand("ConvertToFullKatakana",
                             CompositionState::CONVERT_TO_FULL_KATAKANA);
  RegisterCompositionCommand("ConvertToHalfKatakana",
                             CompositionState::CONVERT_TO_HALF_KATAKANA);
  RegisterCompositionCommand("ConvertToHalfWidth",
                             CompositionState::CONVERT_TO_HALF_WIDTH);
  RegisterCompositionCommand("ConvertToFullAlphanumeric",
                             CompositionState::CONVERT_TO_FULL_ALPHANUMERIC);
  RegisterCompositionCommand("ConvertToHalfAlphanumeric",
                             CompositionState::CONVERT_TO_HALF_ALPHANUMERIC);
  RegisterCompositionCommand("SwitchKanaType",
                             CompositionState::SWITCH_KANA_TYPE);
  RegisterCompositionCommand("DisplayAsHiragana",
                             CompositionState::DISPLAY_AS_HIRAGANA);
  RegisterCompositionCommand("DisplayAsFullKatakana",
                             CompositionState::DISPLAY_AS_FULL_KATAKANA);
  RegisterCompositionCommand("DisplayAsHalfKatakana",
                             CompositionState::DISPLAY_AS_HALF_KATAKANA);
  RegisterCompositionCommand("DisplayAsHalfWidth",
                             CompositionState::TRANSLATE_HALF_WIDTH);
  RegisterCompositionCommand("DisplayAsFullAlphanumeric",
                             CompositionState::TRANSLATE_FULL_ASCII);
  RegisterCompositionCommand("DisplayAsHalfAlphanumeric",
                             CompositionState::TRANSLATE_HALF_ASCII);
  RegisterCompositionCommand("ToggleAlphanumericMode",
                             CompositionState::TOGGLE_ALPHANUMERIC_MODE);
  if (kInputModeXCommandSupported) {
    RegisterCompositionCommand("InputModeHiragana",
                               CompositionState::INPUT_MODE_HIRAGANA);
    RegisterCompositionCommand("InputModeFullKatakana",
                               CompositionState::INPUT_MODE_FULL_KATAKANA);
    RegisterCompositionCommand("InputModeHalfKatakana",
                               CompositionState::INPUT_MODE_HALF_KATAKANA);
    RegisterCompositionCommand("InputModeFullAlphanumeric",
                               CompositionState::INPUT_MODE_FULL_ALPHANUMERIC);
    RegisterCompositionCommand("InputModeHalfAlphanumeric",
                               CompositionState::INPUT_MODE_HALF_ALPHANUMERIC);
  } else {
    RegisterCompositionCommand("InputModeHiragana",
                             CompositionState::NONE);
    RegisterCompositionCommand("InputModeFullKatakana",
                               CompositionState::NONE);
    RegisterCompositionCommand("InputModeHalfKatakana",
                               CompositionState::NONE);
    RegisterCompositionCommand("InputModeFullAlphanumeric",
                               CompositionState::NONE);
    RegisterCompositionCommand("InputModeHalfAlphanumeric",
                               CompositionState::NONE);
  }

  // Conversion
  RegisterConversionCommand("IMEOff", ConversionState::IME_OFF);
  RegisterConversionCommand("IMEOn", ConversionState::IME_ON);
  RegisterConversionCommand("InsertCharacter",
                            ConversionState::INSERT_CHARACTER);
  RegisterConversionCommand("InsertSpace",
                            ConversionState::INSERT_SPACE);
  RegisterConversionCommand("InsertAlternateSpace",
                            ConversionState::INSERT_ALTERNATE_SPACE);
  RegisterConversionCommand("InsertHalfSpace",
                            ConversionState::INSERT_HALF_SPACE);
  RegisterConversionCommand("InsertFullSpace",
                            ConversionState::INSERT_FULL_SPACE);
  RegisterConversionCommand("Cancel", ConversionState::CANCEL);
  RegisterConversionCommand("CancelAndIMEOff",
                            ConversionState::CANCEL_AND_IME_OFF);
  RegisterConversionCommand("Undo", ConversionState::UNDO);
  RegisterConversionCommand("SegmentFocusLeft",
                            ConversionState::SEGMENT_FOCUS_LEFT);
  RegisterConversionCommand("SegmentFocusRight",
                            ConversionState::SEGMENT_FOCUS_RIGHT);
  RegisterConversionCommand("SegmentFocusFirst",
                            ConversionState::SEGMENT_FOCUS_FIRST);
  RegisterConversionCommand("SegmentFocusLast",
                            ConversionState::SEGMENT_FOCUS_LAST);
  RegisterConversionCommand("SegmentWidthExpand",
                            ConversionState::SEGMENT_WIDTH_EXPAND);
  RegisterConversionCommand("SegmentWidthShrink",
                            ConversionState::SEGMENT_WIDTH_SHRINK);
  RegisterConversionCommand("ConvertNext", ConversionState::CONVERT_NEXT);
  RegisterConversionCommand("ConvertPrev", ConversionState::CONVERT_PREV);
  RegisterConversionCommand("ConvertNextPage",
                            ConversionState::CONVERT_NEXT_PAGE);
  RegisterConversionCommand("ConvertPrevPage",
                            ConversionState::CONVERT_PREV_PAGE);
  RegisterConversionCommand("PredictAndConvert",
                            ConversionState::PREDICT_AND_CONVERT);
  RegisterConversionCommand("Commit", ConversionState::COMMIT);
  RegisterConversionCommand("CommitOnlyFirstSegment",
                            ConversionState::COMMIT_SEGMENT);
  RegisterConversionCommand("ConvertToHiragana",
                            ConversionState::CONVERT_TO_HIRAGANA);
  RegisterConversionCommand("ConvertToFullKatakana",
                            ConversionState::CONVERT_TO_FULL_KATAKANA);
  RegisterConversionCommand("ConvertToHalfKatakana",
                            ConversionState::CONVERT_TO_HALF_KATAKANA);
  RegisterConversionCommand("ConvertToHalfWidth",
                            ConversionState::CONVERT_TO_HALF_WIDTH);
  RegisterConversionCommand("ConvertToFullAlphanumeric",
                            ConversionState::CONVERT_TO_FULL_ALPHANUMERIC);
  RegisterConversionCommand("ConvertToHalfAlphanumeric",
                            ConversionState::CONVERT_TO_HALF_ALPHANUMERIC);
  RegisterConversionCommand("SwitchKanaType",
                            ConversionState::SWITCH_KANA_TYPE);
  RegisterConversionCommand("ToggleAlphanumericMode",
                            ConversionState::TOGGLE_ALPHANUMERIC_MODE);
  RegisterConversionCommand("DisplayAsHiragana",
                            ConversionState::DISPLAY_AS_HIRAGANA);
  RegisterConversionCommand("DisplayAsFullKatakana",
                            ConversionState::DISPLAY_AS_FULL_KATAKANA);
  RegisterConversionCommand("DisplayAsHalfKatakana",
                            ConversionState::DISPLAY_AS_HALF_KATAKANA);
  RegisterConversionCommand("DisplayAsHalfWidth",
                            ConversionState::TRANSLATE_HALF_WIDTH);
  RegisterConversionCommand("DisplayAsFullAlphanumeric",
                            ConversionState::TRANSLATE_FULL_ASCII);
  RegisterConversionCommand("DisplayAsHalfAlphanumeric",
                            ConversionState::TRANSLATE_HALF_ASCII);
  RegisterConversionCommand("DeleteSelectedCandidate",
                            ConversionState::DELETE_SELECTED_CANDIDATE);
  if (kInputModeXCommandSupported) {
    RegisterConversionCommand("InputModeHiragana",
                              ConversionState::INPUT_MODE_HIRAGANA);
    RegisterConversionCommand("InputModeFullKatakana",
                              ConversionState::INPUT_MODE_FULL_KATAKANA);
    RegisterConversionCommand("InputModeHalfKatakana",
                              ConversionState::INPUT_MODE_HALF_KATAKANA);
    RegisterConversionCommand("InputModeFullAlphanumeric",
                              ConversionState::INPUT_MODE_FULL_ALPHANUMERIC);
    RegisterConversionCommand("InputModeHalfAlphanumeric",
                              ConversionState::INPUT_MODE_HALF_ALPHANUMERIC);
  } else {
    RegisterConversionCommand("InputModeHiragana",
                              ConversionState::NONE);
    RegisterConversionCommand("InputModeFullKatakana",
                              ConversionState::NONE);
    RegisterConversionCommand("InputModeHalfKatakana",
                              ConversionState::NONE);
    RegisterConversionCommand("InputModeFullAlphanumeric",
                              ConversionState::NONE);
    RegisterConversionCommand("InputModeHalfAlphanumeric",
                              ConversionState::NONE);
  }
#ifndef NO_LOGGING  // means NOT RELEASE build
  RegisterConversionCommand("ReportBug", ConversionState::REPORT_BUG);
#endif  // NO_LOGGING
}

bool KeyMapManager::GetCommandDirect(
    const commands::KeyEvent &key_event,
    DirectInputState::Commands *command) const {
  return keymap_direct_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandPrecomposition(
    const commands::KeyEvent &key_event,
    PrecompositionState::Commands *command) const {
  return keymap_precomposition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandComposition(
    const commands::KeyEvent &key_event,
    CompositionState::Commands *command) const {
  return keymap_composition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandZeroQuerySuggestion(
    const commands::KeyEvent &key_event,
    PrecompositionState::Commands *command) const {
  // try zero query suggestion rule first
  if (keymap_zero_query_suggestion_.GetCommand(key_event, command)) {
    return true;
  }
  // use precomposition rule
  return keymap_precomposition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandSuggestion(
    const commands::KeyEvent &key_event,
    CompositionState::Commands *command) const {
  // try suggestion rule first
  if (keymap_suggestion_.GetCommand(key_event, command)) {
    return true;
  }
  // use composition rule
  return keymap_composition_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandConversion(
    const commands::KeyEvent &key_event,
    ConversionState::Commands *command) const {
  return keymap_conversion_.GetCommand(key_event, command);
}

bool KeyMapManager::GetCommandPrediction(
    const commands::KeyEvent &key_event,
    ConversionState::Commands *command) const {
  // try prediction rule first
  if (keymap_prediction_.GetCommand(key_event, command)) {
    return true;
  }
  // use conversion rule
  return keymap_conversion_.GetCommand(key_event, command);
}

bool KeyMapManager::ParseCommandDirect(
    const string &command_string,
    DirectInputState::Commands *command) const {
  if (command_direct_map_.count(command_string) == 0) {
    return false;
  }
  *command = command_direct_map_.find(command_string)->second;
  return true;
}

// This should be in KeyMap instead of KeyMapManager.
bool KeyMapManager::ParseCommandPrecomposition(
    const string &command_string,
    PrecompositionState::Commands *command) const {
  if (command_precomposition_map_.count(command_string) == 0) {
    return false;
  }
  *command = command_precomposition_map_.find(command_string)->second;
  return true;
}


bool KeyMapManager::ParseCommandComposition(
    const string &command_string,
    CompositionState::Commands *command) const {
  if (command_composition_map_.count(command_string) == 0) {
    return false;
  }
  *command = command_composition_map_.find(command_string)->second;
  return true;
}

bool KeyMapManager::ParseCommandConversion(
    const string &command_string,
    ConversionState::Commands *command) const {
  if (command_conversion_map_.count(command_string) == 0) {
    return false;
  }
  *command = command_conversion_map_.find(command_string)->second;
  return true;
}

void KeyMapManager::GetAvailableCommandNameDirect(
    set<string> *command_names) const {
  DCHECK(command_names);
  for (map<string, DirectInputState::Commands>::const_iterator iter
           = command_direct_map_.begin();
       iter != command_direct_map_.end(); ++iter) {
    command_names->insert(iter->first);
  }
}

void KeyMapManager::GetAvailableCommandNamePrecomposition(
    set<string> *command_names) const {
  DCHECK(command_names);
  for (map<string, PrecompositionState::Commands>::const_iterator iter
           = command_precomposition_map_.begin();
       iter != command_precomposition_map_.end(); ++iter) {
    command_names->insert(iter->first);
  }
}

void KeyMapManager::GetAvailableCommandNameComposition(
    set<string> *command_names) const {
  DCHECK(command_names);
  for (map<string, CompositionState::Commands>::const_iterator iter
           = command_composition_map_.begin();
       iter != command_composition_map_.end(); ++iter) {
    command_names->insert(iter->first);
  }
}

void KeyMapManager::GetAvailableCommandNameConversion(
    set<string> *command_names) const {
  DCHECK(command_names);
  for (map<string, ConversionState::Commands>::const_iterator iter
           = command_conversion_map_.begin();
       iter != command_conversion_map_.end(); ++iter) {
    command_names->insert(iter->first);
  }
}

void KeyMapManager::GetAvailableCommandNameZeroQuerySuggestion(
    set<string> *command_names) const {
  GetAvailableCommandNamePrecomposition(command_names);
}

void KeyMapManager::GetAvailableCommandNameSuggestion(
    set<string> *command_names) const {
  GetAvailableCommandNameComposition(command_names);
}

void KeyMapManager::GetAvailableCommandNamePrediction(
    set<string> *command_names) const {
  GetAvailableCommandNameConversion(command_names);
}

}  // namespace keymap
}  // namespace mozc
