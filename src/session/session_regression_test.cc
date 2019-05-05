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

// This is a test with the actual converter.  So the result of the
// conversion may differ from previous versions.

#include <memory>
#include <string>
#include <utility>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/system_util.h"
#include "composer/key_parser.h"
#include "composer/table.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "data_manager/testing/mock_data_manager.h"
#include "engine/engine_factory.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/config.pb.h"
#include "rewriter/rewriter_interface.h"
#include "session/internal/ime_context.h"
#include "session/internal/keymap.h"
#include "session/request_test_util.h"
#include "session/session.h"
#include "session/session_converter_interface.h"
#include "session/session_handler.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_bool(use_history_rewriter);

namespace mozc {
namespace {

string GetComposition(const commands::Command &command) {
  if (!command.output().has_preedit()) {
    return "";
  }

  string preedit;
  for (size_t i = 0; i < command.output().preedit().segment_size(); ++i) {
    preedit.append(command.output().preedit().segment(i).value());
  }
  return preedit;
}

void InitSessionToPrecomposition(session::Session* session) {
#ifdef OS_WIN
  // Session is created with direct mode on Windows
  // Direct status
  commands::Command command;
  session->IMEOn(&command);
#endif  // OS_WIN
}

}  // namespace

class SessionRegressionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    orig_use_history_rewriter_ = FLAGS_use_history_rewriter;
    FLAGS_use_history_rewriter = true;

    // Note: engine must be created after setting all the flags, as it
    // internally depends on global flags, e.g., for creation of rewriters.
    std::unique_ptr<Engine> engine(EngineFactory::Create());

    handler_.reset(new SessionHandler(std::move(engine)));
    ResetSession();
    CHECK(session_.get());
  }

  void TearDown() override {
    // just in case, reset the config in test_tmpdir
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config::ConfigHandler::SetConfig(config);

    FLAGS_use_history_rewriter = orig_use_history_rewriter_;
  }

  bool SendKey(const string &key, commands::Command *command) {
    command->Clear();
    command->mutable_input()->set_type(commands::Input::SEND_KEY);
    if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
      return false;
    }
    return session_->SendKey(command);
  }

  bool SendKeyWithContext(const string &key,
                          const commands::Context &context,
                          commands::Command *command) {
    command->Clear();
    command->mutable_input()->mutable_context()->CopyFrom(context);
    command->mutable_input()->set_type(commands::Input::SEND_KEY);
    if (!KeyParser::ParseKey(key, command->mutable_input()->mutable_key())) {
      return false;
    }
    return session_->SendKey(command);
  }

  void InsertCharacterChars(const string &chars,
                            commands::Command *command) {
    const uint32 kNoModifiers = 0;
    for (size_t i = 0; i < chars.size(); ++i) {
      command->clear_input();
      command->clear_output();
      commands::KeyEvent *key_event = command->mutable_input()->mutable_key();
      key_event->set_key_code(chars[i]);
      key_event->set_modifiers(kNoModifiers);
      session_->InsertCharacter(command);
    }
  }

  void ResetSession() {
    session_.reset(static_cast<session::Session *>(handler_->NewSession()));
    commands::Request request;
    table_.reset(new composer::Table());
    table_->InitializeWithRequestAndConfig(request, config_, data_manager_);
    session_->SetTable(table_.get());
  }

  const testing::MockDataManager data_manager_;
  bool orig_use_history_rewriter_;
  std::unique_ptr<SessionHandler> handler_;
  std::unique_ptr<session::Session> session_;
  std::unique_ptr<composer::Table> table_;
  config::Config config_;
};


TEST_F(SessionRegressionTest, ConvertToTransliterationWithMultipleSegments) {
  InitSessionToPrecomposition(session_.get());

  commands::Command command;
  InsertCharacterChars("liie", &command);

  // Convert
  command.Clear();
  session_->Convert(&command);
  {  // Check the conversion #1
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    ASSERT_LE(2, conversion.segment_size());
    EXPECT_EQ("ぃ", conversion.segment(0).value());
  }

  // TranslateHalfASCII
  command.Clear();
  session_->TranslateHalfASCII(&command);
  {  // Check the conversion #2
    const commands::Output &output = command.output();
    EXPECT_FALSE(output.has_result());
    EXPECT_TRUE(output.has_preedit());
    EXPECT_FALSE(output.has_candidates());

    const commands::Preedit &conversion = output.preedit();
    ASSERT_EQ(2, conversion.segment_size());
    EXPECT_EQ("li", conversion.segment(0).value());
  }
}

TEST_F(SessionRegressionTest,
       ExitTemporaryAlphanumModeAfterCommitingSugesstion) {
  // This is a regression test against http://b/2977131.
  {
    InitSessionToPrecomposition(session_.get());
    commands::Command command;
    InsertCharacterChars("NFL", &command);
    EXPECT_EQ(commands::HALF_ASCII, command.output().status().mode());
    EXPECT_EQ(commands::HALF_ASCII, command.output().mode());  // obsolete

    EXPECT_TRUE(SendKey("F10", &command));
    ASSERT_FALSE(command.output().has_candidates());
    EXPECT_FALSE(command.output().has_result());

    EXPECT_TRUE(SendKey("a", &command));
#if OS_MACOSX
    // The MacOS default short cut of F10 is DisplayAsHalfAlphanumeric.
    // It does not start the conversion so output does not have any result.
    EXPECT_FALSE(command.output().has_result());
#else
    EXPECT_TRUE(command.output().has_result());
#endif
    EXPECT_EQ(commands::HIRAGANA, command.output().status().mode());
    EXPECT_EQ(commands::HIRAGANA, command.output().mode());  // obsolete
  }
}

TEST_F(SessionRegressionTest, HistoryLearning) {
  InitSessionToPrecomposition(session_.get());
  commands::Command command;
  string candidate1;
  string candidate2;

  {  // First session.  Second candidate is commited.
    InsertCharacterChars("kanji", &command);

    command.Clear();
    session_->Convert(&command);
    candidate1 = GetComposition(command);

    command.Clear();
    session_->ConvertNext(&command);
    candidate2 = GetComposition(command);
    EXPECT_NE(candidate1, candidate2);

    command.Clear();
    session_->Commit(&command);
    EXPECT_FALSE(command.output().has_preedit());
    EXPECT_EQ(candidate2, command.output().result().value());
  }

  {  // Second session.  The previous second candidate should be promoted.
    command.Clear();
    InsertCharacterChars("kanji", &command);

    command.Clear();
    session_->Convert(&command);
    EXPECT_NE(candidate1, GetComposition(command));
    EXPECT_EQ(candidate2, GetComposition(command));
  }
}

TEST_F(SessionRegressionTest, Undo) {
  InitSessionToPrecomposition(session_.get());

  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session_->set_client_capability(capability);

  commands::Command command;
  InsertCharacterChars("kanji", &command);

  command.Clear();
  session_->Convert(&command);
  const string candidate1 = GetComposition(command);

  command.Clear();
  session_->ConvertNext(&command);
  const string candidate2 = GetComposition(command);
  EXPECT_NE(candidate1, candidate2);

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(candidate2, command.output().result().value());

  command.Clear();
  session_->Undo(&command);
  EXPECT_NE(candidate1, GetComposition(command));
  EXPECT_EQ(candidate2, GetComposition(command));
}

// TODO(hsumita): This test may be moved to session_test.cc.
// New converter mock is required if move this test.
TEST_F(SessionRegressionTest, PredictionAfterUndo) {
  // This is a unittest against http://b/3427619
  InitSessionToPrecomposition(session_.get());

  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  session_->set_client_capability(capability);

  commands::Command command;
  InsertCharacterChars("yoroshi", &command);
  const string kYoroshikuString = "よろしく";

  command.Clear();
  session_->PredictAndConvert(&command);
  EXPECT_EQ(1, command.output().preedit().segment_size());

  // Candidate contain "よろしく" or NOT
  int yoroshiku_id = -1;
  for (size_t i = 0; i < 10; ++i) {
    if (GetComposition(command) == kYoroshikuString) {
      yoroshiku_id = i;
      break;
    }

    command.Clear();
    session_->ConvertNext(&command);
  }
  EXPECT_EQ(kYoroshikuString, GetComposition(command));
  EXPECT_GE(yoroshiku_id, 0);

  command.Clear();
  session_->Commit(&command);
  EXPECT_FALSE(command.output().has_preedit());
  EXPECT_EQ(kYoroshikuString, command.output().result().value());

  command.Clear();
  session_->Undo(&command);
  EXPECT_EQ(kYoroshikuString, GetComposition(command));
}

// This test is to check the consistency between the result of prediction and
// suggestion.
// Following 4 values are expected to be the same.
// - The 1st candidate of prediction.
// - The result of CommitFirstSuggestion for prediction candidate.
// - The 1st candidate of suggestion.
// - The result of CommitFirstSuggestion for suggestion candidate.
//
// BACKGROUND:
// Previously there was a restriction on the result of prediction
// and suggestion.
// Currently the restriction is removed. This test checks that the logic
// works well or not.
TEST_F(SessionRegressionTest, ConsistencyBetweenPredictionAndSuggesion) {
  const char kKey[] = "aio";

  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);
  session_->SetRequest(&request);

  InitSessionToPrecomposition(session_.get());
  commands::Command command;

  command.Clear();
  InsertCharacterChars(kKey, &command);
  EXPECT_EQ(1, command.output().preedit().segment_size());
  const string suggestion_first_candidate =
      command.output().all_candidate_words().candidates(0).value();

  command.Clear();
  session_->CommitFirstSuggestion(&command);
  const string suggestion_commit_result = command.output().result().value();

  InitSessionToPrecomposition(session_.get());
  command.Clear();
  InsertCharacterChars(kKey, &command);
  command.Clear();
  session_->PredictAndConvert(&command);
  const string prediction_first_candidate =
      command.output().all_candidate_words().candidates(0).value();

  command.Clear();
  session_->Commit(&command);
  const string prediction_commit_result = command.output().result().value();

  EXPECT_EQ(suggestion_first_candidate, suggestion_commit_result);
  EXPECT_EQ(suggestion_first_candidate, prediction_first_candidate);
  EXPECT_EQ(suggestion_first_candidate, prediction_commit_result);
}

TEST_F(SessionRegressionTest, AutoConversionTest) {
  // Default mode
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());

    const char kInputKeys[] = "123456.7";
    for (size_t i = 0; kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session::ImeContext::COMPOSITION, session_->context().state());
  }

  // Auto conversion with KUTEN
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config;
    config::ConfigHandler::GetDefaultConfig(&config);
    config.set_use_auto_conversion(true);
    session_->SetConfig(&config);

    const char kInputKeys[] = "aiueo.";
    for (size_t i = 0; i < kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session::ImeContext::CONVERSION, session_->context().state());
  }

  // Auto conversion with KUTEN, but do not convert in numerical input
  {
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_use_auto_conversion(true);
    session_->SetConfig(&config);

    const char kInputKeys[] = "1234.";
    for (size_t i = 0; i < kInputKeys[i]; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code(kInputKeys[i]);
      key_event->set_key_string(string(1, kInputKeys[i]));
      session_->InsertCharacter(&command);
    }

    EXPECT_EQ(session::ImeContext::COMPOSITION, session_->context().state());
  }
}

TEST_F(SessionRegressionTest, Transliteration_Issue2330463) {
  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[],.", &command);
    command.Clear();
    SendKey("F8", &command);
    EXPECT_EQ("｢｣､｡", command.output().preedit().segment(0).value());
  }

  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[g],.", &command);
    command.Clear();
    SendKey("F8", &command);
    EXPECT_EQ("｢g｣､｡", command.output().preedit().segment(0).value());
  }

  {
    ResetSession();
    commands::Command command;

    InsertCharacterChars("[a],.", &command);
    command.Clear();
    SendKey("F8", &command);
    EXPECT_EQ("｢ｱ｣､｡", command.output().preedit().segment(0).value());
  }
}

TEST_F(SessionRegressionTest, Transliteration_Issue6209563) {
  {  // Romaji mode
    ResetSession();
    commands::Command command;

    InsertCharacterChars("tt", &command);
    command.Clear();
    SendKey("F10", &command);
    EXPECT_EQ("tt", command.output().preedit().segment(0).value());
  }

  {  // Kana mode
    ResetSession();
    commands::Command command;

    InitSessionToPrecomposition(session_.get());
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_preedit_method(config::Config::KANA);

    // Inserts "ち" 5 times
    for (int i = 0; i < 5; ++i) {
      command.Clear();
      commands::KeyEvent *key_event = command.mutable_input()->mutable_key();
      key_event->set_key_code('a');
      key_event->set_key_string("ち");
      session_->InsertCharacter(&command);
    }

    command.Clear();
    SendKey("F10", &command);
    EXPECT_EQ("aaaaa", command.output().preedit().segment(0).value());
  }
}

TEST_F(SessionRegressionTest, CommitT13nSuggestion) {
  // This is the test for http://b/6934881.
  // Pending char chunk remains after committing transliteration.
  commands::Request request;
  commands::RequestForUnitTest::FillMobileRequest(&request);
  session_->SetRequest(&request);

  InitSessionToPrecomposition(session_.get());

  commands::Command command;
  InsertCharacterChars("ssh", &command);
  EXPECT_EQ("っｓｈ", GetComposition(command));

  command.Clear();
  command.mutable_input()->set_type(commands::Input::SEND_COMMAND);
  command.mutable_input()->mutable_command()->set_type(
      commands::SessionCommand::SUBMIT_CANDIDATE);
  const int kHiraganaId = -1;
  command.mutable_input()->mutable_command()->set_id(kHiraganaId);
  session_->SendCommand(&command);

  EXPECT_TRUE(command.output().has_result());
  EXPECT_FALSE(command.output().has_preedit());

  EXPECT_EQ("っｓｈ", command.output().result().value());
}

}  // namespace mozc
