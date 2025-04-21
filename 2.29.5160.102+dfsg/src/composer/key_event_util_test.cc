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

#include "composer/key_event_util.h"

#include <cstdint>
#include <iterator>
#include <string>

#include "base/util.h"
#include "composer/key_parser.h"
#include "protocol/commands.pb.h"
#include "testing/gunit.h"
#include "absl/strings/str_format.h"

namespace mozc {
using commands::KeyEvent;

namespace {
::testing::AssertionResult CompareKeyEvent(const char *expected_expr,
                                           const char *actual_expr,
                                           const KeyEvent &expected,
                                           const KeyEvent &actual) {
  {  // Key code
    const int expected_key_code =
        (expected.has_key_code()) ? expected.key_code() : -1;
    const int actual_key_code =
        (actual.has_key_code()) ? actual.key_code() : -1;
    if (expected_key_code != actual_key_code) {
      const std::string expected_value =
          (expected_key_code == -1)
              ? "None"
              : absl::StrFormat("%c (%d)", expected_key_code,
                                expected_key_code);
      const std::string actual_value =
          (actual_key_code == -1)
              ? std::string("None")
              : absl::StrFormat("%c (%d)", actual_key_code, actual_key_code);
      return ::testing::AssertionFailure()
             << "Key codes are not same\n"
             << "Expected: " << expected_value << "\n"
             << "Actual  : " << actual_value;
    }
  }

  {  // Special key
    const int expected_special_key =
        (expected.has_special_key()) ? expected.special_key() : -1;
    const int actual_special_key =
        (actual.has_special_key()) ? actual.special_key() : -1;
    if (expected_special_key != actual_special_key) {
      const std::string expected_value =
          (expected_special_key == -1) ? "None"
                                       : std::to_string(expected_special_key);
      const std::string actual_value = (actual_special_key == -1)
                                           ? "None"
                                           : std::to_string(actual_special_key);
      return ::testing::AssertionFailure()
             << "Special keys are not same\n"
             << "Expected: " << expected_value << "\n"
             << "Actual  : " << actual_value;
    }
  }

  {  // Modifier keys
    const int expected_modifier_keys = KeyEventUtil::GetModifiers(expected);
    const int actual_modifier_keys = KeyEventUtil::GetModifiers(actual);
    if (expected_modifier_keys != actual_modifier_keys) {
      return ::testing::AssertionFailure()
             << "Modifier keys are not same\n"
             << "Expected: " << expected_modifier_keys << "\n"
             << "Actual  : " << actual_modifier_keys;
    }
  }

  return ::testing::AssertionSuccess();
}

#define EXPECT_EQ_KEY_EVENT(actual, expected) \
  EXPECT_PRED_FORMAT2(CompareKeyEvent, expected, actual)
}  // namespace

TEST(KeyEventUtilTest, GetModifiers) {
  KeyEvent key_event;

  KeyParser::ParseKey("a", &key_event);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), 0);

  KeyParser::ParseKey("Alt", &key_event);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), KeyEvent::ALT);

  KeyParser::ParseKey("Ctrl", &key_event);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), KeyEvent::CTRL);

  KeyParser::ParseKey("Shift", &key_event);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), KeyEvent::SHIFT);

  KeyParser::ParseKey("Caps", &key_event);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event), KeyEvent::CAPS);

  KeyParser::ParseKey("LeftAlt RightAlt", &key_event);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event),
            (KeyEvent::ALT | KeyEvent::LEFT_ALT | KeyEvent::RIGHT_ALT));

  KeyParser::ParseKey("LeftAlt Ctrl RightShift", &key_event);
  EXPECT_EQ(KeyEventUtil::GetModifiers(key_event),
            (KeyEvent::ALT | KeyEvent::LEFT_ALT | KeyEvent::CTRL |
             KeyEvent::SHIFT | KeyEvent::RIGHT_SHIFT));
}

TEST(KeyEventUtilTest, GetKeyInformation) {
  const char *kTestKeys[] = {
      "a",
      "Space",
      "Shift",
      "Shift a",
      "Shift Space",
      "Space a",
      "LeftShift Space a",
  };

  KeyEvent key_event;
  uint64_t output;

  for (size_t i = 0; i < std::size(kTestKeys); ++i) {
    SCOPED_TRACE(kTestKeys[i]);
    KeyParser::ParseKey(kTestKeys[i], &key_event);
    ASSERT_TRUE(KeyEventUtil::GetKeyInformation(key_event, &output));

    uint64_t expected = 0;
    if (key_event.has_key_code()) {
      expected |= static_cast<uint64_t>(key_event.key_code());
    }
    if (key_event.has_special_key()) {
      expected |= static_cast<uint64_t>(key_event.special_key()) << 32;
    }
    expected |= static_cast<uint64_t>(KeyEventUtil::GetModifiers(key_event))
                << 48;

    EXPECT_EQ(output, expected);
  }

  constexpr uint32_t kEscapeKeyCode = 27;
  key_event.Clear();
  key_event.set_key_code(kEscapeKeyCode);
  // Escape key should not set on key_code field.
  EXPECT_FALSE(KeyEventUtil::GetKeyInformation(key_event, &output));
}

TEST(KeyEventUtilTest, NormalizeModifiers) {
  KeyEvent key_event;
  KeyEvent normalized_key_event;

  {  // Removes caps
    KeyParser::ParseKey("CAPS H", &key_event);
    ASSERT_EQ(1, key_event.modifier_keys_size());
    ASSERT_EQ(KeyEvent::CAPS, KeyEventUtil::GetModifiers(key_event));
    ASSERT_EQ('H', key_event.key_code());

    KeyEventUtil::NormalizeModifiers(key_event, &normalized_key_event);
    EXPECT_EQ(normalized_key_event.modifier_keys_size(), 0);
    EXPECT_EQ(normalized_key_event.key_code(), 'h');
  }

  {  // Removes left_shift
    KeyParser::ParseKey("LeftShift", &key_event);
    ASSERT_EQ(2, key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT),
              KeyEventUtil::GetModifiers(key_event));

    KeyEventUtil::NormalizeModifiers(key_event, &normalized_key_event);
    EXPECT_EQ(normalized_key_event.modifier_keys_size(), 1);
    ASSERT_EQ(KeyEvent::SHIFT,
              KeyEventUtil::GetModifiers(normalized_key_event));
  }

  {  // Removes caps and left_shift
    KeyParser::ParseKey("CAPS LeftShift H", &key_event);
    ASSERT_EQ(3, key_event.modifier_keys_size());
    ASSERT_EQ((KeyEvent::CAPS | KeyEvent::SHIFT | KeyEvent::LEFT_SHIFT),
              KeyEventUtil::GetModifiers(key_event));

    ASSERT_EQ('H', key_event.key_code());

    KeyEventUtil::NormalizeModifiers(key_event, &normalized_key_event);
    EXPECT_EQ(normalized_key_event.modifier_keys_size(), 1);
    EXPECT_EQ(KeyEventUtil::GetModifiers(normalized_key_event),
              KeyEvent::SHIFT);
    EXPECT_EQ(normalized_key_event.key_code(), 'h');
  }
}

TEST(KeyEventUtilTest, NormalizeNumpadKey) {
  const struct NormalizeNumpadKeyTestData {
    const char *from;
    const char *to;
  } kNormalizeNumpadKeyTestData[] = {
      {"a", "a"},
      {"Shift", "Shift"},
      {"Caps", "Caps"},
      {"Enter", "Enter"},
      {"Shift Caps a", "Shift Caps a"},
      {"NUMPAD0", "0"},
      {"NUMPAD9", "9"},
      {"MULTIPLY", "*"},
      {"SEPARATOR", "Enter"},
      {"EQUALS", "="},
      {"Ctrl NUMPAD0", "Ctrl 0"},
      {"NUMPAD0 a", "0"},
  };

  for (size_t i = 0; i < std::size(kNormalizeNumpadKeyTestData); ++i) {
    const NormalizeNumpadKeyTestData &data = kNormalizeNumpadKeyTestData[i];
    SCOPED_TRACE(data.from);

    KeyEvent key_event_from, key_event_to, key_event_normalized;
    KeyParser::ParseKey(data.from, &key_event_from);
    KeyParser::ParseKey(data.to, &key_event_to);
    KeyEventUtil::NormalizeNumpadKey(key_event_from, &key_event_normalized);
    EXPECT_EQ_KEY_EVENT(key_event_normalized, key_event_to);
  }
}

TEST(KeyEventUtilTest, MaybeGetKeyStub) {
  KeyEvent key_event;
  KeyInformation key;

  KeyParser::ParseKey("Shift", &key_event);
  EXPECT_FALSE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));

  KeyParser::ParseKey("Space", &key_event);
  EXPECT_FALSE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));

  constexpr uint32_t kEscapeKeyCode = 27;
  key_event.Clear();
  key_event.set_key_code(kEscapeKeyCode);
  EXPECT_FALSE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));

  KeyParser::ParseKey("a", &key_event);
  EXPECT_TRUE(KeyEventUtil::MaybeGetKeyStub(key_event, &key));
  EXPECT_EQ(key, static_cast<KeyInformation>(KeyEvent::TEXT_INPUT) << 32);
}

TEST(KeyEventUtilTest, RemoveModifiers) {
  const struct RemoveModifiersTestData {
    const char *input;
    const char *remove;
    const char *output;
  } kRemoveModifiersTestData[] = {
      {
          "",
          "",
          "",
      },
      {
          "Ctrl Shift LeftAlt Caps",
          "Ctrl Shift LeftAlt Caps",
          "",
      },
      {
          "Ctrl Shift LeftAlt Caps",
          "Shift Caps",
          "Ctrl LeftAlt",
      },
      {
          "Ctrl Shift LeftAlt Caps",
          "Alt",
          "Ctrl Shift Caps",
      },
      {
          "",
          "Ctrl Shift LeftAlt Caps",
          "",
      },
  };

  for (size_t i = 0; i < std::size(kRemoveModifiersTestData); ++i) {
    SCOPED_TRACE(absl::StrFormat("index = %d", static_cast<int>(i)));
    const RemoveModifiersTestData &data = kRemoveModifiersTestData[i];

    KeyEvent input, remove, output;
    KeyParser::ParseKey(data.input, &input);
    KeyParser::ParseKey(data.remove, &remove);
    KeyParser::ParseKey(data.output, &output);
    const uint32_t remove_modifiers = KeyEventUtil::GetModifiers(remove);

    KeyEvent removed_key_event;
    KeyEventUtil::RemoveModifiers(input, remove_modifiers, &removed_key_event);
    EXPECT_EQ_KEY_EVENT(removed_key_event, output);
  }
}

TEST(KeyEventUtilTest, HasModifiers) {
  EXPECT_TRUE(KeyEventUtil::HasAlt(KeyEvent::ALT));
  EXPECT_TRUE(KeyEventUtil::HasAlt(KeyEvent::LEFT_ALT));
  EXPECT_TRUE(KeyEventUtil::HasAlt(KeyEvent::ALT | KeyEvent::CTRL));
  EXPECT_FALSE(KeyEventUtil::HasAlt(0));
  EXPECT_FALSE(KeyEventUtil::HasAlt(KeyEvent::CTRL));
  EXPECT_FALSE(KeyEventUtil::HasAlt(KeyEvent::SHIFT));

  EXPECT_TRUE(KeyEventUtil::HasCtrl(KeyEvent::CTRL));
  EXPECT_TRUE(KeyEventUtil::HasCtrl(KeyEvent::LEFT_CTRL));
  EXPECT_TRUE(KeyEventUtil::HasCtrl(KeyEvent::CTRL | KeyEvent::SHIFT));
  EXPECT_FALSE(KeyEventUtil::HasCtrl(0));
  EXPECT_FALSE(KeyEventUtil::HasCtrl(KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasCtrl(KeyEvent::SHIFT));

  EXPECT_TRUE(KeyEventUtil::HasShift(KeyEvent::SHIFT));
  EXPECT_TRUE(KeyEventUtil::HasShift(KeyEvent::LEFT_SHIFT));
  EXPECT_TRUE(KeyEventUtil::HasShift(KeyEvent::SHIFT | KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasShift(0));
  EXPECT_FALSE(KeyEventUtil::HasShift(KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasShift(KeyEvent::CTRL));

  EXPECT_TRUE(KeyEventUtil::HasCaps(KeyEvent::CAPS));
  EXPECT_TRUE(KeyEventUtil::HasCaps(KeyEvent::CAPS | KeyEvent::ALT));
  EXPECT_FALSE(KeyEventUtil::HasCaps(0));
  EXPECT_FALSE(KeyEventUtil::HasCaps(KeyEvent::CTRL));
}

TEST(KeyEventUtilTest, IsModifiers) {
  const struct IsModifiersTestData {
    uint32_t modifiers;
    bool is_alt;
    bool is_ctrl;
    bool is_shift;
    bool is_alt_ctrl;
    bool is_alt_shift;
    bool is_ctrl_shift;
    bool is_alt_ctrl_shift;
  } kIsModifiersTestData[] = {
      {0, false, false, false, false, false, false, false},
      {
          KeyEvent::ALT,
          true,
          false,
          false,
          false,
          false,
          false,
          false,
      },
      {
          KeyEvent::CTRL,
          false,
          true,
          false,
          false,
          false,
          false,
          false,
      },
      {
          KeyEvent::SHIFT,
          false,
          false,
          true,
          false,
          false,
          false,
          false,
      },
      {
          KeyEvent::ALT | KeyEvent::CTRL,
          false,
          false,
          false,
          true,
          false,
          false,
          false,
      },
      {
          KeyEvent::ALT | KeyEvent::SHIFT,
          false,
          false,
          false,
          false,
          true,
          false,
          false,
      },
      {
          KeyEvent::CTRL | KeyEvent::SHIFT,
          false,
          false,
          false,
          false,
          false,
          true,
          false,
      },
      {
          KeyEvent::ALT | KeyEvent::CTRL | KeyEvent::SHIFT,
          false,
          false,
          false,
          false,
          false,
          false,
          true,
      },
      {
          KeyEvent::LEFT_ALT,
          true,
          false,
          false,
          false,
          false,
          false,
          false,
      },
      {
          KeyEvent::ALT | KeyEvent::LEFT_ALT | KeyEvent::RIGHT_ALT,
          true,
          false,
          false,
          false,
          false,
          false,
          false,
      },
      {
          KeyEvent::CAPS,
          false,
          false,
          false,
          false,
          false,
          false,
          false,
      },
      {
          KeyEvent::ALT | KeyEvent::CAPS,
          true,
          false,
          false,
          false,
          false,
          false,
          false,
      },
  };

  for (size_t i = 0; i < std::size(kIsModifiersTestData); ++i) {
    const IsModifiersTestData &data = kIsModifiersTestData[i];
    SCOPED_TRACE(absl::StrFormat("index: %d", static_cast<int>(i)));

    EXPECT_EQ(KeyEventUtil::IsAlt(data.modifiers), data.is_alt);
    EXPECT_EQ(KeyEventUtil::IsCtrl(data.modifiers), data.is_ctrl);
    EXPECT_EQ(KeyEventUtil::IsShift(data.modifiers), data.is_shift);
    EXPECT_EQ(KeyEventUtil::IsAltCtrl(data.modifiers), data.is_alt_ctrl);
    EXPECT_EQ(KeyEventUtil::IsAltShift(data.modifiers), data.is_alt_shift);
    EXPECT_EQ(KeyEventUtil::IsCtrlShift(data.modifiers), data.is_ctrl_shift);
    EXPECT_EQ(KeyEventUtil::IsAltCtrlShift(data.modifiers),
              data.is_alt_ctrl_shift);
  }
}

TEST(KeyEventUtilTest, IsLowerUpperAlphabet) {
  const struct IsLowerUpperAlphabetTestData {
    const char *key;
    bool is_lower;
    bool is_upper;
  } kIsLowerUpperAlphabetTestData[] = {
      {"a", true, false},
      {"A", false, true},
      {"Shift a", false, true},
      {"Shift A", true, false},
      {"Shift Caps a", true, false},
      {"Shift Caps A", false, true},
      {"0", false, false},
      {"Shift", false, false},
      {"Caps", false, false},
      {"Space", false, false},
  };

  for (size_t i = 0; i < std::size(kIsLowerUpperAlphabetTestData); ++i) {
    const IsLowerUpperAlphabetTestData &data = kIsLowerUpperAlphabetTestData[i];
    SCOPED_TRACE(data.key);
    KeyEvent key_event;
    KeyParser::ParseKey(data.key, &key_event);
    EXPECT_EQ(KeyEventUtil::IsLowerAlphabet(key_event), data.is_lower);
    EXPECT_EQ(KeyEventUtil::IsUpperAlphabet(key_event), data.is_upper);
  }
}

TEST(KeyEventUtilTest, IsNumpadKey) {
  const struct IsNumpadKeyTestData {
    const char *key;
    bool is_numpad_key;
  } kIsNumpadKeyTestData[] = {
      {"a", false},       {"A", false},      {"Shift", false},
      {"Shift a", false}, {"0", false},      {"EISU", false},
      {"NUMPAD0", true},  {"NUMPAD9", true}, {"MULTIPLY", true},
      {"EQUALS", true},   {"COMMA", true},   {"TEXTINPUT", false},
  };

  for (size_t i = 0; i < std::size(kIsNumpadKeyTestData); ++i) {
    const IsNumpadKeyTestData &data = kIsNumpadKeyTestData[i];
    SCOPED_TRACE(data.key);
    KeyEvent key_event;
    KeyParser::ParseKey(data.key, &key_event);
    EXPECT_EQ(KeyEventUtil::IsNumpadKey(key_event), data.is_numpad_key);
  }
}

}  // namespace mozc
