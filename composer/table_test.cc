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

#include "composer/table.h"

#include "base/file_util.h"
#include "base/port.h"
#include "base/system_util.h"
#include "composer/internal/composition_input.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "testing/base/public/gunit.h"
#include "session/commands.pb.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace composer {

using mozc::config::Config;
using mozc::commands::Request;

static void InitTable(Table* table) {
  // "あ"
  table->AddRule("a",  "\xe3\x81\x82", "");
  // "い"
  table->AddRule("i",  "\xe3\x81\x84", "");
  // "か"
  table->AddRule("ka", "\xe3\x81\x8b", "");
  // "き"
  table->AddRule("ki", "\xe3\x81\x8d", "");
  // "く"
  table->AddRule("ku", "\xe3\x81\x8f", "");
  // "け"
  table->AddRule("ke", "\xe3\x81\x91", "");
  // "こ"
  table->AddRule("ko", "\xe3\x81\x93", "");
  // "っ"
  table->AddRule("kk", "\xe3\x81\xa3", "k");
  // "な"
  table->AddRule("na", "\xe3\x81\xaa", "");
  // "に"
  table->AddRule("ni", "\xe3\x81\xab", "");
  // "ん"
  table->AddRule("n",  "\xe3\x82\x93", "");
  // "ん"
  table->AddRule("nn", "\xe3\x82\x93", "");
}

string GetResult(const Table &table, const string &key) {
  const Entry *entry = table.LookUp(key);
  if (entry == NULL) {
    return "<NULL>";
  }
  return entry->result();
}

string GetInput(const Table &table, const string &key) {
  const Entry *entry = table.LookUp(key);
  if (entry == NULL) {
    return "<NULL>";
  }
  return entry->input();
}

class TableTest : public testing::Test {
 protected:
  TableTest() {}

  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    config::ConfigHandler::GetDefaultConfig(&default_config_);
    config::ConfigHandler::SetConfig(default_config_);
  }

  virtual void TearDown() {
    config::ConfigHandler::SetConfig(default_config_);
  }

  void SetCustomRomanTable(const string &roman_table) {
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    config.set_custom_roman_table(roman_table);
    config::ConfigHandler::SetConfig(config);
  }

  config::Config default_config_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TableTest);
};

TEST_F(TableTest, LookUp) {
  static const struct TestCase {
    const char* input;
    const bool expected_result;
    const char* expected_output;
    const char* expected_pending;
  } test_cases[] = {
    // "あ"
    { "a", true, "\xe3\x81\x82", "" },
    { "k", false, "", "" },
    // "か"
    { "ka", true, "\xe3\x81\x8b", "" },
    // "き"
    { "ki", true, "\xe3\x81\x8d", "" },
    // "く"
    { "ku", true, "\xe3\x81\x8f", "" },
    // "っ"
    { "kk", true, "\xe3\x81\xa3", "k" },
    { "aka", false, "", "" },
    // "な"
    { "na", true, "\xe3\x81\xaa", "" },
    // "ん"
    { "n", true, "\xe3\x82\x93", "" },
    // "ん"
    { "nn", true, "\xe3\x82\x93", "" },
  };
  static const int size = arraysize(test_cases);

  Table table;
  InitTable(&table);

  for (int i = 0; i < size; ++i) {
    const TestCase& test = test_cases[i];
    string output;
    string pending;
    const Entry* entry;
    entry = table.LookUp(test.input);

    EXPECT_EQ(test.expected_result, (entry != NULL));
    if (entry == NULL) {
      continue;
    }
    EXPECT_EQ(test.expected_output, entry->result());
    EXPECT_EQ(test.expected_pending, entry->pending());
  }
}

TEST_F(TableTest, LookUpPredictiveAll) {
  Table table;
  InitTable(&table);

  vector<const Entry *> results;
  table.LookUpPredictiveAll("k", &results);

  EXPECT_EQ(6, results.size());
}

TEST_F(TableTest, Punctuations) {
  static const struct TestCase {
    config::Config::PunctuationMethod method;
    const char *input;
    const char *expected;
  } test_cases[] = {
    // "、"
    { config::Config::KUTEN_TOUTEN, ",",  "\xe3\x80\x81" },
    // "。"
    { config::Config::KUTEN_TOUTEN, ".",  "\xe3\x80\x82" },
    // "，"
    { config::Config::COMMA_PERIOD, ",",  "\xef\xbc\x8c" },
    // "．"
    { config::Config::COMMA_PERIOD, ".",  "\xef\xbc\x8e" },
    // "、"
    { config::Config::KUTEN_PERIOD, ",",  "\xe3\x80\x81" },
    // "．"
    { config::Config::KUTEN_PERIOD, ".",  "\xef\xbc\x8e" },
    // "，"
    { config::Config::COMMA_TOUTEN, ",",  "\xef\xbc\x8c" },
    // "。"
    { config::Config::COMMA_TOUTEN, ".",  "\xe3\x80\x82" },
  };

  const string config_file = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                                "mozc_config_test_tmp");
  FileUtil::Unlink(config_file);
  config::ConfigHandler::SetConfigFileName(config_file);
  config::ConfigHandler::Reload();
  commands::Request request;

  for (int i = 0; i < arraysize(test_cases); ++i) {
    config::Config config;
    config.set_punctuation_method(test_cases[i].method);
    EXPECT_TRUE(config::ConfigHandler::SetConfig(config));
    Table table;
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));
    const Entry *entry = table.LookUp(test_cases[i].input);
    EXPECT_TRUE(entry != NULL) << "Failed index = " << i;
    if (entry) {
      EXPECT_EQ(test_cases[i].expected, entry->result());
    }
  }
}

TEST_F(TableTest, Symbols) {
  static const struct TestCase {
    config::Config::SymbolMethod method;
    const char *input;
    const char *expected;
  } test_cases[] = {
    // "「"
    { config::Config::CORNER_BRACKET_MIDDLE_DOT, "[",  "\xe3\x80\x8c" },
    // "」"
    { config::Config::CORNER_BRACKET_MIDDLE_DOT, "]",  "\xe3\x80\x8d" },
    // "・"
    { config::Config::CORNER_BRACKET_MIDDLE_DOT, "/",  "\xe3\x83\xbb" },
    { config::Config::SQUARE_BRACKET_SLASH, "[",  "["      },
    { config::Config::SQUARE_BRACKET_SLASH, "]",  "]"      },
    // "／"
    { config::Config::SQUARE_BRACKET_SLASH, "/",  "\xef\xbc\x8f"      },
    // "「"
    { config::Config::CORNER_BRACKET_SLASH, "[",  "\xe3\x80\x8c"      },
    // "」"
    { config::Config::CORNER_BRACKET_SLASH, "]",  "\xe3\x80\x8d"      },
    // "／"
    { config::Config::CORNER_BRACKET_SLASH, "/",  "\xef\xbc\x8f"      },
    { config::Config::SQUARE_BRACKET_MIDDLE_DOT, "[",  "[" },
    { config::Config::SQUARE_BRACKET_MIDDLE_DOT, "]",  "]" },
    // "・"
    { config::Config::SQUARE_BRACKET_MIDDLE_DOT, "/",  "\xe3\x83\xbb" },
  };

  const string config_file = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                                "mozc_config_test_tmp");
  FileUtil::Unlink(config_file);
  config::ConfigHandler::SetConfigFileName(config_file);
  config::ConfigHandler::Reload();
  commands::Request request;

  for (int i = 0; i < arraysize(test_cases); ++i) {
    config::Config config;
    config.set_symbol_method(test_cases[i].method);
    EXPECT_TRUE(config::ConfigHandler::SetConfig(config));
    Table table;
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));
    const Entry *entry = table.LookUp(test_cases[i].input);
    EXPECT_TRUE(entry != NULL) << "Failed index = " << i;
    if (entry) {
      EXPECT_EQ(test_cases[i].expected, entry->result());
    }
  }
}

TEST_F(TableTest, KanaSuppressed) {
  config::Config config;
  config::ConfigHandler::GetConfig(&config);

  config.set_preedit_method(config::Config::KANA);
  config::ConfigHandler::SetConfig(config);

  commands::Request request;

  Table table;
  ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));

  const Entry *entry = table.LookUp("a");
  ASSERT_TRUE(entry != NULL);
  // "あ"
  EXPECT_EQ("\xE3\x81\x82", entry->result());
  EXPECT_TRUE(entry->pending().empty());
}

TEST_F(TableTest, KanaCombination) {
  Table table;
  commands::Request request;
  ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, default_config_));
  // "か゛"
  const Entry *entry = table.LookUp("\xE3\x81\x8B\xE3\x82\x9B");
  ASSERT_TRUE(entry != NULL);
  // "が"
  EXPECT_EQ("\xE3\x81\x8C", entry->result());
  EXPECT_TRUE(entry->pending().empty());
}

TEST_F(TableTest, InvalidEntryTest) {
  {
    Table table;
    EXPECT_FALSE(table.IsLoopingEntry("a", "b"));
    table.AddRule("a", "aa", "b");

    EXPECT_TRUE(table.IsLoopingEntry("b", "a"));
    table.AddRule("b", "aa", "a");  // looping

    EXPECT_TRUE(table.LookUp("a") != NULL);
    EXPECT_TRUE(table.LookUp("b") == NULL);
  }

  {
    Table table;
    EXPECT_FALSE(table.IsLoopingEntry("a", "ba"));
    table.AddRule("a", "aa", "ba");

    EXPECT_TRUE(table.IsLoopingEntry("b", "a"));
    table.AddRule("b", "aa", "a");  // looping

    EXPECT_TRUE(table.LookUp("a") != NULL);
    EXPECT_TRUE(table.LookUp("b") == NULL);
  }

  {
    Table table;
    EXPECT_FALSE(table.IsLoopingEntry("a", "b"));
    table.AddRule("a", "aa", "b");

    EXPECT_FALSE(table.IsLoopingEntry("b", "c"));
    table.AddRule("b", "aa", "c");

    EXPECT_FALSE(table.IsLoopingEntry("c", "d"));
    table.AddRule("c", "aa", "d");

    EXPECT_TRUE(table.IsLoopingEntry("d", "a"));
    table.AddRule("d", "aa", "a");  // looping

    EXPECT_TRUE(table.LookUp("a") != NULL);
    EXPECT_TRUE(table.LookUp("b") != NULL);
    EXPECT_TRUE(table.LookUp("c") != NULL);
    EXPECT_TRUE(table.LookUp("d") == NULL);
  }

  {
    Table table;
    table.AddRule("wa", "WA", "");
    table.AddRule("ww", "X", "w");

    EXPECT_FALSE(table.IsLoopingEntry("www", "ww"));
    table.AddRule("www", "W", "ww");  // not looping

    EXPECT_TRUE(table.LookUp("wa") != NULL);
    EXPECT_TRUE(table.LookUp("ww") != NULL);
    EXPECT_TRUE(table.LookUp("www") != NULL);
  }

  {
    Table table;
    table.AddRule("wa", "WA", "");
    table.AddRule("www", "W", "ww");

    EXPECT_FALSE(table.IsLoopingEntry("ww", "w"));
    table.AddRule("ww", "X", "w");

    EXPECT_TRUE(table.LookUp("wa") != NULL);
    EXPECT_TRUE(table.LookUp("ww") != NULL);
    EXPECT_TRUE(table.LookUp("www") != NULL);
  }

  {
    Table table;
    EXPECT_TRUE(table.IsLoopingEntry("a", "a"));
    table.AddRule("a", "aa", "a");  // looping

    EXPECT_TRUE(table.LookUp("a") == NULL);
  }

  // Too long input
  {
    Table table;
    string too_long;
    // Maximum size is 300 now.
    for (int i = 0; i < 1024; ++i) {
      too_long += 'a';
    }
    table.AddRule(too_long, "test", "test");
    EXPECT_TRUE(table.LookUp(too_long) == NULL);

    table.AddRule("a", too_long, "test");
    EXPECT_TRUE(table.LookUp("a") == NULL);

    table.AddRule("a", "test", too_long);
    EXPECT_TRUE(table.LookUp("a") == NULL);
  }

  // reasonably long
  {
    Table table;
    string reasonably_long;
    // Maximum size is 300 now.
    for (int i = 0; i < 200; ++i) {
      reasonably_long += 'a';
    }
    table.AddRule(reasonably_long, "test", "test");
    EXPECT_TRUE(table.LookUp(reasonably_long) != NULL);

    table.AddRule("a", reasonably_long, "test");
    EXPECT_TRUE(table.LookUp("a") != NULL);

    table.AddRule("a", "test", reasonably_long);
    EXPECT_TRUE(table.LookUp("a") != NULL);
  }
}

TEST_F(TableTest, CustomPunctuationsAndSymbols) {
  // Test against Issue2465801.
  string custom_roman_table;
  custom_roman_table.append("mozc\tMOZC\n");
  custom_roman_table.append(",\tCOMMA\n");
  custom_roman_table.append(".\tPERIOD\n");
  custom_roman_table.append("/\tSLASH\n");
  custom_roman_table.append("[\tOPEN\n");
  custom_roman_table.append("]\tCLOSE\n");

  SetCustomRomanTable(custom_roman_table);

  Table table;
  commands::Request request;
  config::Config config;
  config::ConfigHandler::GetConfig(&config);
  table.InitializeWithRequestAndConfig(request, config);

  const Entry *entry = NULL;
  entry = table.LookUp("mozc");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("MOZC", entry->result());

  entry = table.LookUp(",");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("COMMA", entry->result());

  entry = table.LookUp(".");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("PERIOD", entry->result());

  entry = table.LookUp("/");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("SLASH", entry->result());

  entry = table.LookUp("[");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("OPEN", entry->result());

  entry = table.LookUp("]");
  ASSERT_TRUE(entry != NULL);
  EXPECT_EQ("CLOSE", entry->result());
}

TEST_F(TableTest, CaseSensitive) {
  Table table;
  table.AddRule("a", "[a]", "");
  table.AddRule("A", "[A]", "");
  table.AddRule("ba", "[ba]", "");
  table.AddRule("BA", "[BA]", "");
  table.AddRule("Ba", "[Ba]", "");
  // The rule of "bA" is intentionally dropped.
  // table.AddRule("bA",  "[bA]", "");
  table.AddRule("za", "[za]", "");

  // case insensitive
  table.set_case_sensitive(false);
  EXPECT_EQ("[a]", GetResult(table, "a"));
  EXPECT_EQ("[a]", GetResult(table, "A"));
  EXPECT_EQ("[ba]", GetResult(table, "ba"));
  EXPECT_EQ("[ba]", GetResult(table, "BA"));
  EXPECT_EQ("[ba]", GetResult(table, "Ba"));
  EXPECT_EQ("[ba]", GetResult(table, "bA"));

  EXPECT_EQ("a", GetInput(table, "a"));
  EXPECT_EQ("a", GetInput(table, "A"));
  EXPECT_EQ("ba", GetInput(table, "ba"));
  EXPECT_EQ("ba", GetInput(table, "BA"));
  EXPECT_EQ("ba", GetInput(table, "Ba"));
  EXPECT_EQ("ba", GetInput(table, "bA"));

  // Test for HasSubRules
  EXPECT_TRUE(table.HasSubRules("Z"));

  {  // Test for LookUpPrefix
    const Entry *entry = NULL;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("bA", &key_length, &fixed);
    EXPECT_TRUE(entry != NULL);
    EXPECT_EQ("[ba]", entry->result());
    EXPECT_EQ(2, key_length);
    EXPECT_TRUE(fixed);
  }

  // case sensitive
  table.set_case_sensitive(true);
  EXPECT_TRUE(table.case_sensitive());
  EXPECT_EQ("[a]", GetResult(table, "a"));
  EXPECT_EQ("[A]", GetResult(table, "A"));
  EXPECT_EQ("[ba]", GetResult(table, "ba"));
  EXPECT_EQ("[BA]", GetResult(table, "BA"));
  EXPECT_EQ("[Ba]", GetResult(table, "Ba"));
  EXPECT_EQ("<NULL>", GetResult(table, "bA"));

  EXPECT_EQ("a", GetInput(table, "a"));
  EXPECT_EQ("A", GetInput(table, "A"));
  EXPECT_EQ("ba", GetInput(table, "ba"));
  EXPECT_EQ("BA", GetInput(table, "BA"));
  EXPECT_EQ("Ba", GetInput(table, "Ba"));
  EXPECT_EQ("<NULL>", GetInput(table, "bA"));

  // Test for HasSubRules
  EXPECT_FALSE(table.HasSubRules("Z"));

  {  // Test for LookUpPrefix
    const Entry *entry = NULL;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("bA", &key_length, &fixed);
    EXPECT_TRUE(entry == NULL);
    EXPECT_EQ(1, key_length);
    EXPECT_TRUE(fixed);
  }
}

TEST_F(TableTest, CaseSensitivity) {
  commands::Request request;
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    table.AddRule("", "", "");
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    table.AddRule("a", "", "");
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    table.AddRule("A", "", "");
    EXPECT_TRUE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    table.AddRule("a{A}a", "", "");
    EXPECT_FALSE(table.case_sensitive());
  }
  {
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    table.AddRule("A{A}A", "", "");
    EXPECT_TRUE(table.case_sensitive());
  }
}

// This test case was needed because the case sensitivity was configured
// by the configuration.
// Currently the case sensitivity is independent from the configuration.
TEST_F(TableTest, CaseSensitiveByConfiguration) {
  config::Config config;
  commands::Request request;
  Table table;

  // config::Config::OFF
  {
    config.set_shift_key_mode_switch(config::Config::OFF);
    EXPECT_TRUE(config::ConfigHandler::SetConfig(config));
    table.InitializeWithRequestAndConfig(request, config);

    table.AddRule("a", "[a]", "");
    table.AddRule("A", "[A]", "");
    table.AddRule("ba", "[ba]", "");
    table.AddRule("BA", "[BA]", "");
    table.AddRule("Ba", "[Ba]", "");

    EXPECT_TRUE(table.case_sensitive());
    EXPECT_EQ("[a]", GetResult(table, "a"));
    EXPECT_EQ("[A]", GetResult(table, "A"));
    EXPECT_EQ("[ba]", GetResult(table, "ba"));
    EXPECT_EQ("[BA]", GetResult(table, "BA"));
    EXPECT_EQ("[Ba]", GetResult(table, "Ba"));
    EXPECT_EQ("<NULL>", GetResult(table, "bA"));

    EXPECT_EQ("a", GetInput(table, "a"));
    EXPECT_EQ("A", GetInput(table, "A"));
    EXPECT_EQ("ba", GetInput(table, "ba"));
    EXPECT_EQ("BA", GetInput(table, "BA"));
    EXPECT_EQ("Ba", GetInput(table, "Ba"));
    EXPECT_EQ("<NULL>", GetInput(table, "bA"));

    // Test for HasSubRules
    EXPECT_FALSE(table.HasSubRules("Z"));

    { // Test for LookUpPrefix
      const Entry *entry = NULL;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("bA", &key_length, &fixed);
      EXPECT_TRUE(entry == NULL);
      EXPECT_EQ(1, key_length);
      EXPECT_TRUE(fixed);
    }
  }

  // config::Config::ASCII_INPUT_MODE
  {
    config.set_shift_key_mode_switch(config::Config::ASCII_INPUT_MODE);
    EXPECT_TRUE(config::ConfigHandler::SetConfig(config));
    table.InitializeWithRequestAndConfig(request, config);

    table.AddRule("a", "[a]", "");
    table.AddRule("A", "[A]", "");
    table.AddRule("ba", "[ba]", "");
    table.AddRule("BA", "[BA]", "");
    table.AddRule("Ba", "[Ba]", "");

    EXPECT_TRUE(table.case_sensitive());
    EXPECT_EQ("[a]", GetResult(table, "a"));
    EXPECT_EQ("[A]", GetResult(table, "A"));
    EXPECT_EQ("[ba]", GetResult(table, "ba"));
    EXPECT_EQ("[BA]", GetResult(table, "BA"));
    EXPECT_EQ("[Ba]", GetResult(table, "Ba"));
    EXPECT_EQ("<NULL>", GetResult(table, "bA"));

    EXPECT_EQ("a", GetInput(table, "a"));
    EXPECT_EQ("A", GetInput(table, "A"));
    EXPECT_EQ("ba", GetInput(table, "ba"));
    EXPECT_EQ("BA", GetInput(table, "BA"));
    EXPECT_EQ("Ba", GetInput(table, "Ba"));
    EXPECT_EQ("<NULL>", GetInput(table, "bA"));

    // Test for HasSubRules
    EXPECT_FALSE(table.HasSubRules("Z"));

    { // Test for LookUpPrefix
      const Entry *entry = NULL;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("bA", &key_length, &fixed);
      EXPECT_TRUE(entry == NULL);
      EXPECT_EQ(1, key_length);
      EXPECT_TRUE(fixed);
    }
  }

  // config::Config::KATAKANA_INPUT_MODE
  {
    config.set_shift_key_mode_switch(config::Config::KATAKANA_INPUT_MODE);
    EXPECT_TRUE(config::ConfigHandler::SetConfig(config));
    table.InitializeWithRequestAndConfig(request, config);

    table.AddRule("a", "[a]", "");
    table.AddRule("A", "[A]", "");
    table.AddRule("ba", "[ba]", "");
    table.AddRule("BA", "[BA]", "");
    table.AddRule("Ba", "[Ba]", "");

    EXPECT_TRUE(table.case_sensitive());
    EXPECT_EQ("[a]", GetResult(table, "a"));
    EXPECT_EQ("[A]", GetResult(table, "A"));
    EXPECT_EQ("[ba]", GetResult(table, "ba"));
    EXPECT_EQ("[BA]", GetResult(table, "BA"));
    EXPECT_EQ("[Ba]", GetResult(table, "Ba"));
    EXPECT_EQ("<NULL>", GetResult(table, "bA"));

    EXPECT_EQ("a", GetInput(table, "a"));
    EXPECT_EQ("A", GetInput(table, "A"));
    EXPECT_EQ("ba", GetInput(table, "ba"));
    EXPECT_EQ("BA", GetInput(table, "BA"));
    EXPECT_EQ("Ba", GetInput(table, "Ba"));
    EXPECT_EQ("<NULL>", GetInput(table, "bA"));

    // Test for HasSubRules
    EXPECT_FALSE(table.HasSubRules("Z"));

    { // Test for LookUpPrefix
      const Entry *entry = NULL;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("bA", &key_length, &fixed);
      EXPECT_TRUE(entry == NULL);
      EXPECT_EQ(1, key_length);
      EXPECT_TRUE(fixed);
    }
  }
}

// Table class automatically enables case-sensitive mode when the given roman
// table has any input rule which contains one or more upper case characters.
//   e.g. "V" -> "5" or "YT" -> "You there"
// This feature was implemented as b/2910223 as per following request.
// http://www.google.com/support/forum/p/ime/thread?tid=4ea9aed4ac8a2ba6&hl=ja
//
// The following test checks if a case-sensitive and a case-inensitive roman
// table enables and disables this "case-sensitive mode", respectively.
TEST_F(TableTest, AutomaticCaseSensitiveDetection) {
  static const char kCaseInsensitiveRomanTable[] = {
    "m\tmozc\n"     // m -> mozc
    "n\tnamazu\n"   // n -> namazu
  };
  static const char kCaseSensitiveRomanTable[] = {
    "m\tmozc\n"     // m -> mozc
    "M\tMozc\n"     // M -> Mozc
  };

  commands::Request request;

  {
    Table table;
    SetCustomRomanTable(kCaseSensitiveRomanTable);
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    EXPECT_FALSE(table.case_sensitive())
        << "case-sensitive mode should be desabled by default.";
    // Load a custom config with case-sensitive custom roman table.
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));
    EXPECT_TRUE(table.case_sensitive())
        << "Case sensitive roman table should enable case-sensitive mode.";
    // Explicitly disable case-sensitive mode.
    table.set_case_sensitive(false);
    ASSERT_FALSE(table.case_sensitive());
  }

  {
    Table table;
    // Load a custom config with case-insensitive custom roman table.
    SetCustomRomanTable(kCaseInsensitiveRomanTable);
    config::Config config;
    config::ConfigHandler::GetConfig(&config);
    ASSERT_TRUE(table.InitializeWithRequestAndConfig(request, config));
    EXPECT_FALSE(table.case_sensitive())
        << "Case insensitive roman table should disable case-sensitive mode.";
    // Explicitly enable case-sensitive mode.
    table.set_case_sensitive(true);
    ASSERT_TRUE(table.case_sensitive());
  }
}

TEST_F(TableTest, MobileMode) {
  mozc::commands::Request request;
  request.set_zero_query_suggestion(true);
  request.set_mixed_conversion(true);
  request.set_combine_all_segments(true);

  {
    // To 12keys -> Hiragana mode
    request.set_special_romanji_table(
        mozc::commands::Request::TWELVE_KEYS_TO_HIRAGANA);
    mozc::composer::Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    {
      const mozc::composer::Entry *entry = NULL;
      size_t key_length = 0;
      bool fixed = false;
      entry = table.LookUpPrefix("2", &key_length, &fixed);
      EXPECT_EQ("2", entry->input());
      EXPECT_EQ("", entry->result());
      // "か"
      EXPECT_EQ("\xE3\x81\x8B", entry->pending());
      EXPECT_EQ(1, key_length);
      EXPECT_TRUE(fixed);
    }
    {
      const mozc::composer::Entry *entry = NULL;
      size_t key_length = 0;
      bool fixed = false;
      // "し*"
      entry = table.LookUpPrefix("\xE3\x81\x97*", &key_length, &fixed);
      // "し*"
      EXPECT_EQ("\xE3\x81\x97*", entry->input());
      EXPECT_EQ("", entry->result());
      // "\x0F*\x0Eじ"
      // 0F and 0E are shift in/out characters.
      EXPECT_EQ("\x0F*\x0E\xE3\x81\x98", entry->pending());
      EXPECT_EQ(4, key_length);
      EXPECT_TRUE(fixed);
    }
  }

  {
    // To 12keys -> Halfwidth Ascii mode
    request.set_special_romanji_table(
        mozc::commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII);
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    const mozc::composer::Entry *entry = NULL;
    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("2", &key_length, &fixed);
    EXPECT_EQ("a", entry->pending());
  }

  {
    // To Godan -> Hiragana mode
    request.set_special_romanji_table(
        mozc::commands::Request::GODAN_TO_HIRAGANA);
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);
    {
      const mozc::composer::Entry *entry = NULL;
      size_t key_length = 0;
      bool fixed = false;
      // "しゃ*"
      entry = table.LookUpPrefix("\xE3\x81\x97\xE3\x82\x83*", &key_length,
                                 &fixed);
      // "じゃ"
      EXPECT_EQ("\xE3\x81\x98\xE3\x82\x83", entry->pending());
    }
  }

  {
    // To Flick -> Hiragana mode.
    request.set_special_romanji_table(
        mozc::commands::Request::FLICK_TO_HIRAGANA);
    Table table;
    table.InitializeWithRequestAndConfig(request, default_config_);

    size_t key_length = 0;
    bool fixed = false;
    const mozc::composer::Entry *entry = table.LookUpPrefix("a", &key_length,
                                                            &fixed);
    // "き"
    EXPECT_EQ("\xE3\x81\x8D", entry->pending());
  }

}

TEST_F(TableTest, OrderOfAddRule) {
  // The order of AddRule should not be sensitive.
  {
    Table table;
    table.AddRule("www", "w", "ww");
    table.AddRule("ww", "[X]", "w");
    table.AddRule("we", "[WE]", "");
    EXPECT_TRUE(table.HasSubRules("ww"));

    const Entry *entry;
    entry = table.LookUp("ww");
    EXPECT_TRUE(NULL != entry);

    size_t key_length;
    bool fixed;
    entry = table.LookUpPrefix("ww", &key_length, &fixed);
    EXPECT_TRUE(NULL != entry);
    EXPECT_EQ(2, key_length);
    EXPECT_FALSE(fixed);
  }
  {
    Table table;
    table.AddRule("ww", "[X]", "w");
    table.AddRule("we", "[WE]", "");
    table.AddRule("www", "w", "ww");
    EXPECT_TRUE(table.HasSubRules("ww"));

    const Entry *entry = NULL;
    entry = table.LookUp("ww");
    EXPECT_TRUE(NULL != entry);

    size_t key_length = 0;
    bool fixed = false;
    entry = table.LookUpPrefix("ww", &key_length, &fixed);
    EXPECT_TRUE(NULL != entry);
    EXPECT_EQ(2, key_length);
    EXPECT_FALSE(fixed);
  }
}

TEST_F(TableTest, AddRuleWithAttributes) {
  const string kInput = "1";
  Table table;
  table.AddRuleWithAttributes(kInput, "", "a", NEW_CHUNK);

  EXPECT_TRUE(table.HasNewChunkEntry(kInput));

  size_t key_length = 0;
  bool fixed = false;
  const Entry *entry = table.LookUpPrefix(kInput, &key_length, &fixed);
  EXPECT_EQ(1, key_length);
  EXPECT_TRUE(fixed);
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ(kInput, entry->input());
  EXPECT_EQ("", entry->result());
  EXPECT_EQ("a", entry->pending());
  EXPECT_EQ(NEW_CHUNK, entry->attributes());

  const string kInput2 = "22";
  table.AddRuleWithAttributes(kInput2, "", "b", NEW_CHUNK | NO_TRANSLITERATION);

  EXPECT_TRUE(table.HasNewChunkEntry(kInput2));

  key_length = 0;
  fixed = false;
  entry = table.LookUpPrefix(kInput2, &key_length, &fixed);
  EXPECT_EQ(2, key_length);
  EXPECT_TRUE(fixed);
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ(kInput2, entry->input());
  EXPECT_EQ("", entry->result());
  EXPECT_EQ("b", entry->pending());
  EXPECT_EQ((NEW_CHUNK | NO_TRANSLITERATION), entry->attributes());
}

TEST_F(TableTest, LoadFromString) {
  const string kRule =
    "# This is a comment\n"
    "\n"  // Empty line to be ignored.
    "a\t[A]\n"  // 2 entry rule
    "kk\t[X]\tk\n"  // 3 entry rule
    "ww\t[W]\tw\tNewChunk\n"  // 3 entry rule + attribute rule
    "xx\t[X]\tx\tNewChunk NoTransliteration\n"  // multiple attribute rules
    // all attributes
    "yy\t[Y]\ty\tNewChunk NoTransliteration DirectInput EndChunk\n"
    "#\t[#]\n";  // This line starts with '#' but should be a rule.
  Table table;
  table.LoadFromString(kRule);

  const Entry *entry = NULL;
  // Test for "a\t[A]\n"  -- 2 entry rule
  EXPECT_FALSE(table.HasNewChunkEntry("a"));
  entry = table.LookUp("a");
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ("[A]", entry->result());
  EXPECT_EQ("", entry->pending());

  // Test for "kk\t[X]\tk\n"  -- 3 entry rule
  EXPECT_FALSE(table.HasNewChunkEntry("kk"));
  entry = table.LookUp("kk");
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ("[X]", entry->result());
  EXPECT_EQ("k", entry->pending());

  // Test for "ww\t[W]\tw\tNewChunk\n"  -- 3 entry rule + attribute rule
  EXPECT_TRUE(table.HasNewChunkEntry("ww"));
  entry = table.LookUp("ww");
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ("[W]", entry->result());
  EXPECT_EQ("w", entry->pending());
  EXPECT_EQ(NEW_CHUNK, entry->attributes());

  // Test for "xx\t[X]\tx\tNewChunk NoTransliteration\n" -- multiple
  // attribute rules
  EXPECT_TRUE(table.HasNewChunkEntry("xx"));
  entry = table.LookUp("xx");
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ("[X]", entry->result());
  EXPECT_EQ("x", entry->pending());
  EXPECT_EQ((NEW_CHUNK | NO_TRANSLITERATION), entry->attributes());

  // Test for "yy\t[Y]\ty\tNewChunk NoTransliteration DirectInput EndChunk\n"
  // -- all attributes
  EXPECT_TRUE(table.HasNewChunkEntry("yy"));
  entry = table.LookUp("yy");
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ("[Y]", entry->result());
  EXPECT_EQ("y", entry->pending());
  EXPECT_EQ((NEW_CHUNK | NO_TRANSLITERATION | DIRECT_INPUT | END_CHUNK),
            entry->attributes());

  // Test for "#\t[#]\n"  -- This line starts with '#' but should be a rule.
  entry = table.LookUp("#");
  ASSERT_TRUE(NULL != entry);
  EXPECT_EQ("[#]", entry->result());
  EXPECT_EQ("", entry->pending());
}

TEST_F(TableTest, SpecialKeys) {
  {
    Table table;
    table.AddRule("x{#1}y", "X1Y", "");
    table.AddRule("x{#2}y", "X2Y", "");
    table.AddRule("x{{}", "X{", "");
    table.AddRule("xy", "XY", "");

    const Entry *entry = NULL;
    entry = table.LookUp("x{#1}y");
    EXPECT_TRUE(NULL == entry);

    string key;
    key = Table::ParseSpecialKey("x{#1}y");
    entry = table.LookUp(key);
    ASSERT_TRUE(NULL != entry);
    EXPECT_EQ(key, entry->input());
    EXPECT_EQ("X1Y", entry->result());

    key = Table::ParseSpecialKey("x{#2}y");
    entry = table.LookUp(key);
    ASSERT_TRUE(NULL != entry);
    EXPECT_EQ(key, entry->input());
    EXPECT_EQ("X2Y", entry->result());

    key = "x{";
    entry = table.LookUp(key);
    ASSERT_TRUE(NULL != entry);
    EXPECT_EQ(key, entry->input());
    EXPECT_EQ("X{", entry->result());
  }

  {
    // "{{}" is replaced with "{".
    // "{*}" is replaced with "\x0F*\x0E".
    Table table;
    EXPECT_EQ("\x0F" "\x0E", table.AddRule("{}", "", "")->input());
    EXPECT_EQ("{", table.AddRule("{", "", "")->input());
    EXPECT_EQ("}", table.AddRule("}", "", "")->input());
    EXPECT_EQ("{", table.AddRule("{{}", "", "")->input());
    EXPECT_EQ("{}", table.AddRule("{{}}", "", "")->input());
    EXPECT_EQ("a{", table.AddRule("a{", "", "")->input());
    EXPECT_EQ("{a", table.AddRule("{a", "", "")->input());
    EXPECT_EQ("a{a", table.AddRule("a{a", "", "")->input());
    EXPECT_EQ("a}", table.AddRule("a}", "", "")->input());
    EXPECT_EQ("}a", table.AddRule("}a", "", "")->input());
    EXPECT_EQ("a}a", table.AddRule("a}a", "", "")->input());
    EXPECT_EQ("a" "\x0F" "b" "\x0E" "c",
              table.AddRule("a{b}c", "", "")->input());
    EXPECT_EQ("a" "\x0F" "b" "\x0E" "c" "\x0F" "d" "\x0E" "\x0F" "e" "\x0E",
              table.AddRule("a{b}c{d}{e}", "", "")->input());
    EXPECT_EQ("}-{", table.AddRule("}-{", "", "")->input());
    EXPECT_EQ("a{bc", table.AddRule("a{bc", "", "")->input());

    // This is not a fixed specification, but a current behavior.
    EXPECT_EQ("\x0F" "{-" "\x0E" "}",
              table.AddRule("{{-}}", "", "")->input());
  }
}

TEST_F(TableTest, TableManager) {
  TableManager table_manager;
  set<const Table*> table_set;
  static const commands::Request::SpecialRomanjiTable
      special_romanji_table[] = {
    commands::Request::DEFAULT_TABLE,
    commands::Request::TWELVE_KEYS_TO_HIRAGANA,
    commands::Request::TWELVE_KEYS_TO_HALFWIDTHASCII,
    commands::Request::TWELVE_KEYS_TO_NUMBER,
    commands::Request::FLICK_TO_HIRAGANA,
    commands::Request::FLICK_TO_HALFWIDTHASCII,
    commands::Request::FLICK_TO_NUMBER,
    commands::Request::TOGGLE_FLICK_TO_HIRAGANA,
    commands::Request::TOGGLE_FLICK_TO_HALFWIDTHASCII,
    commands::Request::TOGGLE_FLICK_TO_NUMBER,
    commands::Request::GODAN_TO_HIRAGANA,
    commands::Request::QWERTY_MOBILE_TO_HIRAGANA,
    commands::Request::QWERTY_MOBILE_TO_HIRAGANA_NUMBER,
    commands::Request::QWERTY_MOBILE_TO_HALFWIDTHASCII,
  };
  static const config::Config::PreeditMethod preedit_method[] = {
    config::Config::ROMAN,
    config::Config::KANA
  };
  static const config::Config::PunctuationMethod punctuation_method[] = {
    config::Config::KUTEN_TOUTEN,
    config::Config::COMMA_PERIOD,
    config::Config::KUTEN_PERIOD,
    config::Config::COMMA_TOUTEN
  };
  static const config::Config::SymbolMethod symbol_method[] = {
    config::Config::CORNER_BRACKET_MIDDLE_DOT,
    config::Config::SQUARE_BRACKET_SLASH,
    config::Config::CORNER_BRACKET_SLASH,
    config::Config::SQUARE_BRACKET_MIDDLE_DOT
  };

  for (int romanji = 0; romanji < arraysize(special_romanji_table); ++romanji) {
    for (int preedit = 0; preedit < arraysize(preedit_method); ++preedit) {
      for (int punctuation = 0; punctuation < arraysize(punctuation_method);
           ++punctuation) {
        for (int symbol = 0; symbol < arraysize(symbol_method); ++symbol) {
          commands::Request request;
          request.set_special_romanji_table(special_romanji_table[romanji]);
          config::Config config;
          config.set_preedit_method(preedit_method[preedit]);
          config.set_punctuation_method(punctuation_method[punctuation]);
          config.set_symbol_method(symbol_method[symbol]);
          const Table *table = table_manager.GetTable(request, config);
          EXPECT_TRUE(table != NULL);
          EXPECT_TRUE(table_manager.GetTable(request, config) == table);
          EXPECT_TRUE(table_set.find(table) == table_set.end());
          table_set.insert(table);
        }
      }
    }
  }

  {
    // b/6788850.
    const string kRule =
        "a\t[A]\n";  // 2 entry rule

    commands::Request request;
    request.set_special_romanji_table(Request::DEFAULT_TABLE);
    config::Config config;
    config.set_preedit_method(Config::ROMAN);
    config.set_punctuation_method(Config::KUTEN_TOUTEN);
    config.set_symbol_method(Config::CORNER_BRACKET_MIDDLE_DOT);
    config.set_custom_roman_table(kRule);
    const Table *table = table_manager.GetTable(request, config);
    EXPECT_TRUE(table != NULL);
    EXPECT_TRUE(table_manager.GetTable(request, config) == table);
    EXPECT_TRUE(NULL != table->LookUp("a"));
    EXPECT_TRUE(NULL == table->LookUp("kk"));

    const string kRule2 =
        "a\t[A]\n"  // 2 entry rule
        "kk\t[X]\tk\n";  // 3 entry rule
    config.set_custom_roman_table(kRule2);
    const Table *table2 = table_manager.GetTable(request, config);
    EXPECT_TRUE(table2 != NULL);
    EXPECT_TRUE(table_manager.GetTable(request, config) == table2);
    EXPECT_TRUE(NULL != table2->LookUp("a"));
    EXPECT_TRUE(NULL != table2->LookUp("kk"));
  }
}

}  // namespace composer
}  // namespace mozc
