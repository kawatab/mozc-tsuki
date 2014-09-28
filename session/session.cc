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

// Session class of Mozc server.

#include "session/session.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/port.h"
#include "base/process.h"
#include "base/singleton.h"
#include "base/url.h"
#include "base/util.h"
#include "base/version.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "engine/engine_interface.h"
#include "engine/user_data_manager_interface.h"
#include "session/commands.pb.h"
#include "session/internal/ime_context.h"
#include "session/internal/key_event_transformer.h"
#include "session/internal/keymap-inl.h"
#include "session/internal/keymap.h"
#include "session/internal/keymap_factory.h"
#include "session/internal/session_output.h"
#include "session/key_event_util.h"
#include "session/session_converter.h"
#include "session/session_usage_stats_util.h"
#include "usage_stats/usage_stats.h"

using mozc::usage_stats::UsageStats;

namespace mozc {
namespace session {

namespace {

// Set input mode if the current input mode is not the given mode.
void SwitchInputMode(const transliteration::TransliterationType mode,
                     composer::Composer *composer) {
  if (composer->GetInputMode() != mode) {
    composer->SetInputMode(mode);
  }
  composer->SetNewInput();
}

// Set input mode to the |composer| if the the input mode of |composer| is not
// the given |mode|.
void ApplyInputMode(const commands::CompositionMode mode,
                    composer::Composer *composer) {
  switch (mode) {
    case commands::HIRAGANA:
      SwitchInputMode(transliteration::HIRAGANA, composer);
      break;
    case commands::FULL_KATAKANA:
      SwitchInputMode(transliteration::FULL_KATAKANA, composer);
      break;
    case commands::HALF_KATAKANA:
      SwitchInputMode(transliteration::HALF_KATAKANA, composer);
      break;
    case commands::FULL_ASCII:
      SwitchInputMode(transliteration::FULL_ASCII, composer);
      break;
    case commands::HALF_ASCII:
      SwitchInputMode(transliteration::HALF_ASCII, composer);
      break;
    default:
      LOG(DFATAL) << "ime on with invalid mode";
  }
}

// Return true if the specified key event consists of any modifier key only.
bool IsPureModifierKeyEvent(const commands::KeyEvent &key) {
  if (key.has_key_code()) {
    return false;
  }
  if (key.has_special_key()) {
    return false;
  }
  if (key.modifier_keys_size() == 0) {
    return false;
  }
  return true;
}

bool IsPureSpaceKey(const commands::KeyEvent &key) {
  if (key.has_key_code()) {
    return false;
  }
  if (key.modifier_keys_size() > 0) {
    return false;
  }
  if (!key.has_special_key()) {
    return false;
  }
  if (key.special_key() != commands::KeyEvent::SPACE) {
    return false;
  }
  return true;
}

// Set session state to the given state and also update related status.
void SetSessionState(const ImeContext::State state, ImeContext *context) {
  const ImeContext::State prev_state = context->state();
  context->set_state(state);
  switch (state) {
    case ImeContext::DIRECT:
    case ImeContext::PRECOMPOSITION:
      context->mutable_composer()->Reset();
      break;
    case ImeContext::CONVERSION:
      context->mutable_composer()->ResetInputMode();
      break;
    case ImeContext::COMPOSITION:
      if (prev_state == ImeContext::PRECOMPOSITION) {
        // NOTE: In case of state change including commitment, state change
        // doesn't happen directly at once from CONVERSION to COMPOSITION.
        // Actual state change is CONVERSION to PRECOMPOSITION at first,
        // followed by PRECOMPOSITION to COMPOSITION.
        // However in this case we can only get one SendCaretRectangle
        // because the state change is executed atomically.
        context->mutable_composition_rectangle()->CopyFrom(
            context->caret_rectangle());
        // Notify the start of composition to the converter so that internal
        // state can be refreshed by the client context (especially by
        // preceding text).
        context->mutable_converter()->OnStartComposition(
            context->client_context());
      }
      break;
    default:
      // Do nothing.
      break;
  }
}

commands::CompositionMode ToCompositionMode(
  mozc::transliteration::TransliterationType type) {
    commands::CompositionMode mode = commands::HIRAGANA;
    switch (type) {
    case transliteration::HIRAGANA:
      mode = commands::HIRAGANA;
      break;
    case transliteration::FULL_KATAKANA:
      mode = commands::FULL_KATAKANA;
      break;
    case transliteration::HALF_KATAKANA:
      mode = commands::HALF_KATAKANA;
      break;
    case transliteration::FULL_ASCII:
      mode = commands::FULL_ASCII;
      break;
    case transliteration::HALF_ASCII:
      mode = commands::HALF_ASCII;
      break;
    default:
      LOG(ERROR) << "Unknown input mode: " << type;
      // use HIRAGANA as a default.
    }
    return mode;
}

ImeContext::State GetEffectiveStateForTestSendKey(
    const commands::KeyEvent &key,
    ImeContext::State state) {
  if (!key.has_activated()) {
    return state;
  }
  if (state == ImeContext::DIRECT && key.activated()) {
    // Indirect IME On found.
    return ImeContext::PRECOMPOSITION;
  }
  if (state != ImeContext::DIRECT && !key.activated()) {
    // Indirect IME Off found.
    return ImeContext::DIRECT;
  }
  return state;
}

}  // namespace

// TODO(komatsu): Remove these argument by using/making singletons.
Session::Session(EngineInterface *engine)
    : engine_(engine), context_(new ImeContext) {
  InitContext(context_.get());
}

Session::~Session() {}

void Session::InitContext(ImeContext *context) const {
  context->set_create_time(Util::GetTime());
  context->set_last_command_time(0);
  context->set_composer(new composer::Composer(NULL, &context->GetRequest()));
  context->set_converter(
      new SessionConverter(engine_->GetConverter(), &context->GetRequest()));
#ifdef OS_WIN
  // On Windows session is started with direct mode.
  // FIXME(toshiyuki): Ditto for Mac after verifying on Mac.
  context->set_state(ImeContext::DIRECT);
#else
  context->set_state(ImeContext::PRECOMPOSITION);
#endif
  context->mutable_client_context()->Clear();

  UpdateConfig(config::ConfigHandler::GetConfig(), context);
}


void Session::PushUndoContext() {
  // TODO(komatsu): Support multiple undo.
  prev_context_.reset(new ImeContext);
  InitContext(prev_context_.get());
  ImeContext::CopyContext(*context_, prev_context_.get());
}

void Session::PopUndoContext() {
  // TODO(komatsu): Support multiple undo.
  if (!prev_context_.get()) {
    return;
  }
  context_.swap(prev_context_);
  prev_context_.reset(NULL);
}

void Session::ClearUndoContext() {
  prev_context_.reset(NULL);
}

void Session::EnsureIMEIsOn() {
  if (context_->state() == ImeContext::DIRECT) {
    SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
  }
}

bool Session::SendCommand(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  if (!command->input().has_command()) {
    return false;
  }
  TransformInput(command->mutable_input());

  SessionUsageStatsUtil::AddSendCommandInputStats(command->input());

  const commands::SessionCommand &session_command = command->input().command();
  bool result = false;
  if (session_command.type() == commands::SessionCommand::SWITCH_INPUT_MODE) {
    if (!session_command.has_composition_mode()) {
      return false;
    }
    switch (session_command.composition_mode()) {
      case commands::DIRECT:
        // TODO(komatsu): Implement here.
        break;
      case commands::HIRAGANA:
        result = InputModeHiragana(command);
        break;
      case commands::FULL_KATAKANA:
        result = InputModeFullKatakana(command);
        break;
      case commands::HALF_ASCII:
        result = InputModeHalfASCII(command);
        break;
      case commands::FULL_ASCII:
        result = InputModeFullASCII(command);
        break;
      case commands::HALF_KATAKANA:
        result = InputModeHalfKatakana(command);
        break;
      default:
        LOG(ERROR) << "Unknown mode: " << session_command.composition_mode();
        break;
    }
    return result;
  }

  DCHECK_EQ(false, result);
  switch (command->input().command().type()) {
    case commands::SessionCommand::REVERT:
      result = Revert(command);
      break;
    case commands::SessionCommand::SUBMIT:
      result = Commit(command);
      break;
    case commands::SessionCommand::SELECT_CANDIDATE:
      result = SelectCandidate(command);
      break;
    case commands::SessionCommand::SUBMIT_CANDIDATE:
      result = CommitCandidate(command);
      break;
    case commands::SessionCommand::HIGHLIGHT_CANDIDATE:
      result = HighlightCandidate(command);
      break;
    case commands::SessionCommand::GET_STATUS:
      result = GetStatus(command);
      break;
    case commands::SessionCommand::CONVERT_REVERSE:
      result = ConvertReverse(command);
      break;
    case commands::SessionCommand::UNDO:
      result = Undo(command);
      break;
    case commands::SessionCommand::RESET_CONTEXT:
      result = ResetContext(command);
      break;
    case commands::SessionCommand::MOVE_CURSOR:
      result = MoveCursorTo(command);
      break;
    case commands::SessionCommand::EXPAND_SUGGESTION:
      result = ExpandSuggestion(command);
      break;
    case commands::SessionCommand::SWITCH_INPUT_FIELD_TYPE:
      result = SwitchInputFieldType(command);
      break;
    case commands::SessionCommand::USAGE_STATS_EVENT:
      // Set consumed to false, because the client don't need to do anything
      // when it receive the output from the server.
      command->mutable_output()->set_consumed(false);
      result = true;
      break;
    case commands::SessionCommand::UNDO_OR_REWIND:
      result = UndoOrRewind(command);
      break;
    case commands::SessionCommand::SEND_CARET_LOCATION:
      result = SetCaretLocation(command);
      break;
    case commands::SessionCommand::COMMIT_RAW_TEXT:
      result = CommitRawText(command);
      break;
    case commands::SessionCommand::CONVERT_PREV_PAGE:
      result = ConvertPrevPage(command);
      break;
    case commands::SessionCommand::CONVERT_NEXT_PAGE:
      result = ConvertNextPage(command);
      break;
    case commands::SessionCommand::TURN_ON_IME:
      result = MakeSureIMEOn(command);
      break;
    case commands::SessionCommand::TURN_OFF_IME:
      result = MakeSureIMEOff(command);
      break;
    default:
      LOG(WARNING) << "Unknown command" << command->DebugString();
      result = DoNothing(command);
      break;
  }

  return result;
}

bool Session::TestSendKey(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  TransformInput(command->mutable_input());

  if (context_->state() == ImeContext::NONE) {
    // This must be an error.
    LOG(ERROR) << "Invalid state: NONE";
    return false;
  }

  const commands::KeyEvent &key = command->input().key();

  // To support indirect IME on/off by using KeyEvent::activated, use effective
  // state instead of directly using context_->state().
  const ImeContext::State state = GetEffectiveStateForTestSendKey(
      key, context_->state());

  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());

  // Direct input
  if (state == ImeContext::DIRECT) {
    keymap::DirectInputState::Commands key_command;
    if (!keymap->GetCommandDirect(key, &key_command) ||
        key_command == keymap::DirectInputState::NONE) {
      return EchoBack(command);
    }
    return DoNothing(command);
  }

  // Precomposition
  if (state == ImeContext::PRECOMPOSITION) {
    keymap::PrecompositionState::Commands key_command;
    const bool is_suggestion =
        context_->converter().CheckState(SessionConverterInterface::SUGGESTION);
    const bool result = is_suggestion
        ? keymap->GetCommandZeroQuerySuggestion(key, &key_command)
        : keymap->GetCommandPrecomposition(key, &key_command);
    if (!result || key_command == keymap::PrecompositionState::NONE) {
      // Clear undo context just in case. b/5529702.
      // Note that the undo context will not be cleared in
      // EchoBackAndClearUndoContext if the key event consists of modifier keys
      // only.
      return EchoBackAndClearUndoContext(command);
    }
    // If the input_style is DIRECT_INPUT, KeyEvent is not consumed
    // and done echo back.  It works only when key_string is equal to
    // key_code.  We should fix this limitation when the as_is flag is
    // used for rather than numpad characters.
    if (key_command == keymap::PrecompositionState::INSERT_CHARACTER &&
        key.input_style() == commands::KeyEvent::DIRECT_INPUT) {
      return EchoBack(command);
    }

    // TODO(komatsu): This is a hack to work around the problem with
    // the inconsistency between TestSendKey and SendKey.
    switch (key_command) {
      case keymap::PrecompositionState::INSERT_SPACE:
        if (!IsFullWidthInsertSpace(command->input()) && IsPureSpaceKey(key)) {
          return EchoBackAndClearUndoContext(command);
        }
        return DoNothing(command);
      case keymap::PrecompositionState::INSERT_ALTERNATE_SPACE:
        if (IsFullWidthInsertSpace(command->input()) && IsPureSpaceKey(key)) {
          return EchoBackAndClearUndoContext(command);
        }
        return DoNothing(command);
      case keymap::PrecompositionState::INSERT_HALF_SPACE:
        if (IsPureSpaceKey(key)) {
          return EchoBackAndClearUndoContext(command);
        }
        return DoNothing(command);
      case keymap::PrecompositionState::INSERT_FULL_SPACE:
        return DoNothing(command);
      default:
        // Do nothing.
        break;
    }

    if (key_command == keymap::PrecompositionState::REVERT) {
      return Revert(command);
    }

    // If undo context is empty, echoes back the key event so that it can be
    // handled by the application. b/5553298
    if (key_command == keymap::PrecompositionState::UNDO &&
        !prev_context_.get()) {
      return EchoBack(command);
    }

    return DoNothing(command);
  }

  // Do nothing.
  return DoNothing(command);
}

bool Session::SendKey(commands::Command *command) {
  UpdateTime();
  UpdatePreferences(command);
  TransformInput(command->mutable_input());
  // To support indirect IME on/off by using KeyEvent::activated, use effective
  // state instead of directly using context_->state().
  HandleIndirectImeOnOff(command);

  SessionUsageStatsUtil::AddSendKeyInputStats(command->input());

  bool result = false;
  switch (context_->state()) {
    case ImeContext::DIRECT:
      result = SendKeyDirectInputState(command);
      break;

    case ImeContext::PRECOMPOSITION:
      result = SendKeyPrecompositionState(command);
      break;

    case ImeContext::COMPOSITION:
      result = SendKeyCompositionState(command);
      break;

    case ImeContext::CONVERSION:
      result = SendKeyConversionState(command);
      break;

    case ImeContext::NONE:
      result = false;
      break;
  }

  SessionUsageStatsUtil::AddSendKeyOutputStats(command->output());

  return result;
}

bool Session::SendKeyDirectInputState(commands::Command *command) {
  keymap::DirectInputState::Commands key_command;
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  if (!keymap->GetCommandDirect(command->input().key(), &key_command)) {
    return EchoBackAndClearUndoContext(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandDirect(key_command, &command_name)) {
    UsageStats::IncrementCount("Performed_Direct_" + command_name);
  }
  switch (key_command) {
    case keymap::DirectInputState::IME_ON:
      return IMEOn(command);
    case keymap::DirectInputState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);
    case keymap::DirectInputState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);
    case keymap::DirectInputState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);
    case keymap::DirectInputState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);
    case keymap::DirectInputState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);
    case keymap::DirectInputState::NONE:
      return EchoBackAndClearUndoContext(command);
    case keymap::DirectInputState::RECONVERT:
      return RequestConvertReverse(command);
  }
  return false;
}

bool Session::SendKeyPrecompositionState(commands::Command *command) {
  keymap::PrecompositionState::Commands key_command;
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  const bool result =
      context_->converter().CheckState(SessionConverterInterface::SUGGESTION) ?
      keymap->GetCommandZeroQuerySuggestion(command->input().key(),
                                            &key_command) :
      keymap->GetCommandPrecomposition(command->input().key(), &key_command);

  if (!result) {
    return EchoBackAndClearUndoContext(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandPrecomposition(key_command, &command_name)) {
    UsageStats::IncrementCount("Performed_Precomposition_" + command_name);
  }

  // Update the client context (if any) for later use. Note that the client
  // context is updated only here. In other words, we will stop updating the
  // client context once a conversion starts (mainly for performance reasons).
  if (command->has_input() && command->input().has_context()) {
    context_->mutable_client_context()->CopyFrom(
        command->input().context());
  } else {
    context_->mutable_client_context()->Clear();
  }

  switch (key_command) {
    case keymap::PrecompositionState::INSERT_CHARACTER:
      return InsertCharacter(command);
    case keymap::PrecompositionState::INSERT_SPACE:
      return InsertSpace(command);
    case keymap::PrecompositionState::INSERT_ALTERNATE_SPACE:
      return InsertSpaceToggled(command);
    case keymap::PrecompositionState::INSERT_HALF_SPACE:
      return InsertSpaceHalfWidth(command);
    case keymap::PrecompositionState::INSERT_FULL_SPACE:
      return InsertSpaceFullWidth(command);
    case keymap::PrecompositionState::TOGGLE_ALPHANUMERIC_MODE:
      return ToggleAlphanumericMode(command);
    case keymap::PrecompositionState::REVERT:
      return Revert(command);
    case keymap::PrecompositionState::UNDO:
      return RequestUndo(command);
    case keymap::PrecompositionState::IME_OFF:
      return IMEOff(command);
    case keymap::PrecompositionState::IME_ON:
      return DoNothing(command);

    case keymap::PrecompositionState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);
    case keymap::PrecompositionState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);
    case keymap::PrecompositionState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);
    case keymap::PrecompositionState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);
    case keymap::PrecompositionState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);
    case keymap::PrecompositionState::INPUT_MODE_SWITCH_KANA_TYPE:
      return InputModeSwitchKanaType(command);

    case keymap::PrecompositionState::LAUNCH_CONFIG_DIALOG:
      return LaunchConfigDialog(command);
    case keymap::PrecompositionState::LAUNCH_DICTIONARY_TOOL:
      return LaunchDictionaryTool(command);
    case keymap::PrecompositionState::LAUNCH_WORD_REGISTER_DIALOG:
      return LaunchWordRegisterDialog(command);

    // For zero query suggestion
    case keymap::PrecompositionState::CANCEL:
      // It is a little kind of abuse of the EditCancel command.  It
      // would be nice to make a new command when EditCancel is
      // extended or the requirement of this command is added.
      return EditCancel(command);
    case keymap::PrecompositionState::CANCEL_AND_IME_OFF:
      // The same to keymap::PrecompositionState::CANCEL.
      return EditCancelAndIMEOff(command);
    // For zero query suggestion
    case keymap::PrecompositionState::COMMIT_FIRST_SUGGESTION:
      return CommitFirstSuggestion(command);
    // For zero query suggestion
    case keymap::PrecompositionState::PREDICT_AND_CONVERT:
      return PredictAndConvert(command);

    case keymap::PrecompositionState::NONE:
      return EchoBackAndClearUndoContext(command);
    case keymap::PrecompositionState::RECONVERT:
      return RequestConvertReverse(command);
  }
  return false;
}

bool Session::SendKeyCompositionState(commands::Command *command) {
  keymap::CompositionState::Commands key_command;
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  const bool result =
      context_->converter().CheckState(SessionConverterInterface::SUGGESTION) ?
      keymap->GetCommandSuggestion(command->input().key(), &key_command) :
      keymap->GetCommandComposition(command->input().key(), &key_command);

  if (!result) {
    return DoNothing(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandComposition(key_command, &command_name)) {
    UsageStats::IncrementCount("Performed_Composition_" + command_name);
  }
  switch (key_command) {
    case keymap::CompositionState::INSERT_CHARACTER:
      return InsertCharacter(command);

    case keymap::CompositionState::COMMIT:
      return Commit(command);

    case keymap::CompositionState::COMMIT_FIRST_SUGGESTION:
      return CommitFirstSuggestion(command);

    case keymap::CompositionState::CONVERT:
      return Convert(command);

    case keymap::CompositionState::CONVERT_WITHOUT_HISTORY:
      return ConvertWithoutHistory(command);

    case keymap::CompositionState::PREDICT_AND_CONVERT:
      return PredictAndConvert(command);

    case keymap::CompositionState::DEL:
      return Delete(command);

    case keymap::CompositionState::BACKSPACE:
      return Backspace(command);

    case keymap::CompositionState::INSERT_SPACE:
      return InsertSpace(command);

    case keymap::CompositionState::INSERT_ALTERNATE_SPACE:
      return InsertSpaceToggled(command);

    case keymap::CompositionState::INSERT_HALF_SPACE:
      return InsertSpaceHalfWidth(command);

    case keymap::CompositionState::INSERT_FULL_SPACE:
      return InsertSpaceFullWidth(command);

    case keymap::CompositionState::MOVE_CURSOR_LEFT:
      return MoveCursorLeft(command);

    case keymap::CompositionState::MOVE_CURSOR_RIGHT:
      return MoveCursorRight(command);

    case keymap::CompositionState::MOVE_CURSOR_TO_BEGINNING:
      return MoveCursorToBeginning(command);

    case keymap::CompositionState::MOVE_MOVE_CURSOR_TO_END:
      return MoveCursorToEnd(command);

    case keymap::CompositionState::CANCEL:
      return EditCancel(command);

    case keymap::CompositionState::CANCEL_AND_IME_OFF:
      return EditCancelAndIMEOff(command);

    case keymap::CompositionState::UNDO:
      return RequestUndo(command);

    case keymap::CompositionState::IME_OFF:
      return IMEOff(command);

    case keymap::CompositionState::IME_ON:
      return DoNothing(command);

    case keymap::CompositionState::CONVERT_TO_HIRAGANA:
      return ConvertToHiragana(command);

    case keymap::CompositionState::CONVERT_TO_FULL_KATAKANA:
      return ConvertToFullKatakana(command);

    case keymap::CompositionState::CONVERT_TO_HALF_KATAKANA:
      return ConvertToHalfKatakana(command);

    case keymap::CompositionState::CONVERT_TO_HALF_WIDTH:
      return ConvertToHalfWidth(command);

    case keymap::CompositionState::CONVERT_TO_FULL_ALPHANUMERIC:
      return ConvertToFullASCII(command);

    case keymap::CompositionState::CONVERT_TO_HALF_ALPHANUMERIC:
      return ConvertToHalfASCII(command);

    case keymap::CompositionState::SWITCH_KANA_TYPE:
      return SwitchKanaType(command);

    case keymap::CompositionState::DISPLAY_AS_HIRAGANA:
      return DisplayAsHiragana(command);

    case keymap::CompositionState::DISPLAY_AS_FULL_KATAKANA:
      return DisplayAsFullKatakana(command);

    case keymap::CompositionState::DISPLAY_AS_HALF_KATAKANA:
      return DisplayAsHalfKatakana(command);

    case keymap::CompositionState::TRANSLATE_HALF_WIDTH:
      return TranslateHalfWidth(command);

    case keymap::CompositionState::TRANSLATE_FULL_ASCII:
      return TranslateFullASCII(command);

    case keymap::CompositionState::TRANSLATE_HALF_ASCII:
      return TranslateHalfASCII(command);

    case keymap::CompositionState::TOGGLE_ALPHANUMERIC_MODE:
      return ToggleAlphanumericMode(command);

    case keymap::CompositionState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);

    case keymap::CompositionState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);

    case keymap::CompositionState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);

    case keymap::CompositionState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);

    case keymap::CompositionState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);

    case keymap::CompositionState::NONE:
      return DoNothing(command);
  }
  return false;
}

bool Session::SendKeyConversionState(commands::Command *command) {
  keymap::ConversionState::Commands key_command;
  const keymap::KeyMapManager *keymap =
      keymap::KeyMapFactory::GetKeyMapManager(context_->keymap());
  const bool result =
      context_->converter().CheckState(SessionConverterInterface::PREDICTION) ?
      keymap->GetCommandPrediction(command->input().key(), &key_command) :
      keymap->GetCommandConversion(command->input().key(), &key_command);

  if (!result) {
    return DoNothing(command);
  }
  string command_name;
  if (keymap->GetNameFromCommandConversion(key_command,
                                           &command_name)) {
    UsageStats::IncrementCount("Performed_Conversion_" + command_name);
  }
  switch (key_command) {
    case keymap::ConversionState::INSERT_CHARACTER:
      return InsertCharacter(command);

    case keymap::ConversionState::INSERT_SPACE:
      return InsertSpace(command);

    case keymap::ConversionState::INSERT_ALTERNATE_SPACE:
      return InsertSpaceToggled(command);

    case keymap::ConversionState::INSERT_HALF_SPACE:
      return InsertSpaceHalfWidth(command);

    case keymap::ConversionState::INSERT_FULL_SPACE:
      return InsertSpaceFullWidth(command);

    case keymap::ConversionState::COMMIT:
      return Commit(command);

    case keymap::ConversionState::COMMIT_SEGMENT:
      return CommitSegment(command);

    case keymap::ConversionState::CONVERT_NEXT:
      return ConvertNext(command);

    case keymap::ConversionState::CONVERT_PREV:
      return ConvertPrev(command);

    case keymap::ConversionState::CONVERT_NEXT_PAGE:
      return ConvertNextPage(command);

    case keymap::ConversionState::CONVERT_PREV_PAGE:
      return ConvertPrevPage(command);

    case keymap::ConversionState::PREDICT_AND_CONVERT:
      return PredictAndConvert(command);

    case keymap::ConversionState::SEGMENT_FOCUS_LEFT:
      return SegmentFocusLeft(command);

    case keymap::ConversionState::SEGMENT_FOCUS_RIGHT:
      return SegmentFocusRight(command);

    case keymap::ConversionState::SEGMENT_FOCUS_FIRST:
      return SegmentFocusLeftEdge(command);

    case keymap::ConversionState::SEGMENT_FOCUS_LAST:
      return SegmentFocusLast(command);

    case keymap::ConversionState::SEGMENT_WIDTH_EXPAND:
      return SegmentWidthExpand(command);

    case keymap::ConversionState::SEGMENT_WIDTH_SHRINK:
      return SegmentWidthShrink(command);

    case keymap::ConversionState::CANCEL:
      return ConvertCancel(command);

    case keymap::ConversionState::CANCEL_AND_IME_OFF:
      return EditCancelAndIMEOff(command);

    case keymap::ConversionState::UNDO:
      return RequestUndo(command);

    case keymap::ConversionState::IME_OFF:
      return IMEOff(command);

    case keymap::ConversionState::IME_ON:
      return DoNothing(command);

    case keymap::ConversionState::CONVERT_TO_HIRAGANA:
      return ConvertToHiragana(command);

    case keymap::ConversionState::CONVERT_TO_FULL_KATAKANA:
      return ConvertToFullKatakana(command);

    case keymap::ConversionState::CONVERT_TO_HALF_KATAKANA:
      return ConvertToHalfKatakana(command);

    case keymap::ConversionState::CONVERT_TO_HALF_WIDTH:
      return ConvertToHalfWidth(command);

    case keymap::ConversionState::CONVERT_TO_FULL_ALPHANUMERIC:
      return ConvertToFullASCII(command);

    case keymap::ConversionState::CONVERT_TO_HALF_ALPHANUMERIC:
      return ConvertToHalfASCII(command);

    case keymap::ConversionState::SWITCH_KANA_TYPE:
      return SwitchKanaType(command);

    case keymap::ConversionState::DISPLAY_AS_HIRAGANA:
      return DisplayAsHiragana(command);

    case keymap::ConversionState::DISPLAY_AS_FULL_KATAKANA:
      return DisplayAsFullKatakana(command);

    case keymap::ConversionState::DISPLAY_AS_HALF_KATAKANA:
      return DisplayAsHalfKatakana(command);

    case keymap::ConversionState::TRANSLATE_HALF_WIDTH:
      return TranslateHalfWidth(command);

    case keymap::ConversionState::TRANSLATE_FULL_ASCII:
      return TranslateFullASCII(command);

    case keymap::ConversionState::TRANSLATE_HALF_ASCII:
      return TranslateHalfASCII(command);

    case keymap::ConversionState::TOGGLE_ALPHANUMERIC_MODE:
      return ToggleAlphanumericMode(command);

    case keymap::ConversionState::INPUT_MODE_HIRAGANA:
      return InputModeHiragana(command);

    case keymap::ConversionState::INPUT_MODE_FULL_KATAKANA:
      return InputModeFullKatakana(command);

    case keymap::ConversionState::INPUT_MODE_HALF_KATAKANA:
      return InputModeHalfKatakana(command);

    case keymap::ConversionState::INPUT_MODE_FULL_ALPHANUMERIC:
      return InputModeFullASCII(command);

    case keymap::ConversionState::INPUT_MODE_HALF_ALPHANUMERIC:
      return InputModeHalfASCII(command);

    case keymap::ConversionState::REPORT_BUG:
      return ReportBug(command);

    case keymap::ConversionState::DELETE_SELECTED_CANDIDATE:
      return DeleteSelectedCandidateFromHistory(command);

    case keymap::ConversionState::NONE:
      return DoNothing(command);
  }
  return false;
}

void Session::UpdatePreferences(commands::Command *command) {
  DCHECK(command);

  const config::Config &config = command->input().config();
  if (config.has_session_keymap()) {
    context_->set_keymap(config.session_keymap());
  } else {
    context_->set_keymap(GET_CONFIG(session_keymap));
  }

  if (command->input().has_capability()) {
    context_->mutable_client_capability()->CopyFrom(
        command->input().capability());
  }

  UpdateOperationPreferences(config, context_.get());
}

bool Session::IMEOn(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
  if (command->input().has_key() && command->input().key().has_mode()) {
    ApplyInputMode(
        command->input().key().mode(), context_->mutable_composer());
  }
  OutputMode(command);
  return true;
}

bool Session::IMEOff(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  Commit(command);

  // Reset the context.
  context_->mutable_converter()->Reset();

  SetSessionState(ImeContext::DIRECT, context_.get());
  OutputMode(command);
  return true;
}

bool Session::MakeSureIMEOn(mozc::commands::Command *command) {
  if (command->input().has_command() &&
      command->input().command().has_composition_mode() &&
      (command->input().command().composition_mode() == commands::DIRECT)) {
    // This is invalid and unsupported usage.
    return false;
  }

  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::DIRECT) {
    ClearUndoContext();
    SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
  }
  if (command->input().has_command() &&
      command->input().command().has_composition_mode()) {
    ApplyInputMode(command->input().command().composition_mode(),
                   context_->mutable_composer());
  }
  OutputMode(command);
  return true;
}

bool Session::MakeSureIMEOff(mozc::commands::Command *command) {
  if (command->input().has_command() &&
      command->input().command().has_composition_mode() &&
      (command->input().command().composition_mode() == commands::DIRECT)) {
    // This is invalid and unsupported usage.
    return false;
  }

  command->mutable_output()->set_consumed(true);
  if (context_->state() != ImeContext::DIRECT) {
    ClearUndoContext();
    Commit(command);
    // Reset the context.
    context_->mutable_converter()->Reset();
    SetSessionState(ImeContext::DIRECT, context_.get());
  }
  if (command->input().has_command() &&
      command->input().command().has_composition_mode()) {
    ApplyInputMode(command->input().command().composition_mode(),
                   context_->mutable_composer());
  }
  OutputMode(command);
  return true;
}

bool Session::EchoBack(commands::Command *command) {
  command->mutable_output()->set_consumed(false);
  context_->mutable_converter()->Reset();
  OutputKey(command);
  return true;
}

bool Session::EchoBackAndClearUndoContext(commands::Command *command) {
  command->mutable_output()->set_consumed(false);

  // Don't clear undo context when KeyEvent has a modifier key only.
  // TODO(hsumita): A modifier key may be assigned to another functions.
  //                ex) InsertSpace
  //                We need to check it outside of this function.
  const commands::KeyEvent &key_event = command->input().key();
  if (!IsPureModifierKeyEvent(key_event)) {
    ClearUndoContext();
  }

  return EchoBack(command);
}

bool Session::DoNothing(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  // Quick hack for zero query suggestion.
  // Caveats: Resetting converter causes b/8703702 on Windows.
  // Basically we should not *do* something in DoNothing.
  // TODO(komatsu): Fix this.
  if (context_->GetRequest().zero_query_suggestion() &&
      context_->converter().IsActive() &&
      (context_->state() == ImeContext::PRECOMPOSITION)) {
    context_->mutable_converter()->Reset();
    Output(command);
  }
  if (context_->state() & (ImeContext::COMPOSITION | ImeContext::CONVERSION)) {
    Output(command);
  }
  return true;
}

bool Session::Revert(commands::Command *command) {
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    context_->mutable_converter()->Revert();
    return EchoBackAndClearUndoContext(command);
  }

  if (!(context_->state() & (ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  if (context_->state() == ImeContext::CONVERSION) {
    context_->mutable_converter()->Cancel();
  }

  SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
  OutputMode(command);
  return true;
}

bool Session::ResetContext(commands::Command *command) {
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    context_->mutable_converter()->Reset();
    return EchoBackAndClearUndoContext(command);
  }

  command->mutable_output()->set_consumed(true);
  ClearUndoContext();

  context_->mutable_converter()->Reset();

  SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
  OutputMode(command);
  return true;
}

void Session::SetTable(const composer::Table *table) {
  ClearUndoContext();
  context_.get()->mutable_composer()->SetTable(table);
}

void Session::ReloadConfig() {
  UpdateConfig(config::ConfigHandler::GetConfig(), context_.get());
}

void Session::SetRequest(const commands::Request *request) {
  ClearUndoContext();
  context_->SetRequest(request);
}

// static
void Session::UpdateConfig(const config::Config &config, ImeContext *context) {
  context->set_keymap(config.session_keymap());

  Singleton<KeyEventTransformer>::get()->ReloadConfig(config);
  context->mutable_composer()->ReloadConfig();
  UpdateOperationPreferences(config, context);
}

// static
void Session::UpdateOperationPreferences(const config::Config &config,
                                         ImeContext *context) {
  OperationPreferences operation_preferences;
  // Keyboard shortcut for candidates.
  const char kShortcut123456789[] = "123456789";
  const char kShortcutASDFGHJKL[] = "asdfghjkl";
  config::Config::SelectionShortcut shortcut;
  if (config.has_selection_shortcut()) {
    shortcut = config.selection_shortcut();
  } else {
    shortcut = GET_CONFIG(selection_shortcut);
  }
  switch (shortcut) {
    case config::Config::SHORTCUT_123456789:
      operation_preferences.candidate_shortcuts = kShortcut123456789;
      break;
    case config::Config::SHORTCUT_ASDFGHJKL:
      operation_preferences.candidate_shortcuts = kShortcutASDFGHJKL;
      break;
    case config::Config::NO_SHORTCUT:
      operation_preferences.candidate_shortcuts.clear();
      break;
    default:
      LOG(WARNING) << "Unkown shortcuts type: "
                   << GET_CONFIG(selection_shortcut);
      break;
  }

  // Cascading Window.
#ifndef OS_LINUX
  if (config.has_use_cascading_window()) {
    operation_preferences.use_cascading_window = config.use_cascading_window();
  }
#endif
  context->mutable_converter()->SetOperationPreferences(operation_preferences);
}

bool Session::GetStatus(commands::Command *command) {
  OutputMode(command);
  return true;
}

bool Session::RequestConvertReverse(commands::Command *command) {
  if (context_->state() != ImeContext::PRECOMPOSITION &&
      context_->state() != ImeContext::DIRECT) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  Output(command);

  // Fill callback message.
  commands::SessionCommand *session_command =
      command->mutable_output()->mutable_callback()->mutable_session_command();
  session_command->set_type(commands::SessionCommand::CONVERT_REVERSE);
  return true;
}

bool Session::ConvertReverse(commands::Command *command) {
  if (context_->state() != ImeContext::PRECOMPOSITION &&
      context_->state() != ImeContext::DIRECT) {
    return DoNothing(command);
  }
  const string &composition = command->input().command().text();
  string reading;
  if (!context_->mutable_converter()->GetReadingText(composition, &reading)) {
    LOG(ERROR) << "Failed to get reading text";
    return DoNothing(command);
  }

  composer::Composer *composer = context_->mutable_composer();
  composer->Reset();
  vector<string> reading_characters;
  composer->InsertCharacterPreedit(reading);
  composer->set_source_text(composition);
  // start conversion here.
  if (!context_->mutable_converter()->Convert(*composer)) {
    LOG(ERROR) << "Failed to start conversion for reverse conversion";
    return false;
  }

  command->mutable_output()->set_consumed(true);

  SetSessionState(ImeContext::CONVERSION, context_.get());
  context_->mutable_converter()->SetCandidateListVisible(true);
  Output(command);
  return true;
}

bool Session::RequestUndo(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }

  // If undo context is empty, echoes back the key event so that it can be
  // handled by the application. b/5553298
  if (context_->state() == ImeContext::PRECOMPOSITION &&
      !prev_context_.get()) {
    return EchoBack(command);
  }

  command->mutable_output()->set_consumed(true);
  Output(command);

  // Fill callback message.
  commands::SessionCommand *session_command =
      command->mutable_output()->mutable_callback()->mutable_session_command();
  session_command->set_type(commands::SessionCommand::UNDO);
  return true;
}

bool Session::Undo(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  // Check the undo context
  if (!prev_context_.get()) {
    return DoNothing(command);
  }

  // Rollback the last user history.
  context_->mutable_converter()->Revert();

  size_t result_size = 0;
  if (context_->output().has_result()) {
    // Check the client's capability
    if (!(context_->client_capability().text_deletion() &
          commands::Capability::DELETE_PRECEDING_TEXT)) {
      return DoNothing(command);
    }
    result_size = Util::CharsLen(context_->output().result().value());
  }

  PopUndoContext();

  if (result_size > 0) {
    commands::DeletionRange *range =
        command->mutable_output()->mutable_deletion_range();
    range->set_offset(-static_cast<int>(result_size));
    range->set_length(result_size);
  }

  Output(command);
  return true;
}

bool Session::SelectCandidateInternal(commands::Command *command) {
  // If the current state is not conversion, composition or
  // precomposition, the candidate window should not be shown.  (On
  // composition or precomposition, the window is able to be shown as
  // a suggestion window).
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION |
                             ImeContext::PRECOMPOSITION))) {
    return false;
  }
  if (!command->input().has_command() ||
      !command->input().command().has_id()) {
    LOG(WARNING) << "input.command or input.command.id did not exist.";
    return false;
  }
  if (!context_->converter().IsActive()) {
    LOG(WARNING) << "converter is not active. (no candidates)";
    return false;
  }

  command->mutable_output()->set_consumed(true);

  context_->mutable_converter()->CandidateMoveToId(
      command->input().command().id(), context_->composer());
  SetSessionState(ImeContext::CONVERSION, context_.get());

  return true;
}

bool Session::SelectCandidate(commands::Command *command) {
  if (!SelectCandidateInternal(command)) {
    return DoNothing(command);
  }
  Output(command);
  return true;
}

bool Session::CommitCandidate(commands::Command *command) {
  if (!(context_->state() & (ImeContext::COMPOSITION |
                             ImeContext::CONVERSION |
                             ImeContext::PRECOMPOSITION))) {
    return false;
  }
  const commands::Input &input = command->input();
  if (!input.has_command() ||
      !input.command().has_id()) {
    LOG(WARNING) << "input.command or input.command.id did not exist.";
    return false;
  }
  if (!context_->converter().IsActive()) {
    LOG(WARNING) << "converter is not active. (no candidates)";
    return false;
  }
  command->mutable_output()->set_consumed(true);

  PushUndoContext();

  if (context_->state() & ImeContext::CONVERSION) {
    // There is a focused candidate so just select a candidate based on
    // input message and commit first segment.
    context_->mutable_converter()->CandidateMoveToId(
        input.command().id(), context_->composer());
    CommitHeadToFocusedSegmentsInternal(command->input().context());
  } else {
    // No candidate is focused.
    size_t consumed_key_size = 0;
    if (context_->mutable_converter()->CommitSuggestionById(
            input.command().id(), context_->composer(),
            command->input().context(), &consumed_key_size)) {
      if (consumed_key_size < context_->composer().GetLength()) {
        // partial suggestion was committed.
        context_->mutable_composer()->DeleteRange(0, consumed_key_size);
        MoveCursorToEnd(command);
        // Copy the previous output for Undo.
        context_->mutable_output()->CopyFrom(command->output());
        return true;
      }
    }
  }

  if (!context_->converter().IsActive()) {
    // If the converter is not active (ie. the segment size was one.),
    // the state should be switched to precomposition.
    SetSessionState(ImeContext::PRECOMPOSITION, context_.get());

    // Get suggestion if zero_query_suggestion is set.
    // zero_query_suggestion is usually set where the client is a
    // mobile.
    if (context_->GetRequest().zero_query_suggestion()) {
      Suggest(command->input());
    }
  }
  Output(command);
  // Copy the previous output for Undo.
  context_->mutable_output()->CopyFrom(command->output());
  return true;
}

bool Session::HighlightCandidate(commands::Command *command) {
  if (!SelectCandidateInternal(command)) {
    return false;
  }
  context_->mutable_converter()->SetCandidateListVisible(true);
  Output(command);
  return true;
}

bool Session::MaybeSelectCandidate(commands::Command *command) {
  if (context_->state() != ImeContext::CONVERSION) {
    return false;
  }

  // Note that SHORTCUT_ASDFGHJKL should be handled even when the CapsLock is
  // enabled. This is why we need to normalize the key event here.
  // See b/5655743.
  commands::KeyEvent normalized_keyevent;
  KeyEventUtil::NormalizeModifiers(command->input().key(),
                                   &normalized_keyevent);

  // Check if the input character is in the shortcut.
  // TODO(komatsu): Support non ASCII characters such as Unicode and
  // special keys.
  const char shortcut = static_cast<char>(normalized_keyevent.key_code());
  return context_->mutable_converter()->CandidateMoveToShortcut(shortcut);
}

void Session::set_client_capability(const commands::Capability &capability) {
  context_->mutable_client_capability()->CopyFrom(capability);
}

void Session::set_application_info(const commands::ApplicationInfo
                                   &application_info) {
  context_->mutable_application_info()->CopyFrom(application_info);
}

const commands::ApplicationInfo &Session::application_info() const {
  return context_->application_info();
}

uint64 Session::create_session_time() const {
  return context_->create_time();
}

uint64 Session::last_command_time() const {
  return context_->last_command_time();
}

bool Session::InsertCharacter(commands::Command *command) {
  if (!command->input().has_key()) {
    LOG(ERROR) << "No key event: " << command->input().DebugString();
    return false;
  }

  const commands::KeyEvent &key = command->input().key();
  if (key.input_style() == commands::KeyEvent::DIRECT_INPUT &&
      context_->state() == ImeContext::PRECOMPOSITION) {
    // If the key event represents a half width ascii character (ie.
    // key_code is equal to key_string), that key event is not
    // consumed and done echo back.
    // We must not call |EchoBackAndClearUndoContext| for a half-width space
    // here because it should be done in Session::TestSendKey or
    // Session::InsertSpaceHalfWidth. Note that the |key| comes from
    // Session::InsertSpaceHalfWidth and Session::InsertSpaceFullWidth is
    // different from the original key event.
    // For example, when the client sends a key command like
    //   {key.special_key(): HENKAN, key.modifier_keys(): [SHIFT]},
    // Session::InsertSpaceHalfWidth replaces it with
    //   {key.key_string(): " ", key.key_code(): ' '}
    // when you assign [Shift+HENKAN] to [InsertSpaceHalfWidth].
    // So |key.key_code() == ' '| does not always mean that the original key is
    // a space key w/o any modifier.
    // This is why we cannot call |EchoBackAndClearUndoContext| when
    // |key.key_code() == ' '|. This issue was found in b/5872031.
    if (key.key_string().size() == 1 &&
        key.key_code() == key.key_string()[0] &&
        key.key_code() != ' ') {
      return EchoBackAndClearUndoContext(command);
    }

    context_->mutable_composer()->InsertCharacterKeyEvent(key);
    CommitCompositionDirectly(command);
    ClearUndoContext();  // UndoContext must be invalidated.
    return true;
  }

  command->mutable_output()->set_consumed(true);

  // Handle shortcut keys selecting a candidate from a list.
  if (MaybeSelectCandidate(command)) {
    Output(command);
    return true;
  }

  string composition;
  context_->composer().GetQueryForConversion(&composition);
  bool should_commit = (context_->state() == ImeContext::CONVERSION);

  if (context_->GetRequest().space_on_alphanumeric() ==
      commands::Request::SPACE_OR_CONVERT_COMMITING_COMPOSITION &&
      context_->state() == ImeContext::COMPOSITION &&
      // TODO(komatsu): Support FullWidthSpace
      Util::EndsWith(composition, " ")) {
    should_commit = true;
  }

  if (should_commit) {
    CommitNotTriggeringZeroQuerySuggest(command);
    if (key.input_style() == commands::KeyEvent::DIRECT_INPUT) {
      // Do ClearUndoContext() because it is a direct input.
      ClearUndoContext();
      context_->mutable_composer()->InsertCharacterKeyEvent(key);
      CommitCompositionDirectly(command);
      return true;
    }
  }

  context_->mutable_composer()->InsertCharacterKeyEvent(key);
  if (context_->mutable_composer()->ShouldCommit()) {
    CommitCompositionDirectly(command);
    return true;
  }
  size_t length_to_commit = 0;
  if (context_->composer().ShouldCommitHead(&length_to_commit)) {
    return CommitHead(length_to_commit, command);
  }

  SetSessionState(ImeContext::COMPOSITION, context_.get());
  if (CanStartAutoConversion(key)) {
    return Convert(command);
  }

  if (Suggest(command->input())) {
    Output(command);
    return true;
  }

  OutputComposition(command);
  return true;
}

bool Session::IsFullWidthInsertSpace(const commands::Input &input) const {
  // If IME is off, any space has to be half-width.
  if (context_->state() == ImeContext::DIRECT) {
    return false;
  }

  // In this method, we should not update the actual input mode stored in
  // the composer even when |input| has a new input mode. Note that this
  // method can be called from TestSendKey, where internal input mode is
  // is not expected to be changed. This is one of the reasons why this
  // method is a const method.
  // On the other hand, this method should behave as if the new input mode
  // in |input| was applied. For example, this method should behave as if
  // the current input mode was HALF_KATAKANA in the following situation.
  //   composer's input mode: HIRAGANA
  //   input.key().mode()   : HALF_KATAKANA
  // To achieve this, we create a temporary composer object to which the
  // new input mode will be stored when |input| has a new input mode.
  const composer::Composer* target_composer = &context_->composer();
  scoped_ptr<composer::Composer> temporary_composer;
  if (input.has_key() && input.key().has_mode()) {
    // Allocate an object only when it is necessary.
    temporary_composer.reset(new composer::Composer(NULL, NULL));
    // Copy the current composer state just in case.
    temporary_composer->CopyFrom(context_->composer());
    ApplyInputMode(input.key().mode(), temporary_composer.get());
    // Refer to this temporary composer in this method.
    target_composer = temporary_composer.get();
  }

  // Check the current config and the current input status.
  bool is_full_width = false;
  switch (GET_CONFIG(space_character_form)) {
    case config::Config::FUNDAMENTAL_INPUT_MODE: {
      const transliteration::TransliterationType input_mode =
          target_composer->GetInputMode();
      if (transliteration::T13n::IsInHalfAsciiTypes(input_mode) ||
          transliteration::T13n::IsInHalfKatakanaTypes(input_mode)) {
        is_full_width = false;
      } else {
        is_full_width = true;
      }
      break;
    }
    case config::Config::FUNDAMENTAL_FULL_WIDTH:
      is_full_width = true;
      break;
    case config::Config::FUNDAMENTAL_HALF_WIDTH:
      is_full_width = false;
      break;
    default:
      LOG(WARNING) << "Unknown input mode";
      is_full_width = false;
      break;
  }

  return is_full_width;
}

bool Session::InsertSpace(commands::Command *command) {
  if (IsFullWidthInsertSpace(command->input())) {
    return InsertSpaceFullWidth(command);
  } else {
    return InsertSpaceHalfWidth(command);
  }
}

bool Session::InsertSpaceToggled(commands::Command *command) {
  if (IsFullWidthInsertSpace(command->input())) {
    return InsertSpaceHalfWidth(command);
  } else {
    return InsertSpaceFullWidth(command);
  }
}

bool Session::InsertSpaceHalfWidth(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }

  if (context_->state() == ImeContext::PRECOMPOSITION) {
    // TODO(komatsu): This is a hack to work around the problem with
    // the inconsistency between TestSendKey and SendKey.
    if (IsPureSpaceKey(command->input().key())) {
      return EchoBackAndClearUndoContext(command);
    }
    // UndoContext will be cleared in |InsertCharacter| in this case.
  }

  const bool has_mode = command->input().key().has_mode();
  const commands::CompositionMode mode = command->input().key().mode();
  command->mutable_input()->clear_key();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(' ');
  key_event->set_key_string(" ");
  key_event->set_input_style(commands::KeyEvent::DIRECT_INPUT);
  if (has_mode) {
    key_event->set_mode(mode);
  }
  return InsertCharacter(command);
}

bool Session::InsertSpaceFullWidth(commands::Command *command) {
  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }

  if (context_->state() == ImeContext::PRECOMPOSITION) {
    // UndoContext will be cleared in |InsertCharacter| in this case.

    // TODO(komatsu): make sure if
    // |context_->mutable_converter()->Reset()| is necessary here.
    context_->mutable_converter()->Reset();
  }

  const bool has_mode = command->input().key().has_mode();
  const commands::CompositionMode mode = command->input().key().mode();
  command->mutable_input()->clear_key();
  commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
  key_event->set_key_code(' ');
  // "　" (full-width space)
  key_event->set_key_string("\xE3\x80\x80");
  key_event->set_input_style(commands::KeyEvent::DIRECT_INPUT);
  if (has_mode) {
    key_event->set_mode(mode);
  }
  return InsertCharacter(command);
}

bool Session::TryCancelConvertReverse(commands::Command *command) {
  // If source_text is set, it usually means this session started by a
  // reverse conversion.
  if (context_->composer().source_text().empty()) {
    return false;
  }
  CommitSourceTextDirectly(command);
  return true;
}

bool Session::EditCancelOnPasswordField(commands::Command *command) {
  if (context_->composer().GetInputFieldType() != commands::Context::PASSWORD) {
    return false;
  }

  // In password mode, we should commit preedit and close keyboard
  // on Android.
  // TODO(matsuzakit): Remove this trick. b/5955618
  if (context_->composer().source_text().empty()) {
    CommitCompositionDirectly(command);
  } else {
    // Commits original text of reverse conversion.
    CommitSourceTextDirectly(command);
  }
  // Passes the key event through to MozcService.java
  // to continue the processes which are invoked by cancel operation.
  command->mutable_output()->set_consumed(false);

  return true;
}

bool Session::EditCancel(commands::Command *command) {
  if (EditCancelOnPasswordField(command)) {
    return true;
  }

  command->mutable_output()->set_consumed(true);

  // To work around b/5034698, we need to use OutputMode() unless the
  // original text is restored to cancel reconversion.
  const bool text_restored = TryCancelConvertReverse(command);
  SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
  if (text_restored) {
    Output(command);
  } else {
    // It is nice to use Output() instead of OutputMode().  However, if
    // Output() is used, unnecessary candidate words are shown because
    // the previous candidate state is not cleared here.  To fix it, we
    // should carefully modify SessionConverter. See b/5034698.
    //
    // TODO(komatsu): Use Output() instead of OutputMode.
    OutputMode(command);
  }
  return true;
}

bool Session::EditCancelAndIMEOff(commands::Command *command) {
  if (EditCancelOnPasswordField(command)) {
    return true;
  }

  if (!(context_->state() & (ImeContext::PRECOMPOSITION |
                             ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);

  TryCancelConvertReverse(command);

  ClearUndoContext();

  // Reset the context.
  context_->mutable_converter()->Reset();

  SetSessionState(ImeContext::DIRECT, context_.get());
  Output(command);
  return true;
}

bool Session::CommitInternal(commands::Command *command,
                             bool trigger_zero_query_suggest) {
  if (!(context_->state() & (ImeContext::COMPOSITION |
                             ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  PushUndoContext();

  if (context_->state() == ImeContext::COMPOSITION) {
    context_->mutable_converter()->CommitPreedit(context_->composer(),
                                                 command->input().context());
  } else {  // ImeContext::CONVERSION
    context_->mutable_converter()->Commit(context_->composer(),
                                          command->input().context());
  }

  SetSessionState(ImeContext::PRECOMPOSITION, context_.get());

  if (trigger_zero_query_suggest) {
    Suggest(command->input());
  }

  Output(command);
  // Copy the previous output for Undo.
  context_->mutable_output()->CopyFrom(command->output());
  return true;
}

bool Session::Commit(commands::Command *command) {
  return CommitInternal(command,
                        context_->GetRequest().zero_query_suggestion());
}

bool Session::CommitNotTriggeringZeroQuerySuggest(commands::Command *command) {
  return CommitInternal(command, false);
}

bool Session::CommitHead(size_t count, commands::Command *command) {
  if (!(context_->state() &
        (ImeContext::COMPOSITION | ImeContext::PRECOMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  // TODO(yamaguchi): Support undo feature.
  ClearUndoContext();

  size_t committed_size;
  context_->mutable_converter()->
      CommitHead(count, context_->composer(), &committed_size);
  context_->mutable_composer()->DeleteRange(0, committed_size);
  Output(command);
  return true;
}

bool Session::CommitFirstSuggestion(commands::Command *command) {
  if (!(context_->state() == ImeContext::COMPOSITION ||
        context_->state() == ImeContext::PRECOMPOSITION)) {
    return DoNothing(command);
  }
  if (!context_->converter().IsActive()) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  PushUndoContext();

  const int kFirstIndex = 0;
  size_t committed_key_size = 0;
  context_->mutable_converter()->CommitSuggestionByIndex(
      kFirstIndex, context_->composer(), command->input().context(),
      &committed_key_size);

  SetSessionState(ImeContext::PRECOMPOSITION, context_.get());

  // Get suggestion if zero_query_suggestion is set.
  // zero_query_suggestion is usually set where the client is a mobile.
  if (context_->GetRequest().zero_query_suggestion()) {
    Suggest(command->input());
  }

  Output(command);
  // Copy the previous output for Undo.
  context_->mutable_output()->CopyFrom(command->output());
  return true;
}

bool Session::CommitSegment(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  PushUndoContext();

  CommitFirstSegmentInternal(command->input().context());

  if (!context_->converter().IsActive()) {
    // If the converter is not active (ie. the segment size was one.),
    // the state should be switched to precomposition.
    SetSessionState(ImeContext::PRECOMPOSITION, context_.get());

    // Get suggestion if zero_query_suggestion is set.
    // zero_query_suggestion is usually set where the client is a mobile.
    if (context_->GetRequest().zero_query_suggestion()) {
      Suggest(command->input());
    }
  }
  Output(command);
  // Copy the previous output for Undo.
  context_->mutable_output()->CopyFrom(command->output());
  return true;
}

void Session::CommitFirstSegmentInternal(const commands::Context &context) {
  size_t size;
  context_->mutable_converter()->CommitFirstSegment(
      context_->composer(), context, &size);
  if (size > 0) {
    // Delete the key characters of the first segment from the preedit.
    context_->mutable_composer()->DeleteRange(0, size);
    // The number of segments should be more than one.
    DCHECK_GT(context_->composer().GetLength(), 0);
  }
}

void Session::CommitHeadToFocusedSegmentsInternal(
    const commands::Context &context) {
  size_t size;
  context_->mutable_converter()->CommitHeadToFocusedSegments(
      context_->composer(), context, &size);
  if (size > 0) {
    // Delete the key characters of the first segment from the preedit.
    context_->mutable_composer()->DeleteRange(0, size);
    // The number of segments should be more than one.
    DCHECK_GT(context_->composer().GetLength(), 0);
  }
}

void Session::CommitCompositionDirectly(commands::Command *command) {
  string composition, conversion;
  context_->composer().GetQueryForConversion(&composition);
  context_->composer().GetStringForSubmission(&conversion);
  CommitStringDirectly(composition, conversion, command);
}

void Session::CommitSourceTextDirectly(commands::Command *command) {
  // We cannot use a reference since composer will be cleared on
  // CommitStringDirectly.
  const string copied_source_text = context_->composer().source_text();
  CommitStringDirectly(copied_source_text, copied_source_text, command);
}

void Session::CommitRawTextDirectly(commands::Command *command) {
  string raw_text;
  context_->composer().GetRawString(&raw_text);
  CommitStringDirectly(raw_text, raw_text, command);
}

void Session::CommitStringDirectly(const string &key, const string &preedit,
                                   commands::Command *command) {
  if (key.empty() || preedit.empty()) {
    return;
  }

  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->Reset();

  commands::Result *result = command->mutable_output()->mutable_result();
  DCHECK(result != NULL);
  result->set_type(commands::Result::STRING);
  result->mutable_key()->append(key);
  result->mutable_value()->append(preedit);
  SetSessionState(ImeContext::PRECOMPOSITION, context_.get());

  // Get suggestion if zero_query_suggestion is set.
  // zero_query_suggestion is usually set where the client is a mobile.
  if (context_->GetRequest().zero_query_suggestion()) {
    Suggest(command->input());
  }

  Output(command);
}

namespace {
bool SuppressSuggestion(const commands::Input &input) {
  if (!input.has_context()) {
    return false;
  }
  // If the target input field is in Chrome's Omnibox or Google
  // search box, the suggest window is hidden.
  for (size_t i = 0; i < input.context().experimental_features_size(); ++i) {
    const string &feature = input.context().experimental_features(i);
    if (feature == "chrome_omnibox" || feature == "google_search_box") {
      return true;
    }
  }
  return false;
}
}  // namespace

bool Session::Suggest(const commands::Input &input) {
  if (SuppressSuggestion(input)) {
    return false;
  }

  // |reuqest_suggestion| is not supposed to always ensure suppressing
  // suggestion since this field is used for performance improvement
  // by skipping interim suggestions.  However, the implementation of
  // SessionConverter::SuggestWithPreferences does not perform suggest
  // whenever this flag is on.  So the caller should consider whether
  // this flag should be set or not.  Because the original logic was
  // implemented in Session::InserCharacter, we check the input.type()
  // is SEND_KEY assuming SEND_KEY results InsertCharacter (in most
  // cases).
  //
  // TODO(komatsu): Move the logic into SessionConverter.
  if (input.has_request_suggestion() &&
      input.type() == commands::Input::SEND_KEY) {
    ConversionPreferences conversion_preferences =
        context_->converter().conversion_preferences();
    conversion_preferences.request_suggestion = input.request_suggestion();
    return context_->mutable_converter()->SuggestWithPreferences(
        context_->composer(), conversion_preferences);
  }

  return context_->mutable_converter()->Suggest(context_->composer());
}


bool Session::ConvertToTransliteration(
    commands::Command *command,
    const transliteration::TransliterationType type) {
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!context_->mutable_converter()->ConvertToTransliteration(
          context_->composer(), type)) {
    return false;
  }
  SetSessionState(ImeContext::CONVERSION, context_.get());
  Output(command);
  return true;
}

bool Session::ConvertToHiragana(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::HIRAGANA);
}

bool Session::ConvertToFullKatakana(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::FULL_KATAKANA);
}

bool Session::ConvertToHalfKatakana(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::HALF_KATAKANA);
}

bool Session::ConvertToFullASCII(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::FULL_ASCII);
}

bool Session::ConvertToHalfASCII(commands::Command *command) {
  return ConvertToTransliteration(command, transliteration::HALF_ASCII);
}

bool Session::SwitchKanaType(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!context_->mutable_converter()->SwitchKanaType(context_->composer())) {
    return false;
  }
  SetSessionState(ImeContext::CONVERSION, context_.get());
  Output(command);
  return true;
}

bool Session::DisplayAsHiragana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHiragana(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(transliteration::HIRAGANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::DisplayAsFullKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToFullKatakana(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(transliteration::FULL_KATAKANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::DisplayAsHalfKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHalfKatakana(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(transliteration::HALF_KATAKANA);
    OutputComposition(command);
    return true;
  }
}

bool Session::TranslateFullASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToFullASCII(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(
        transliteration::T13n::ToggleFullAsciiTypes(
            context_->composer().GetOutputMode()));
    OutputComposition(command);
    return true;
  }
}

bool Session::TranslateHalfASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHalfASCII(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    context_->mutable_composer()->SetOutputMode(
        transliteration::T13n::ToggleHalfAsciiTypes(
            context_->composer().GetOutputMode()));
    OutputComposition(command);
    return true;
  }
}

bool Session::InputModeHiragana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::HIRAGANA, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeFullKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::FULL_KATAKANA, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeHalfKatakana(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::HALF_KATAKANA, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeFullASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::FULL_ASCII, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeHalfASCII(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  EnsureIMEIsOn();
  // The temporary mode should not be overridden.
  SwitchInputMode(transliteration::HALF_ASCII, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::InputModeSwitchKanaType(commands::Command *command) {
  if (context_->state() != ImeContext::PRECOMPOSITION) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);

  transliteration::TransliterationType current_type =
      context_->composer().GetInputMode();
  transliteration::TransliterationType next_type;

  switch (current_type) {
    case transliteration::HIRAGANA:
      next_type = transliteration::FULL_KATAKANA;
      break;

    case transliteration::FULL_KATAKANA:
      next_type = transliteration::HALF_KATAKANA;
      break;

    case transliteration::HALF_KATAKANA:
      next_type = transliteration::HIRAGANA;
      break;

    case transliteration::HALF_ASCII:
    case transliteration::FULL_ASCII:
      next_type = current_type;
      break;

    default:
      LOG(ERROR) << "Unknown input mode: " << current_type;
      // don't change input mode
      next_type = current_type;
      break;
  }

  // The temporary mode should not be overridden.
  SwitchInputMode(next_type, context_->mutable_composer());
  OutputFromState(command);
  return true;
}

bool Session::ConvertToHalfWidth(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION |
                             ImeContext::COMPOSITION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);

  if (!context_->mutable_converter()->ConvertToHalfWidth(
          context_->composer())) {
    return false;
  }
  SetSessionState(ImeContext::CONVERSION, context_.get());
  Output(command);
  return true;
}

bool Session::TranslateHalfWidth(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertToHalfWidth(command);
  } else {  // context_->state() == ImeContext::COMPOSITION
    const transliteration::TransliterationType type =
        context_->composer().GetOutputMode();
    if (type == transliteration::HIRAGANA ||
        type == transliteration::FULL_KATAKANA ||
        type == transliteration::HALF_KATAKANA) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_KATAKANA);
    } else if (type == transliteration::FULL_ASCII) {
      context_->mutable_composer()->SetOutputMode(transliteration::HALF_ASCII);
    } else if (type == transliteration::FULL_ASCII_UPPER) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_ASCII_UPPER);
    } else if (type == transliteration::FULL_ASCII_LOWER) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_ASCII_LOWER);
    } else if (type == transliteration::FULL_ASCII_CAPITALIZED) {
      context_->mutable_composer()->SetOutputMode(
          transliteration::HALF_ASCII_CAPITALIZED);
    } else {
      // transliteration::HALF_ASCII_something
      return TranslateHalfASCII(command);
    }
    OutputComposition(command);
    return true;
  }
}

bool Session::LaunchConfigDialog(commands::Command *command) {
  command->mutable_output()->set_launch_tool_mode(
      commands::Output::CONFIG_DIALOG);
  return DoNothing(command);
}

bool Session::LaunchDictionaryTool(commands::Command *command) {
  command->mutable_output()->set_launch_tool_mode(
      commands::Output::DICTIONARY_TOOL);
  return DoNothing(command);
}

bool Session::LaunchWordRegisterDialog(commands::Command *command) {
  command->mutable_output()->set_launch_tool_mode(
      commands::Output::WORD_REGISTER_DIALOG);
  return DoNothing(command);
}

bool Session::UndoOrRewind(commands::Command *command) {
  // Rewind if the state is in composition.
  if (context_->state() & ImeContext::COMPOSITION) {
    command->mutable_output()->set_consumed(true);
    return SendComposerCommand(composer::Composer::REWIND, command);
  }

  // Undo if we can order UNDO command.
  if (prev_context_.get()) {
    return Undo(command);
  }

  return DoNothing(command);
}

bool Session::SendComposerCommand(
    const composer::Composer::InternalCommand composer_command,
    commands::Command *command) {
  if (!(context_->state() & ImeContext::COMPOSITION)) {
    DLOG(WARNING) << "State : " << context_->state();
    return false;
  }

  context_->mutable_composer()->InsertCommandCharacter(composer_command);
  // InsertCommandCharacter method updates the preedit text
  // so we need to update suggest candidates.
  if (Suggest(command->input())) {
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::ToggleAlphanumericMode(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->ToggleInputMode();

  OutputFromState(command);
  return true;
}

bool Session::DeleteSelectedCandidateFromHistory(commands::Command *command) {
  const Segment::Candidate *cand =
      context_->converter().GetSelectedCandidateOfFocusedSegment();
  if (cand == NULL) {
    LOG(WARNING) << "No candidate is selected.";
    return DoNothing(command);
  }
  UserDataManagerInterface *manager = engine_->GetUserDataManager();
  if (!manager->ClearUserPredictionEntry(cand->key, cand->value)) {
    DLOG(WARNING) << "Cannot delete non-history candidate or deletion failed: "
                  << cand->DebugString();
    return DoNothing(command);
  }
  return ConvertCancel(command);
}

bool Session::Convert(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  string composition;
  context_->composer().GetQueryForConversion(&composition);

  // TODO(komatsu): Make a function like ConvertOrSpace.
  // Handle a space key on the ASCII composition mode.
  if (context_->state() == ImeContext::COMPOSITION &&
      (context_->composer().GetInputMode() == transliteration::HALF_ASCII ||
       context_->composer().GetInputMode() == transliteration::FULL_ASCII) &&
      command->input().key().has_special_key() &&
      command->input().key().special_key() == commands::KeyEvent::SPACE) {
    // TODO(komatsu): Consider FullWidth Space too.
    if (!Util::EndsWith(composition, " ")) {
      if (context_->GetRequest().space_on_alphanumeric() ==
          commands::Request::COMMIT) {
        // Space is committed with the composition
        context_->mutable_composer()->InsertCharacterPreedit(" ");
        return Commit(command);
      } else {
        // SPACE_OR_CONVERT_KEEPING_COMPOSITION or
        // SPACE_OR_CONVERT_COMMITING_COMPOSITION.

        // If the last character is not space, space is inserted to the
        // composition.
        command->mutable_input()->mutable_key()->set_key_code(' ');
        return InsertCharacter(command);
      }
    }

    if (!composition.empty()) {
      DCHECK_EQ(' ', composition[composition.size() - 1]);
      // Delete the last space.
      context_->mutable_composer()->Backspace();
    }
  }

  if (!context_->mutable_converter()->Convert(context_->composer())) {
    LOG(ERROR) << "Conversion failed for some reasons.";
    OutputComposition(command);
    return true;
  }

  SetSessionState(ImeContext::CONVERSION, context_.get());
  Output(command);
  return true;
}

bool Session::ConvertWithoutHistory(commands::Command *command) {
  command->mutable_output()->set_consumed(true);

  ConversionPreferences preferences =
      context_->converter().conversion_preferences();
  preferences.use_history = false;
  if (!context_->mutable_converter()->ConvertWithPreferences(
          context_->composer(), preferences)) {
    LOG(ERROR) << "Conversion failed for some reasons.";
    OutputComposition(command);
    return true;
  }

  SetSessionState(ImeContext::CONVERSION, context_.get());
  Output(command);
  return true;
}

bool Session::CommitIfPassword(commands::Command *command) {
  if (context_->composer().GetInputFieldType() == commands::Context::PASSWORD) {
    CommitCompositionDirectly(command);
    return true;
  }
  return false;
}

bool Session::MoveCursorRight(commands::Command *command) {
  // In future, we may want to change the strategy of committing, to support
  // more flexible behavior.
  // - If the composing text has some "pending toggling character(s) at the
  //   end", we'd like to "fix" the toggling state, but not to commit.
  // - Otherwise (i.e. if there is no such character(s)), we'd like to commit
  //   (considering the use cases, probably we'd like to apply it only for
  //   alphabet mode).
  // Before supporting it, we'll need to support auto fixing by waiting
  // a period. Also, it is necessary to support displaying the current toggling
  // state (otherwise, users would be confused).
  // So, to keep users out from such confusion, we only commit if the current
  // composing mode doesn't has toggling state. Clients has the responsibility
  // to check if the keyboard has toggling state or not. Note that the server
  // should know the current table has toggling state or not. However,
  // a client may NOT want to auto committing even if the composition mode
  // doesn't have the toggling state, so the server just relies on the flag
  // passed from the client.
  // TODO(hidehiko): Support it, when it is prioritized.
  if (context_->GetRequest().crossing_edge_behavior() ==
      commands::Request::COMMIT_WITHOUT_CONSUMING &&
      context_->composer().GetLength() == context_->composer().GetCursor()) {
    Commit(command);

    // Do not consume.
    command->mutable_output()->set_consumed(false);
    return true;
  }

  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorRight();
  if (Suggest(command->input())) {
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorLeft(commands::Command *command) {
  if (context_->GetRequest().crossing_edge_behavior() ==
      commands::Request::COMMIT_WITHOUT_CONSUMING &&
      context_->composer().GetCursor() == 0) {
    Commit(command);

    // Move the cursor to the beginning of the values.
    command->mutable_output()->mutable_result()->set_cursor_offset(
        -static_cast<int32>(
            Util::CharsLen(command->output().result().value())));

    // Do not consume.
    command->mutable_output()->set_consumed(false);
    return true;
  }

  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorLeft();
  if (Suggest(command->input())) {
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorToEnd(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorToEnd();
  if (Suggest(command->input())) {
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorTo(commands::Command *command) {
  if (context_->state() != ImeContext::COMPOSITION) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->
      MoveCursorTo(command->input().command().cursor_position());
  if (Suggest(command->input())) {
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::MoveCursorToBeginning(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  if (CommitIfPassword(command)) {
    return true;
  }
  context_->mutable_composer()->MoveCursorToBeginning();
  if (Suggest(command->input())) {
    Output(command);
    return true;
  }
  OutputComposition(command);
  return true;
}

bool Session::Delete(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->Delete();
  if (context_->mutable_composer()->Empty()) {
    SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
    OutputMode(command);
  } else if (Suggest(command->input())) {
    Output(command);
    return true;
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::Backspace(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->Backspace();
  if (context_->mutable_composer()->Empty()) {
    SetSessionState(ImeContext::PRECOMPOSITION, context_.get());
    OutputMode(command);
  } else if (Suggest(command->input())) {
    Output(command);
    return true;
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::SegmentFocusRight(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusRight();
  Output(command);
  return true;
}

bool Session::SegmentFocusLast(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusLast();
  Output(command);
  return true;
}

bool Session::SegmentFocusLeft(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusLeft();
  Output(command);
  return true;
}

bool Session::SegmentFocusLeftEdge(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentFocusLeftEdge();
  Output(command);
  return true;
}

bool Session::SegmentWidthExpand(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentWidthExpand(context_->composer());
  Output(command);
  return true;
}

bool Session::SegmentWidthShrink(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->SegmentWidthShrink(context_->composer());
  Output(command);
  return true;
}

bool Session::ReportBug(commands::Command *command) {
  return DoNothing(command);
}

bool Session::ConvertNext(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidateNext(context_->composer());
  Output(command);
  return true;
}

bool Session::ConvertNextPage(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidateNextPage();
  Output(command);
  return true;
}

bool Session::ConvertPrev(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidatePrev();
  Output(command);
  return true;
}

bool Session::ConvertPrevPage(commands::Command *command) {
  if (!(context_->state() & (ImeContext::CONVERSION))) {
    return DoNothing(command);
  }
  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->CandidatePrevPage();
  Output(command);
  return true;
}

bool Session::ConvertCancel(commands::Command *command) {
  command->mutable_output()->set_consumed(true);

  SetSessionState(ImeContext::COMPOSITION, context_.get());
  context_->mutable_converter()->Cancel();
  if (Suggest(command->input())) {
    Output(command);
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::PredictAndConvert(commands::Command *command) {
  if (context_->state() == ImeContext::CONVERSION) {
    return ConvertNext(command);
  }

  command->mutable_output()->set_consumed(true);
  if (context_->mutable_converter()->Predict(context_->composer())) {
    SetSessionState(ImeContext::CONVERSION, context_.get());
    Output(command);
  } else {
    OutputComposition(command);
  }
  return true;
}

bool Session::ExpandSuggestion(commands::Command *command) {
  if (context_->state() == ImeContext::CONVERSION ||
      context_->state() == ImeContext::DIRECT) {
    return DoNothing(command);
  }

  command->mutable_output()->set_consumed(true);
  context_->mutable_converter()->ExpandSuggestion(context_->composer());
  Output(command);
  return true;
}

void Session::OutputFromState(commands::Command *command) {
  if (context_->state() == ImeContext::PRECOMPOSITION) {
    OutputMode(command);
    return;
  }
  if (context_->state() == ImeContext::COMPOSITION) {
    OutputComposition(command);
    return;
  }
  if (context_->state() == ImeContext::CONVERSION) {
    Output(command);
    return;
  }
  OutputMode(command);
}

void Session::Output(commands::Command *command) {
  OutputMode(command);
  context_->mutable_converter()->PopOutput(
      context_->composer(), command->mutable_output());
  OutputWindowLocation(command);
}

void Session::OutputWindowLocation(commands::Command *command) const {
  if (!(command->output().has_candidates() &&
        context_->caret_rectangle().IsInitialized() &&
        context_->composition_rectangle().IsInitialized())) {
    return;
  }

  DCHECK(command->output().candidates().has_category());

  commands::Candidates *candidates =
      command->mutable_output()->mutable_candidates();

  candidates->mutable_caret_rectangle()->CopyFrom(
      context_->caret_rectangle());

  candidates->mutable_composition_rectangle()->CopyFrom(
      context_->composition_rectangle());

  if (command->output().candidates().category() == commands::SUGGESTION ||
      command->output().candidates().category() == commands::PREDICTION) {
    candidates->set_window_location(commands::Candidates::COMPOSITION);
  } else {
    candidates->set_window_location(commands::Candidates::CARET);
  }
}

void Session::OutputMode(commands::Command *command) const {
  const commands::CompositionMode mode =
      ToCompositionMode(context_->composer().GetInputMode());
  const commands::CompositionMode comeback_mode =
      ToCompositionMode(context_->composer().GetComebackInputMode());

  commands::Output *output = command->mutable_output();
  commands::Status *status = output->mutable_status();
  if (context_->state() == ImeContext::DIRECT) {
    output->set_mode(commands::DIRECT);
    status->set_activated(false);
  } else {
    output->set_mode(mode);
    status->set_activated(true);
  }
  status->set_mode(mode);
  status->set_comeback_mode(comeback_mode);
}

void Session::OutputComposition(commands::Command *command) const {
  OutputMode(command);
  commands::Preedit *preedit = command->mutable_output()->mutable_preedit();
  SessionOutput::FillPreedit(context_->composer(), preedit);
}

void Session::OutputKey(commands::Command *command) const {
  OutputMode(command);
  commands::KeyEvent *key = command->mutable_output()->mutable_key();
  key->CopyFrom(command->input().key());
}

namespace {
// return
// ((key_code == static_cast<uint32>('.') ||
//       key_string == "." || key_string == "．" ||
//   key_string == "。" || key_string == "｡") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_KUTEN)) ||
// ((key_code == static_cast<uint32>(',') ||
//       key_string == "," || key_string == "，" ||
//   key_string == "、" || key_string == "､") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_TOUTEN)) ||
// ((key_code == static_cast<uint32>('?') ||
//   key_string == "?" || key_string == "？") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_QUESTION_MARK)) ||
// ((key_code == static_cast<uint32>('!') ||
//   key_string == "!" || key_string == "！") &&
//  (config.auto_conversion_key() &
//   config::Config::AUTO_CONVERSION_EXCLAMATION_MARK));
bool IsValidKey(const config::Config &config,
                const uint32 key_code, const string &key_string) {
  return
      (((key_code == static_cast<uint32>('.') && key_string.empty()) ||
        key_string == "." || key_string == "\xEF\xBC\x8E" ||
        key_string == "\xE3\x80\x82" || key_string == "\xEF\xBD\xA1") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_KUTEN)) ||
      (((key_code == static_cast<uint32>(',') && key_string.empty()) ||
        key_string == "," || key_string == "\xEF\xBC\x8C" ||
        key_string == "\xE3\x80\x81" || key_string == "\xEF\xBD\xA4") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_TOUTEN)) ||
      (((key_code == static_cast<uint32>('?') && key_string.empty()) ||
        key_string == "?" || key_string == "\xEF\xBC\x9F") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_QUESTION_MARK)) ||
      (((key_code == static_cast<uint32>('!') && key_string.empty()) ||
        key_string == "!" || key_string == "\xEF\xBC\x81") &&
       (config.auto_conversion_key() &
        config::Config::AUTO_CONVERSION_EXCLAMATION_MARK));
}
}  // namespace

bool Session::CanStartAutoConversion(
    const commands::KeyEvent &key_event) const {
  if (!GET_CONFIG(use_auto_conversion)) {
    return false;
  }

  // Disable if the input comes from non-standard user keyboards, like numpad.
  // http://b/issue?id=2932067
  if (key_event.input_style() != commands::KeyEvent::FOLLOW_MODE) {
    return false;
  }

  // This is a tentative workaround for the bug http://b/issue?id=2932028
  // When user types <Shift Down>O<Shift Up>racle<Shift Down>!<Shift Up>,
  // The final "!" must be half-width, however, due to the limitation
  // of converter interface, we don't have a good way to change it halfwidth, as
  // the default preference of "!" is fullwidth. Basically, the converter is
  // not composition-mode-aware.
  // We simply disable the auto conversion feature if the mode is ASCII.
  // We conclude that disabling this feature is better in this situation.
  // TODO(taku): fix the behavior. Converter module needs to be fixed.
  if (key_event.mode() == commands::HALF_ASCII ||
      key_event.mode() == commands::FULL_ASCII) {
    return false;
  }

  // We should NOT check key_string. http://b/issue?id=3217992

  // Auto conversion is not triggered if the composition is empty or
  // only one character, or the cursor is not in the end of the
  // composition.
  const size_t length = context_->composer().GetLength();
  if (length <= 1 || length != context_->composer().GetCursor()) {
    return false;
  }

  const config::Config &config = config::ConfigHandler::GetConfig();
  const uint32 key_code = key_event.key_code();

  string preedit;
  context_->composer().GetStringForPreedit(&preedit);
  const string last_char = Util::SubString(preedit, length - 1, 1);
  if (last_char.empty()) {
    return false;
  }

  // Check last character as user may change romaji table,
  // For instance, if user assigns "." as "foo", we don't
  // want to invoke auto_conversion.
  if (!IsValidKey(config, key_code, last_char)) {
    return false;
  }

  // check the previous character of last_character.
  // when |last_prev_char| is number, we don't invoke auto_conversion
  // if the same invoke key is repeated, do not conversion.
  // http://b/issue?id=2932118
  const string last_prev_char = Util::SubString(preedit, length - 2, 1);
  if (last_prev_char.empty() || last_prev_char == last_char ||
      Util::NUMBER == Util::GetScriptType(last_prev_char)) {
    return false;
  }

  return true;
}

void Session::UpdateTime() {
  context_->set_last_command_time(Util::GetTime());
}

void Session::TransformInput(commands::Input *input) {
  if (input->has_key()) {
    Singleton<KeyEventTransformer>::get()->TransformKeyEvent(
        input->mutable_key());
  }
}

bool Session::SwitchInputFieldType(commands::Command *command) {
  command->mutable_output()->set_consumed(true);
  context_->mutable_composer()->SetInputFieldType(
      command->input().context().input_field_type());
  Output(command);
  return true;
}

bool Session::SetCaretLocation(commands::Command *command) {
  if (!command->input().has_command()) {
    return false;
  }

  const commands::SessionCommand &session_command = command->input().command();
  if (!session_command.has_caret_rectangle()) {
    context_->mutable_caret_rectangle()->Clear();
    return false;
  }

  if (!context_->caret_rectangle().IsInitialized()) {
    context_->mutable_caret_rectangle()->CopyFrom(
        session_command.caret_rectangle());
    return true;
  }

  const int caret_delta_y = abs(
      context_->caret_rectangle().y() - session_command.caret_rectangle().y());

  context_->mutable_caret_rectangle()->CopyFrom(
      session_command.caret_rectangle());

  const int kJumpThreshold = 30;

  // If caret is jumped, assume the text field is also jumped and reset the
  // rectangle of composition text.
  if (caret_delta_y > kJumpThreshold) {
    context_->mutable_composition_rectangle()->CopyFrom(
        context_->caret_rectangle());
  }

  return true;
}

bool Session::HandleIndirectImeOnOff(commands::Command *command) {
  const commands::KeyEvent &key = command->input().key();
  if (!key.has_activated()) {
    return true;
  }
  const ImeContext::State state = context_->state();
  if (state == ImeContext::DIRECT && key.activated()) {
    // Indirect IME On found.
    commands::Command on_command;
    on_command.CopyFrom(*command);
    if (!IMEOn(&on_command)) {
      return false;
    }
  } else if (state != ImeContext::DIRECT && !key.activated()) {
    // Indirect IME Off found.
    commands::Command off_command;
    off_command.CopyFrom(*command);
    if (!IMEOff(&off_command)) {
      return false;
    }
  }
  return true;
}

bool Session::CommitRawText(commands::Command *command) {
  if (context_->composer().GetLength() == 0) {
    return false;
  }
  CommitRawTextDirectly(command);
  return true;
}

// TODO(komatsu): delete this function.
composer::Composer *Session::get_internal_composer_only_for_unittest() {
  return context_->mutable_composer();
}

const ImeContext &Session::context() const {
  return *context_;
}

}  // namespace session
}  // namespace mozc
