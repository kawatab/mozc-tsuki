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

#include "config/config_handler.h"

#ifdef OS_WIN
#include <windows.h>
#endif  // OS_WIN

#include <atomic>
#include <string>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/mozc_hash_set.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/util.h"
#include "protocol/config.pb.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"

namespace mozc {
namespace config {
namespace {

class ConfigHandlerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
    default_config_filename_ = ConfigHandler::GetConfigFileName();
    Config default_config;
    ConfigHandler::GetDefaultConfig(&default_config);
    ConfigHandler::SetConfig(default_config);
  }

  virtual void TearDown() {
    ConfigHandler::SetConfigFileName(default_config_filename_);
    Config default_config;
    ConfigHandler::GetDefaultConfig(&default_config);
    ConfigHandler::SetConfig(default_config);
  }
 private:
  string default_config_filename_;
};

class ScopedSetConfigFileName {
 public:
  explicit ScopedSetConfigFileName(const string &new_name)
      : default_config_filename_(ConfigHandler::GetConfigFileName()) {
    ConfigHandler::SetConfigFileName(new_name);
  }

  ~ScopedSetConfigFileName() {
    ConfigHandler::SetConfigFileName(default_config_filename_);
  }

 private:
  string default_config_filename_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ScopedSetConfigFileName);
};

TEST_F(ConfigHandlerTest, SetConfig) {
  Config input;
  Config output;

  const string config_file = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                                "mozc_config_test_tmp");
  FileUtil::Unlink(config_file);
  ScopedSetConfigFileName scoped_config_file_name(config_file);
  EXPECT_EQ(config_file, ConfigHandler::GetConfigFileName());
  ASSERT_TRUE(ConfigHandler::Reload())
      << "failed to reload: " << ConfigHandler::GetConfigFileName();

  ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(true);
#ifndef NO_LOGGING
  input.set_verbose_level(2);
#endif  // NO_LOGGING
  ConfigHandler::SetMetaData(&input);
  EXPECT_TRUE(ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(ConfigHandler::GetConfig(&output));
  input.mutable_general_config()->set_last_modified_time(0);
  output.mutable_general_config()->set_last_modified_time(0);
  EXPECT_EQ(input.DebugString(), output.DebugString());

  ConfigHandler::GetDefaultConfig(&input);
  input.set_incognito_mode(false);
#ifndef NO_LOGGING
  input.set_verbose_level(0);
#endif  // NO_LOGGING
  ConfigHandler::SetMetaData(&input);
  EXPECT_TRUE(ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(ConfigHandler::GetConfig(&output));

  input.mutable_general_config()->set_last_modified_time(0);
  output.mutable_general_config()->set_last_modified_time(0);
  EXPECT_EQ(input.DebugString(), output.DebugString());

#if defined(OS_ANDROID) && defined(CHANNEL_DEV)
  input.Clear();
  EXPECT_FALSE(input.general_config().has_upload_usage_stats());
  EXPECT_TRUE(ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(ConfigHandler::GetConfig(&output));
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());

  input.Clear();
  input.mutable_general_config()->set_upload_usage_stats(false);
  EXPECT_TRUE(input.general_config().has_upload_usage_stats());
  EXPECT_FALSE(input.general_config().upload_usage_stats());
  EXPECT_TRUE(ConfigHandler::SetConfig(input));
  output.Clear();
  EXPECT_TRUE(ConfigHandler::GetConfig(&output));
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());
#endif  // OS_ANDROID && CHANNEL_DEV
}

TEST_F(ConfigHandlerTest, SetImposedConfig) {
  Config input;
  Config output;

  const string config_file = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                                "mozc_config_test_tmp");
  FileUtil::Unlink(config_file);
  ScopedSetConfigFileName scoped_config_file_name(config_file);
  ASSERT_TRUE(ConfigHandler::Reload())
      << "failed to reload: " << ConfigHandler::GetConfigFileName();

  struct Testcase {
    bool stored_config_value;
    enum {DO_NOT_IMPOSE, IMPOSE_TRUE, IMPOSE_FALSE} imposed_config_value;
    bool expected_value;
  };
  const Testcase kTestcases[] = {
    {true, Testcase::IMPOSE_TRUE, true},
    {true, Testcase::IMPOSE_FALSE, false},
    {false, Testcase::IMPOSE_TRUE, true},
    {false, Testcase::IMPOSE_FALSE, false},
    {true, Testcase::DO_NOT_IMPOSE, true},
    {false, Testcase::DO_NOT_IMPOSE, false},
  };

  for (size_t i = 0; i < arraysize(kTestcases); ++i) {
    const bool stored_config_value = kTestcases[i].stored_config_value;
    const bool expected = kTestcases[i].expected_value;

    // Set current config.
    ConfigHandler::GetDefaultConfig(&input);
    input.set_incognito_mode(stored_config_value);
    ConfigHandler::SetMetaData(&input);
    EXPECT_TRUE(ConfigHandler::SetConfig(input));
    // Set imposed config.
    input.Clear();
    if (kTestcases[i].imposed_config_value != Testcase::DO_NOT_IMPOSE) {
      input.set_incognito_mode(
          kTestcases[i].imposed_config_value == Testcase::IMPOSE_TRUE);
    }
    ConfigHandler::SetImposedConfig(input);
    // Check post-condition.
    output.Clear();
    EXPECT_TRUE(ConfigHandler::GetConfig(&output));
    EXPECT_EQ(expected, output.incognito_mode());
    ConfigHandler::GetConfig(&output);
    EXPECT_EQ(expected, output.incognito_mode());
    ConfigHandler::GetStoredConfig(&output);
    EXPECT_EQ(stored_config_value, output.incognito_mode());

    // Reload and check.
    ASSERT_TRUE(ConfigHandler::Reload())
        << "failed to reload: " << ConfigHandler::GetConfigFileName();
    output.Clear();
    EXPECT_TRUE(ConfigHandler::GetConfig(&output));
    EXPECT_EQ(expected, output.incognito_mode());
    ConfigHandler::GetConfig(&output);
    EXPECT_EQ(expected, output.incognito_mode());
    ConfigHandler::GetStoredConfig(&output);
    EXPECT_EQ(stored_config_value, output.incognito_mode());

    // Unset imposed config.
    input.Clear();
    ConfigHandler::SetImposedConfig(input);
    // Check post-condition.
    output.Clear();
    EXPECT_TRUE(ConfigHandler::GetConfig(&output));
    EXPECT_EQ(stored_config_value, output.incognito_mode());
    ConfigHandler::GetConfig(&output);
    EXPECT_EQ(stored_config_value, output.incognito_mode());
    ConfigHandler::GetStoredConfig(&output);
    EXPECT_EQ(stored_config_value, output.incognito_mode());
  }
}

TEST_F(ConfigHandlerTest, ConfigFileNameConfig) {
  const string config_file =
      string("config") + std::to_string(config::CONFIG_VERSION);

  const string filename = FileUtil::JoinPath(FLAGS_test_tmpdir, config_file);
  FileUtil::Unlink(filename);
  Config input;
  ConfigHandler::SetConfig(input);
  FileUtil::FileExists(filename);
}

TEST_F(ConfigHandlerTest, SetConfigFileName) {
  Config mozc_config;
  const bool default_incognito_mode = mozc_config.incognito_mode();
  mozc_config.set_incognito_mode(!default_incognito_mode);
  ConfigHandler::SetConfig(mozc_config);
  // ScopedSetConfigFileName internally calls SetConfigFileName.
  ScopedSetConfigFileName scoped_config_file_name(
      "memory://set_config_file_name_test.db");
  // After SetConfigFileName called, settings are set as default.
  Config updated_config;
  ConfigHandler::GetConfig(&updated_config);
  EXPECT_EQ(default_incognito_mode, updated_config.incognito_mode());
}

#if !defined(OS_ANDROID) && !defined(OS_NACL)
// Temporarily disable this test because FileUtil::CopyFile fails on
// Android for some reason, and on NaCl since it uses mock file system and the
// mock file system doesn't have a source file.
// TODO(hsumita): Enable this test on Android and NaCl.
TEST_F(ConfigHandlerTest, LoadTestConfig) {
  // TODO(yukawa): Generate test data automatically so that we can keep
  //     the compatibility among variety of config files.
  // TODO(yukawa): Enumerate test data in the directory automatically.
  const char *kDataFiles[] = {
    "linux_config1.db",
    "mac_config1.db",
    "win_config1.db",
  };

  for (size_t i = 0; i < arraysize(kDataFiles); ++i) {
    const char *file_name = kDataFiles[i];
    const string &src_path = mozc::testing::GetSourceFileOrDie({
        "data", "test", "config", file_name});
    const string &dest_path = FileUtil::JoinPath(
        SystemUtil::GetUserProfileDirectory(), file_name);
    ASSERT_TRUE(FileUtil::CopyFile(src_path, dest_path))
        << "Copy failed: " << src_path << " to " << dest_path;

    ScopedSetConfigFileName scoped_config_file_name(
        "user://" + string(file_name));
    ASSERT_TRUE(ConfigHandler::Reload())
        << "failed to reload: " << ConfigHandler::GetConfigFileName();

    Config default_config;
    EXPECT_TRUE(ConfigHandler::GetConfig(&default_config))
        << "failed to GetConfig from: " << file_name;

#ifdef OS_WIN
    // Reset the file attributes since it may contain FILE_ATTRIBUTE_READONLY.
    std::wstring wdest_path;
    Util::UTF8ToWide(dest_path, &wdest_path);
    ::SetFileAttributesW(wdest_path.c_str(), FILE_ATTRIBUTE_NORMAL);
#endif  // OS_WIN

    // Remove test file just in case.
    ASSERT_TRUE(FileUtil::Unlink(dest_path));
    EXPECT_FALSE(FileUtil::FileExists(dest_path));
  }
}
#endif  // !OS_ANDROID && !OS_NACL

TEST_F(ConfigHandlerTest, GetDefaultConfig) {
  Config output;

  output.Clear();
  ConfigHandler::GetDefaultConfig(&output);
#ifdef OS_MACOSX
  EXPECT_EQ(output.session_keymap(), Config::KOTOERI);
#elif defined(OS_NACL)  // OS_MACOSX
  EXPECT_EQ(output.session_keymap(), Config::CHROMEOS);
#else  // OS_MACOSX || OS_NACL
  EXPECT_EQ(output.session_keymap(), Config::MSIME);
#endif  // OS_MACOSX || OS_NACL
  EXPECT_EQ(output.character_form_rules_size(), 13);

  struct TestCase {
    const char *group;
    Config::CharacterForm preedit_character_form;
    Config::CharacterForm conversion_character_form;
  };
  const TestCase testcases[] = {
    // "ア"
    {"ア", Config::FULL_WIDTH, Config::FULL_WIDTH},
    {"A", Config::FULL_WIDTH, Config::LAST_FORM},
    {"0", Config::FULL_WIDTH, Config::LAST_FORM},
    {"(){}[]", Config::FULL_WIDTH, Config::LAST_FORM},
    {".,", Config::FULL_WIDTH, Config::LAST_FORM},
    // "。、",
    {"。、",
      Config::FULL_WIDTH, Config::FULL_WIDTH},
    // "・「」"
    {"・「」",
      Config::FULL_WIDTH, Config::FULL_WIDTH},
    {"\"'", Config::FULL_WIDTH, Config::LAST_FORM},
    {":;", Config::FULL_WIDTH, Config::LAST_FORM},
    {"#%&@$^_|`\\", Config::FULL_WIDTH, Config::LAST_FORM},
    {"~", Config::FULL_WIDTH, Config::LAST_FORM},
    {"<>=+-/*", Config::FULL_WIDTH, Config::LAST_FORM},
    {"?!", Config::FULL_WIDTH, Config::LAST_FORM},
  };
  EXPECT_EQ(output.character_form_rules_size(), arraysize(testcases));
  for (size_t i = 0; i < arraysize(testcases); ++i) {
    EXPECT_EQ(output.character_form_rules(i).group(),
              testcases[i].group);
    EXPECT_EQ(output.character_form_rules(i).preedit_character_form(),
              testcases[i].preedit_character_form);
    EXPECT_EQ(output.character_form_rules(i).conversion_character_form(),
              testcases[i].conversion_character_form);
  }

#if defined(OS_ANDROID) && defined(CHANNEL_DEV)
  EXPECT_TRUE(output.general_config().has_upload_usage_stats());
  EXPECT_TRUE(output.general_config().upload_usage_stats());
#endif  // OS_ANDROID && CHANNEL_DEV
}

TEST_F(ConfigHandlerTest, DefaultConfig) {
  Config config;
  ConfigHandler::GetDefaultConfig(&config);
  EXPECT_EQ(config.DebugString(),
            ConfigHandler::DefaultConfig().DebugString());
}

class SetConfigThread final : public Thread {
 public:
  explicit SetConfigThread(const std::vector<Config> &configs)
      : quitting_(false),
        configs_(configs) {
  }

  ~SetConfigThread() override {
    quitting_ = true;
    Join();
  }

 protected:
  void Run() override {
    while (!quitting_) {
      const size_t next_index = Util::Random(configs_.size());
      ConfigHandler::SetConfig(configs_.at(next_index));
    }
  }

 private:
  std::atomic<bool> quitting_;
  std::vector<Config> configs_;
};

// Returns concatenated serialized data of |Config::character_form_rules|.
string ExtractCharacterFormRules(const Config &config) {
  string rules;
  for (size_t i = 0; i < config.character_form_rules_size(); ++i) {
    config.character_form_rules(i).AppendToString(&rules);
  }
  return rules;
}

class GetConfigThread final : public Thread {
 public:
  explicit GetConfigThread(
      const mozc_hash_set<string> &character_form_rules_set)
      : quitting_(false),
        character_form_rules_set_(character_form_rules_set) {
  }

  ~GetConfigThread() override {
    quitting_ = true;
    Join();
  }

 protected:
  void Run() override {
    while (!quitting_) {
      Config config;
      ConfigHandler::GetConfig(&config);
      const auto &rules = ExtractCharacterFormRules(config);
      EXPECT_NE(character_form_rules_set_.end(),
                character_form_rules_set_.find(rules));
    }
  }

 private:
  std::atomic<bool> quitting_;
  const mozc_hash_set<string> character_form_rules_set_;
};


TEST_F(ConfigHandlerTest, ConcurrentAccess) {
  std::vector<Config> configs;

  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    configs.push_back(config);
  }
  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    config.clear_character_form_rules();
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("0");
      rule->set_preedit_character_form(Config::HALF_WIDTH);
      rule->set_conversion_character_form(Config::HALF_WIDTH);
    }
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("A");
      rule->set_preedit_character_form(Config::LAST_FORM);
      rule->set_conversion_character_form(Config::LAST_FORM);
    }
    configs.push_back(config);
  }
  {
    Config config;
    ConfigHandler::GetDefaultConfig(&config);
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("0");
      rule->set_preedit_character_form(Config::HALF_WIDTH);
      rule->set_conversion_character_form(Config::HALF_WIDTH);
    }
    {
      auto *rule = config.add_character_form_rules();
      rule->set_group("A");
      rule->set_preedit_character_form(Config::LAST_FORM);
      rule->set_conversion_character_form(Config::LAST_FORM);
    }
    configs.push_back(config);
  }

  // Since |ConfigHandler::SetConfig()| actually updates some metadata in
  // |GeneralConfig|, the returned object from |ConfigHandler::GetConfig()|
  // is not predictable.  Hence we only make sure that
  // |Config::character_form_rules()| is one of expected values.
  mozc_hash_set<string> character_form_rules_set;
  for (const auto &config : configs) {
    character_form_rules_set.insert(ExtractCharacterFormRules(config));
  }

  // Before starting concurrent test, check to see if it works in single
  // thread.
  for (const auto &config : configs) {
    // Update the global config.
    ConfigHandler::SetConfig(config);

    // Check to see if the returned config contains one of expected
    // |Config::character_form_rules()|.
    Config returned_config;
    ConfigHandler::GetConfig(&returned_config);
    const auto &rules = ExtractCharacterFormRules(returned_config);
    ASSERT_NE(character_form_rules_set.end(),
              character_form_rules_set.find(rules));
  }

  // 250 msec is good enough to crash the code if it is not guarded by
  // the lock, but feel free to change the duration.  It is basically an
  // arbitrary number.
  const uint32 kTestDurationMSec = 250;  // 250 msec
  const size_t kNumSetThread = 2;
  const size_t kNumGetThread = 4;
  {
    // Set up background threads for concurrent access.
    std::vector<std::unique_ptr<SetConfigThread>> set_threads;
    for (size_t i = 0; i < kNumSetThread; ++i) {
      set_threads.emplace_back(std::unique_ptr<SetConfigThread>(
          new SetConfigThread(configs)));
    }
    std::vector<std::unique_ptr<GetConfigThread>> get_threads;
    for (size_t i = 0; i < kNumGetThread; ++i) {
      get_threads.emplace_back(std::unique_ptr<GetConfigThread>(
          new GetConfigThread(character_form_rules_set)));
    }
    // Let background threads start accessing ConfigHandler from multiple
    // background threads.
    for (size_t i = 0; i < set_threads.size(); ++i) {
      set_threads[i]->Start(
          Util::StringPrintf("SetConfigThread%d", static_cast<int>(i)));
    }
    for (size_t i = 0; i < get_threads.size(); ++i) {
      get_threads[i]->Start(
          Util::StringPrintf("GetConfigThread%d", static_cast<int>(i)));
    }
    // Wait for a while to see if everything goes well.
    Util::Sleep(kTestDurationMSec);
    // Destructors of |SetConfigThread| and |GetConfigThread| will take
    // care of their background threads (in a blocking way).
    set_threads.clear();
    get_threads.clear();
  }
}

}  // namespace
}  // namespace config
}  // namespace mozc
