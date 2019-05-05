// Modified code
// Copyright 2019, Yasuhiro Yamakawa <kawatab@yahoo.co.jp>
// All rights reserved.
//
// - Added a map of Tsuki layout for US keyboard, tsuki_map_us
// - Added a map of Tsuki layout for JP keyboard, tsuki_map_jp
// - Added a function for Tsuki layout
//     bool KeyTranslator::IsTsukiAvailable(guint keyval,
//                                          guint keycode,
//                                          guint modifiers,
//                                          bool layout_is_jp,
//                                          string *out) const
// - Modified the function:
//     bool KeyTranslator::Translate(guint keyval,
//                                   guint keycode,
//                                   guint modifiers,
//                                   config::Config::PreeditMethod method,
//                                   bool layout_is_jp,
//                                   commands::KeyEvent *out_event) const
//

// Original code
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

#include "unix/ibus/key_translator.h"

#include <map>
#include <set>
#include <string>

#include "base/logging.h"
#include "base/port.h"

namespace {

const struct SpecialKeyMap {
  guint from;
  mozc::commands::KeyEvent::SpecialKey to;
} special_key_map[] = {
  {IBUS_space, mozc::commands::KeyEvent::SPACE},
  {IBUS_Return, mozc::commands::KeyEvent::ENTER},
  {IBUS_Left, mozc::commands::KeyEvent::LEFT},
  {IBUS_Right, mozc::commands::KeyEvent::RIGHT},
  {IBUS_Up, mozc::commands::KeyEvent::UP},
  {IBUS_Down, mozc::commands::KeyEvent::DOWN},
  {IBUS_Escape, mozc::commands::KeyEvent::ESCAPE},
  {IBUS_Delete, mozc::commands::KeyEvent::DEL},
  {IBUS_BackSpace, mozc::commands::KeyEvent::BACKSPACE},
  {IBUS_Insert, mozc::commands::KeyEvent::INSERT},
  {IBUS_Henkan, mozc::commands::KeyEvent::HENKAN},
  {IBUS_Muhenkan, mozc::commands::KeyEvent::MUHENKAN},
  {IBUS_Hiragana, mozc::commands::KeyEvent::KANA},
  {IBUS_Hiragana_Katakana, mozc::commands::KeyEvent::KANA},
  {IBUS_Katakana, mozc::commands::KeyEvent::KATAKANA},
  {IBUS_Zenkaku, mozc::commands::KeyEvent::HANKAKU},
  {IBUS_Hankaku, mozc::commands::KeyEvent::HANKAKU},
  {IBUS_Zenkaku_Hankaku, mozc::commands::KeyEvent::HANKAKU},
  {IBUS_Eisu_toggle, mozc::commands::KeyEvent::EISU},
  {IBUS_Home, mozc::commands::KeyEvent::HOME},
  {IBUS_End, mozc::commands::KeyEvent::END},
  {IBUS_Tab, mozc::commands::KeyEvent::TAB},
  {IBUS_F1, mozc::commands::KeyEvent::F1},
  {IBUS_F2, mozc::commands::KeyEvent::F2},
  {IBUS_F3, mozc::commands::KeyEvent::F3},
  {IBUS_F4, mozc::commands::KeyEvent::F4},
  {IBUS_F5, mozc::commands::KeyEvent::F5},
  {IBUS_F6, mozc::commands::KeyEvent::F6},
  {IBUS_F7, mozc::commands::KeyEvent::F7},
  {IBUS_F8, mozc::commands::KeyEvent::F8},
  {IBUS_F9, mozc::commands::KeyEvent::F9},
  {IBUS_F10, mozc::commands::KeyEvent::F10},
  {IBUS_F11, mozc::commands::KeyEvent::F11},
  {IBUS_F12, mozc::commands::KeyEvent::F12},
  {IBUS_F13, mozc::commands::KeyEvent::F13},
  {IBUS_F14, mozc::commands::KeyEvent::F14},
  {IBUS_F15, mozc::commands::KeyEvent::F15},
  {IBUS_F16, mozc::commands::KeyEvent::F16},
  {IBUS_F17, mozc::commands::KeyEvent::F17},
  {IBUS_F18, mozc::commands::KeyEvent::F18},
  {IBUS_F19, mozc::commands::KeyEvent::F19},
  {IBUS_F20, mozc::commands::KeyEvent::F20},
  {IBUS_F21, mozc::commands::KeyEvent::F21},
  {IBUS_F22, mozc::commands::KeyEvent::F22},
  {IBUS_F23, mozc::commands::KeyEvent::F23},
  {IBUS_F24, mozc::commands::KeyEvent::F24},
  {IBUS_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {IBUS_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},

  // Keypad (10-key).
  {IBUS_KP_0, mozc::commands::KeyEvent::NUMPAD0},
  {IBUS_KP_1, mozc::commands::KeyEvent::NUMPAD1},
  {IBUS_KP_2, mozc::commands::KeyEvent::NUMPAD2},
  {IBUS_KP_3, mozc::commands::KeyEvent::NUMPAD3},
  {IBUS_KP_4, mozc::commands::KeyEvent::NUMPAD4},
  {IBUS_KP_5, mozc::commands::KeyEvent::NUMPAD5},
  {IBUS_KP_6, mozc::commands::KeyEvent::NUMPAD6},
  {IBUS_KP_7, mozc::commands::KeyEvent::NUMPAD7},
  {IBUS_KP_8, mozc::commands::KeyEvent::NUMPAD8},
  {IBUS_KP_9, mozc::commands::KeyEvent::NUMPAD9},
  {IBUS_KP_Equal, mozc::commands::KeyEvent::EQUALS},  // [=]
  {IBUS_KP_Multiply, mozc::commands::KeyEvent::MULTIPLY},  // [*]
  {IBUS_KP_Add, mozc::commands::KeyEvent::ADD},  // [+]
  {IBUS_KP_Separator, mozc::commands::KeyEvent::SEPARATOR},  // enter
  {IBUS_KP_Subtract, mozc::commands::KeyEvent::SUBTRACT},  // [-]
  {IBUS_KP_Decimal, mozc::commands::KeyEvent::DECIMAL},  // [.]
  {IBUS_KP_Divide, mozc::commands::KeyEvent::DIVIDE},  // [/]
  {IBUS_KP_Space, mozc::commands::KeyEvent::SPACE},
  {IBUS_KP_Tab, mozc::commands::KeyEvent::TAB},
  {IBUS_KP_Enter, mozc::commands::KeyEvent::ENTER},
  {IBUS_KP_Home, mozc::commands::KeyEvent::HOME},
  {IBUS_KP_Left, mozc::commands::KeyEvent::LEFT},
  {IBUS_KP_Up, mozc::commands::KeyEvent::UP},
  {IBUS_KP_Right, mozc::commands::KeyEvent::RIGHT},
  {IBUS_KP_Down, mozc::commands::KeyEvent::DOWN},
  {IBUS_KP_Page_Up, mozc::commands::KeyEvent::PAGE_UP},
  {IBUS_KP_Page_Down, mozc::commands::KeyEvent::PAGE_DOWN},
  {IBUS_KP_End, mozc::commands::KeyEvent::END},
  {IBUS_KP_Delete, mozc::commands::KeyEvent::DEL},
  {IBUS_KP_Insert, mozc::commands::KeyEvent::INSERT},
  {IBUS_Caps_Lock, mozc::commands::KeyEvent::CAPS_LOCK},

  // Shift+TAB.
  {IBUS_ISO_Left_Tab, mozc::commands::KeyEvent::TAB},

  // TODO(mazda): Handle following keys?
  //   - IBUS_Kana_Lock? IBUS_KEY_Kana_Shift?
};

const struct ModifierKeyMapData {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_key_map_data[] = {
  {IBUS_Shift_L, mozc::commands::KeyEvent::SHIFT},
  {IBUS_Shift_R, mozc::commands::KeyEvent::SHIFT},
  {IBUS_Control_L, mozc::commands::KeyEvent::CTRL},
  {IBUS_Control_R, mozc::commands::KeyEvent::CTRL},
  {IBUS_Alt_L, mozc::commands::KeyEvent::ALT},
  {IBUS_Alt_R, mozc::commands::KeyEvent::ALT},
  {IBUS_LOCK_MASK, mozc::commands::KeyEvent::CAPS},
};

const struct ModifierMaskMapData {
  guint from;
  mozc::commands::KeyEvent::ModifierKey to;
} modifier_mask_map_data[] = {
  {IBUS_SHIFT_MASK, mozc::commands::KeyEvent::SHIFT},
  {IBUS_CONTROL_MASK, mozc::commands::KeyEvent::CTRL},
  {IBUS_MOD1_MASK, mozc::commands::KeyEvent::ALT},
};

// TODO(team): Add kana_map_dv to support Dvoraklayout.
const struct KanaMap {
  guint code;
  const char *no_shift;
  const char *shift;
} kana_map_jp[] = {
  { '1' , "ぬ", "ぬ" },
  { '!' , "ぬ", "ぬ" },
  { '2' , "ふ", "ふ" },
  { '\"', "ふ", "ふ" },
  { '3' , "あ", "ぁ" },
  { '#' , "あ", "ぁ" },
  { '4' , "う", "ぅ" },
  { '$' , "う", "ぅ" },
  { '5' , "え", "ぇ" },
  { '%' , "え", "ぇ" },
  { '6' , "お", "ぉ" },
  { '&' , "お", "ぉ" },
  { '7' , "や", "ゃ" },
  { '\'', "や", "ゃ" },
  { '8' , "ゆ", "ゅ" },
  { '(' , "ゆ", "ゅ" },
  { '9' , "よ", "ょ" },
  { ')' , "よ", "ょ" },
  { '0' , "わ", "を" },
  { '-' , "ほ", "ほ" },
  { '=' , "ほ", "ほ" },
  { '^' , "へ", "を" },
  { '~' , "へ", "を" },
  { '|' , "ー", "ー" },
  { 'q' , "た", "た" },
  { 'Q' , "た", "た" },
  { 'w' , "て", "て" },
  { 'W' , "て", "て" },
  { 'e' , "い", "ぃ" },
  { 'E' , "い", "ぃ" },
  { 'r' , "す", "す" },
  { 'R' , "す", "す" },
  { 't' , "か", "か" },
  { 'T' , "か", "か" },
  { 'y' , "ん", "ん" },
  { 'Y' , "ん", "ん" },
  { 'u' , "な", "な" },
  { 'U' , "な", "な" },
  { 'i' , "に", "に" },
  { 'I' , "に", "に" },
  { 'o' , "ら", "ら" },
  { 'O' , "ら", "ら" },
  { 'p' , "せ", "せ" },
  { 'P' , "せ", "せ" },
  { '@' , "゛", "゛" },
  { '`' , "゛", "゛" },
  { '[' , "゜", "「" },
  { '{' , "゜", "「" },
  { 'a' , "ち", "ち" },
  { 'A' , "ち", "ち" },
  { 's' , "と", "と" },
  { 'S' , "と", "と" },
  { 'd' , "し", "し" },
  { 'D' , "し", "し" },
  { 'f' , "は", "は" },
  { 'F' , "は", "は" },
  { 'g' , "き", "き" },
  { 'G' , "き", "き" },
  { 'h' , "く", "く" },
  { 'H' , "く", "く" },
  { 'j' , "ま", "ま" },
  { 'J' , "ま", "ま" },
  { 'k' , "の", "の" },
  { 'K' , "の", "の" },
  { 'l' , "り", "り" },
  { 'L' , "り", "り" },
  { ';' , "れ", "れ" },
  { '+' , "れ", "れ" },
  { ':' , "け", "け" },
  { '*' , "け", "け" },
  { ']' , "む", "」" },
  { '}' , "む", "」" },
  { 'z' , "つ", "っ" },
  { 'Z' , "つ", "っ" },
  { 'x' , "さ", "さ" },
  { 'X' , "さ", "さ" },
  { 'c' , "そ", "そ" },
  { 'C' , "そ", "そ" },
  { 'v' , "ひ", "ひ" },
  { 'V' , "ひ", "ひ" },
  { 'b' , "こ", "こ" },
  { 'B' , "こ", "こ" },
  { 'n' , "み", "み" },
  { 'N' , "み", "み" },
  { 'm' , "も", "も" },
  { 'M' , "も", "も" },
  { ',' , "ね", "、" },
  { '<' , "ね", "、" },
  { '.' , "る", "。" },
  { '>' , "る", "。" },
  { '/' , "め", "・" },
  { '?' , "め", "・" },
  { '_' , "ろ", "ろ" },
  // A backslash is handled in a special way because it is input by
  // two different keys (the one next to Backslash and the one next
  // to Right Shift).
  { '\\', "", "" },
}, kana_map_us[] = {
  { '`' , "ろ", "ろ" },
  { '~' , "ろ", "ろ" },
  { '1' , "ぬ", "ぬ" },
  { '!' , "ぬ", "ぬ" },
  { '2' , "ふ", "ふ" },
  { '@' , "ふ", "ふ" },
  { '3' , "あ", "ぁ" },
  { '#' , "あ", "ぁ" },
  { '4' , "う", "ぅ" },
  { '$' , "う", "ぅ" },
  { '5' , "え", "ぇ" },
  { '%' , "え", "ぇ" },
  { '6' , "お", "ぉ" },
  { '^' , "お", "ぉ" },
  { '7' , "や", "ゃ" },
  { '&' , "や", "ゃ" },
  { '8' , "ゆ", "ゅ" },
  { '*' , "ゆ", "ゅ" },
  { '9' , "よ", "ょ" },
  { '(' , "よ", "ょ" },
  { '0' , "わ", "を" },
  { ')' , "わ", "を" },
  { '-' , "ほ", "ー" },
  { '_' , "ほ", "ー" },
  { '=' , "へ", "へ" },
  { '+' , "へ", "へ" },
  { 'q' , "た", "た" },
  { 'Q' , "た", "た" },
  { 'w' , "て", "て" },
  { 'W' , "て", "て" },
  { 'e' , "い", "ぃ" },
  { 'E' , "い", "ぃ" },
  { 'r' , "す", "す" },
  { 'R' , "す", "す" },
  { 't' , "か", "か" },
  { 'T' , "か", "か" },
  { 'y' , "ん", "ん" },
  { 'Y' , "ん", "ん" },
  { 'u' , "な", "な" },
  { 'U' , "な", "な" },
  { 'i' , "に", "に" },
  { 'I' , "に", "に" },
  { 'o' , "ら", "ら" },
  { 'O' , "ら", "ら" },
  { 'p' , "せ", "せ" },
  { 'P' , "せ", "せ" },
  { '[' , "゛", "゛" },
  { '{' , "゛", "゛" },
  { ']' , "゜", "「" },
  { '}' , "゜", "「" },
  { '\\', "む", "」" },
  { '|' , "む", "」" },
  { 'a' , "ち", "ち" },
  { 'A' , "ち", "ち" },
  { 's' , "と", "と" },
  { 'S' , "と", "と" },
  { 'd' , "し", "し" },
  { 'D' , "し", "し" },
  { 'f' , "は", "は" },
  { 'F' , "は", "は" },
  { 'g' , "き", "き" },
  { 'G' , "き", "き" },
  { 'h' , "く", "く" },
  { 'H' , "く", "く" },
  { 'j' , "ま", "ま" },
  { 'J' , "ま", "ま" },
  { 'k' , "の", "の" },
  { 'K' , "の", "の" },
  { 'l' , "り", "り" },
  { 'L' , "り", "り" },
  { ';' , "れ", "れ" },
  { ':' , "れ", "れ" },
  { '\'', "け", "け" },
  { '\"', "け", "け" },
  { 'z' , "つ", "っ" },
  { 'Z' , "つ", "っ" },
  { 'x' , "さ", "さ" },
  { 'X' , "さ", "さ" },
  { 'c' , "そ", "そ" },
  { 'C' , "そ", "そ" },
  { 'v' , "ひ", "ひ" },
  { 'V' , "ひ", "ひ" },
  { 'b' , "こ", "こ" },
  { 'B' , "こ", "こ" },
  { 'n' , "み", "み" },
  { 'N' , "み", "み" },
  { 'm' , "も", "も" },
  { 'M' , "も", "も" },
  { ',' , "ね", "、" },
  { '<' , "ね", "、" },
  { '.' , "る", "。" },
  { '>' , "る", "。" },
  { '/' , "め", "・" },
  { '?' , "め", "・" },
}, tsuki_map_jp[] = {
  { '1' , "\xef\xbc\x91", "\xef\xbc\x91" },  // "１", "１"
  { '!' , "\xef\xbc\x81", "\xef\xbc\x81" },  // "！", "！"
  { '2' , "\xef\xbc\x92", "\xef\xbc\x92" },  // "２", "２"
  { '"' , "\xef\xbc\x82", "\xef\xbc\x82" },  // "＂", "＂"
  { '3' , "\xef\xbc\x93", "\xef\xbc\x93" },  // "３", "３"
  { '#' , "\xef\xbc\x83", "\xef\xbc\x83" },  // "＃", "＃"
  { '4' , "\xef\xbc\x94", "\xef\xbc\x94" },  // "４", "４"
  { '$' , "\xef\xbc\x84", "\xef\xbc\x84" },  // "＄", "＄"
  { '5' , "\xef\xbc\x95", "\xef\xbc\x95" },  // "５", "５"
  { '%' , "\xef\xbc\x85", "\xef\xbc\x85" },  // "％", "％"
  { '6' , "\xef\xbc\x96", "\xef\xbc\x96" },  // "６", "６"
  { '&' , "\xef\xbc\x86", "\xef\xbc\x86" },  // "＆", "＆"
  { '7' , "\xef\xbc\x97", "\xef\xbc\x97" },  // "７", "７"
  { '\'', "\xef\xbc\x87", "\xef\xbc\x87" },  // "＇", "＇"
  { '8' , "\xef\xbc\x98", "\xef\xbc\x98" },  // "８", "８"
  { '(' , "\xef\xbc\x88", "\xef\xbc\x88" },  // "（", "（"
  { '9' , "\xef\xbc\x99", "\xef\xbc\x99" },  // "９", "９"
  { ')' , "\xef\xbc\x89", "\xef\xbc\x89" },  // "）", "）"
  { '0' , "\xef\xbc\x90", "\xef\xbc\x90" },  // "０", "０"
  { '-' , "\xef\xbc\x8d", "\xef\xbc\x8d" },  // "－", "－"
  { '=' , "\xef\xbc\x9d", "\xef\xbc\x9d" },  // "＝", "＝"
  { '^' , "\xef\xbc\xbe", "\xef\xbc\xbe" },  // "＾", "＾"
  { '~' , "\xef\xbd\x9e", "\xef\xbd\x9e" },  // "～", "～"
  { '|' , "\xef\xbd\x9c", "\xef\xbd\x9c" },  // "｜", "｜"
  { 'q' , "\xe3\x81\x9d", "\xef\xbd\x91" },  // "そ", "ｑ"
  { 'Q' , "\xe3\x81\x9d", "\xef\xbc\xb1" },  // "そ", "Ｑ"
  { 'w' , "\xe3\x81\x93", "\xef\xbd\x97" },  // "こ", "ｗ"
  { 'W' , "\xe3\x81\x93", "\xef\xbc\xb7" },  // "こ", "Ｗ"
  { 'e' , "\xe3\x81\x97", "\xef\xbd\x85" },  // "し", "ｅ"
  { 'E' , "\xe3\x81\x97", "\xef\xbc\xa5" },  // "し", "Ｅ"
  { 'r' , "\xe3\x81\xa6", "\xef\xbd\x92" },  // "て", "ｒ"
  { 'R' , "\xe3\x81\xa6", "\xef\xbc\xb2" },  // "て", "Ｒ"
  { 't' , "\xe3\x82\x87", "\xef\xbd\x94" },  // "ょ", "ｔ"
  { 'T' , "\xe3\x82\x87", "\xef\xbc\xb4" },  // "ょ", "Ｔ"
  { 'y' , "\xe3\x81\xa4", "\xef\xbd\x99" },  // "つ", "ｙ"
  { 'Y' , "\xe3\x81\xa4", "\xef\xbc\xb9" },  // "つ", "Ｙ"
  { 'u' , "\xe3\x82\x93", "\xef\xbd\x95" },  // "ん", "ｕ"
  { 'U' , "\xe3\x82\x93", "\xef\xbc\xb5" },  // "ん", "Ｕ"
  { 'i' , "\xe3\x81\x84", "\xef\xbd\x89" },  // "い", "ｉ"
  { 'I' , "\xe3\x81\x84", "\xef\xbc\xa9" },  // "い", "Ｉ"
  { 'o' , "\xe3\x81\xae", "\xef\xbd\x8f" },  // "の", "ｏ"
  { 'O' , "\xe3\x81\xae", "\xef\xbc\xaf" },  // "の", "Ｏ"
  { 'p' , "\xe3\x82\x8a", "\xef\xbd\x90" },  // "り", "ｐ"
  { 'P' , "\xe3\x82\x8a", "\xef\xbc\xb0" },  // "り", "Ｐ"
  { '@' , "\xe3\x81\xa1", "\xef\xbc\xa0" },  // "ち", "＠"
  { '`' , "\xe3\x81\xa1", "\xef\xbd\x80" },  // "ち", "｀"
  { '[' , "\xef\xbc\xbb", "\xef\xbc\xbb" },  // "［", "［"
  { '{' , "\xef\xbd\x9b", "\xef\xbd\x9b" },  // "｛", "｛"
  { 'a' , "\xe3\x81\xaf", "\xef\xbd\x81" },  // "は", "ａ"
  { 'A' , "\xe3\x81\xaf", "\xef\xbc\xa1" },  // "は", "Ａ"
  { 's' , "\xe3\x81\x8b", "\xef\xbd\x93" },  // "か", "ｓ"
  { 'S' , "\xe3\x81\x8b", "\xef\xbc\xb3" },  // "か", "Ｓ"
  { 'd' , "\xe3\x82\x97", "\xef\xbd\x84" },  // "゗", "ｄ"
  { 'D' , "\xe3\x82\x97", "\xef\xbc\xa4" },  // "゗", "Ｄ"
  { 'f' , "\xe3\x81\xa8", "\xef\xbd\x86" },  // "と", "ｆ"
  { 'F' , "\xe3\x81\xa8", "\xef\xbc\xa6" },  // "と", "Ｆ"
  { 'g' , "\xe3\x81\x9f", "\xef\xbd\x87" },  // "た", "ｇ"
  { 'G' , "\xe3\x81\x9f", "\xef\xbc\xa7" },  // "た", "Ｇ"
  { 'h' , "\xe3\x81\x8f", "\xef\xbd\x88" },  // "く", "ｈ"
  { 'H' , "\xe3\x81\x8f", "\xef\xbc\xa8" },  // "く", "Ｈ"
  { 'j' , "\xe3\x81\x86", "\xef\xbd\x8a" },  // "う", "ｊ"
  { 'J' , "\xe3\x81\x86", "\xef\xbc\xaa" },  // "う", "Ｊ"
  { 'k' , "\xe3\x82\x98", "\xef\xbd\x8b" },  // "゘", "ｋ"
  { 'K' , "\xe3\x82\x98", "\xef\xbc\xab" },  // "゘", "Ｋ"
  { 'l' , "\xe3\x82\x9b", "\xef\xbd\x8c" },  // "゛", "ｌ"
  { 'L' , "\xe3\x82\x9b", "\xef\xbc\xac" },  // "゛", "Ｌ"
  { ';' , "\xe3\x81\x8d", "\xef\xbc\x9b" },  // "き", "；"
  { '+' , "\xe3\x81\x8d", "\xef\xbc\x8b" },  // "き", "＋"
  { ':' , "\xe3\x82\x8c", "\xef\xbc\x9a" },  // "れ", "："
  { '*' , "\xe3\x82\x8c", "\xef\xbc\x8a" },  // "れ", "＊"
  { ']' , "\xef\xbc\xbd", "\xef\xbc\xbd" },  // "］", "］"
  { '}' , "\xef\xbd\x9d", "\xef\xbd\x9d" },  // "｝", "｝"
  { 'z' , "\xe3\x81\x99", "\xef\xbd\x9a" },  // "す", "ｚ"
  { 'Z' , "\xe3\x81\x99", "\xef\xbc\xba" },  // "す", "Ｚ"
  { 'x' , "\xe3\x81\x91", "\xef\xbd\x98" },  // "け", "ｘ"
  { 'X' , "\xe3\x81\x91", "\xef\xbc\xb8" },  // "け", "Ｘ"
  { 'c' , "\xe3\x81\xab", "\xef\xbd\x83" },  // "に", "ｃ"
  { 'C' , "\xe3\x81\xab", "\xef\xbc\xa3" },  // "に", "Ｃ"
  { 'v' , "\xe3\x81\xaa", "\xef\xbd\x96" },  // "な", "ｖ"
  { 'V' , "\xe3\x81\xaa", "\xef\xbc\xb6" },  // "な", "Ｖ"
  { 'b' , "\xe3\x81\x95", "\xef\xbd\x82" },  // "さ", "ｂ"
  { 'B' , "\xe3\x81\x95", "\xef\xbc\xa2" },  // "さ", "Ｂ"
  { 'n' , "\xe3\x81\xa3", "\xef\xbd\x8e" },  // "っ", "ｎ"
  { 'N' , "\xe3\x81\xa3", "\xef\xbc\xae" },  // "っ", "Ｎ"
  { 'm' , "\xe3\x82\x8b", "\xef\xbd\x8d" },  // "る", "ｍ"
  { 'M' , "\xe3\x82\x8b", "\xef\xbc\xad" },  // "る", "Ｍ"
  { ',' , "\xe3\x80\x81", "\xef\xbc\x8c" },  // "、", "，"
  { '<' , "\xe3\x80\x81", "\xef\xbc\x9c" },  // "、", "＜"
  { '.' , "\xe3\x80\x82", "\xef\xbc\x8e" },  // "。", "．"
  { '>' , "\xe3\x80\x82", "\xef\xbc\x9e" },  // "。", "＞"
  { '/' , "\xe3\x82\x9c", "\xef\xbc\x8f" },  // "゜", "／"
  { '?' , "\xe3\x82\x9c", "\xef\xbc\x9f" },  // "゜", "？"
  { '_' , "\xe3\x83\xbb", "\xef\xbc\xbf" },  // "・", "＿"
  // A backslash is handled in a special way because it is input by
  // two different keys (the one next to Backslash and the one next
  // to Right Shift).
  { '\\', "", "" },
}, tsuki_map_us[] = {
  { '`' , "\xef\xbd\x80", "\xef\xbd\x80" },  // "｀", "｀"
  { '~' , "\xef\xbd\x9e", "\xef\xbd\x9e" },  // "～", "～"
  { '1' , "\xef\xbc\x91", "\xef\xbc\x91" },  // "１", "１"
  { '!' , "\xef\xbc\x81", "\xef\xbc\x81" },  // "！", "！"
  { '2' , "\xef\xbc\x92", "\xef\xbc\x92" },  // "２", "２"
  { '@' , "\xef\xbc\xa0", "\xef\xbc\xa0" },  // "＠", "＠"
  { '3' , "\xef\xbc\x93", "\xef\xbc\x93" },  // "３", "３"
  { '#' , "\xef\xbc\x83", "\xef\xbc\x83" },  // "＃", "＃"
  { '4' , "\xef\xbc\x94", "\xef\xbc\x94" },  // "４", "４"
  { '$' , "\xef\xbc\x84", "\xef\xbc\x84" },  // "＄", "＄"
  { '5' , "\xef\xbc\x95", "\xef\xbc\x95" },  // "５", "５"
  { '%' , "\xef\xbc\x85", "\xef\xbc\x85" },  // "％", "％"
  { '6' , "\xef\xbc\x96", "\xef\xbc\x96" },  // "６", "６"
  { '^' , "\xef\xbc\xbe", "\xef\xbc\xbe" },  // "＾", "＾"
  { '7' , "\xef\xbc\x97", "\xef\xbc\x97" },  // "７", "７"
  { '&' , "\xef\xbc\x86", "\xef\xbc\x86" },  // "＆", "＆"
  { '8' , "\xef\xbc\x98", "\xef\xbc\x98" },  // "８", "８"
  { '*' , "\xef\xbc\x8a", "\xef\xbc\x8a" },  // "＊", "＊"
  { '9' , "\xef\xbc\x99", "\xef\xbc\x99" },  // "９", "９"
  { '(' , "\xef\xbc\x88", "\xef\xbc\x88" },  // "（", "（"
  { '0' , "\xef\xbc\x90", "\xef\xbc\x90" },  // "０", "０"
  { ')' , "\xef\xbc\x89", "\xef\xbc\x89" },  // "）", "）"
  { '-' , "\xef\xbc\x8d", "\xef\xbc\x8d" },  // "－", "－"
  { '_' , "\xef\xbc\xbf", "\xef\xbc\xbf" },  // "＿", "＿"
  { '=' , "\xef\xbc\x9d", "\xef\xbc\x9d" },  // "＝", "＝"
  { '+' , "\xef\xbc\x8b", "\xef\xbc\x8b" },  // "＋", "＋"
  { 'q' , "\xe3\x81\x9d", "\xef\xbd\x91" },  // "そ", "ｑ"
  { 'Q' , "\xe3\x81\x9d", "\xef\xbc\xb1" },  // "そ", "Ｑ"
  { 'w' , "\xe3\x81\x93", "\xef\xbd\x97" },  // "こ", "ｗ"
  { 'W' , "\xe3\x81\x93", "\xef\xbc\xb7" },  // "こ", "Ｗ"
  { 'e' , "\xe3\x81\x97", "\xef\xbd\x85" },  // "し", "ｅ"
  { 'E' , "\xe3\x81\x97", "\xef\xbc\xa5" },  // "し", "Ｅ"
  { 'r' , "\xe3\x81\xa6", "\xef\xbd\x92" },  // "て", "ｒ"
  { 'R' , "\xe3\x81\xa6", "\xef\xbc\xb2" },  // "て", "Ｒ"
  { 't' , "\xe3\x82\x87", "\xef\xbd\x94" },  // "ょ", "ｔ"
  { 'T' , "\xe3\x82\x87", "\xef\xbc\xb4" },  // "ょ", "Ｔ"
  { 'y' , "\xe3\x81\xa4", "\xef\xbd\x99" },  // "つ", "ｙ"
  { 'Y' , "\xe3\x81\xa4", "\xef\xbc\xb9" },  // "つ", "Ｙ"
  { 'u' , "\xe3\x82\x93", "\xef\xbd\x95" },  // "ん", "ｕ"
  { 'U' , "\xe3\x82\x93", "\xef\xbc\xb5" },  // "ん", "Ｕ"
  { 'i' , "\xe3\x81\x84", "\xef\xbd\x89" },  // "い", "ｉ"
  { 'I' , "\xe3\x81\x84", "\xef\xbc\xa9" },  // "い", "Ｉ"
  { 'o' , "\xe3\x81\xae", "\xef\xbd\x8f" },  // "の", "ｏ"
  { 'O' , "\xe3\x81\xae", "\xef\xbc\xaf" },  // "の", "Ｏ"
  { 'p' , "\xe3\x82\x8a", "\xef\xbd\x90" },  // "り", "ｐ"
  { 'P' , "\xe3\x82\x8a", "\xef\xbc\xb0" },  // "り", "Ｐ"
  { '[' , "\xe3\x81\xa1", "\xef\xbc\xbb" },  // "ち", "［"
  { '{' , "\xe3\x81\xa1", "\xef\xbd\x9b" },  // "ち", "｛"
  { ']' , "\xe3\x83\xbb", "\xef\xbc\xbd" },  // "・", "］"
  { '}' , "\xe3\x83\xbb", "\xef\xbd\x9d" },  // "・", "｝"
  { '\\', "\xef\xbc\xbc", "\xef\xbc\xbc" },  // "＼", "＼"
  { '|' , "\xef\xbd\x9c", "\xef\xbd\x9c" },  // "｜", "｜"
  { 'a' , "\xe3\x81\xaf", "\xef\xbd\x81" },  // "は", "ａ"
  { 'A' , "\xe3\x81\xaf", "\xef\xbc\xa1" },  // "は", "Ａ"
  { 's' , "\xe3\x81\x8b", "\xef\xbd\x93" },  // "か", "ｓ"
  { 'S' , "\xe3\x81\x8b", "\xef\xbc\xb3" },  // "か", "Ｓ"
  { 'd' , "\xe3\x82\x97", "\xef\xbd\x84" },  // "゗", "ｄ"
  { 'D' , "\xe3\x82\x97", "\xef\xbc\xa4" },  // "゗", "Ｄ"
  { 'f' , "\xe3\x81\xa8", "\xef\xbd\x86" },  // "と", "ｆ"
  { 'F' , "\xe3\x81\xa8", "\xef\xbc\xa6" },  // "と", "Ｆ"
  { 'g' , "\xe3\x81\x9f", "\xef\xbd\x87" },  // "た", "ｇ"
  { 'G' , "\xe3\x81\x9f", "\xef\xbc\xa7" },  // "た", "Ｇ"
  { 'h' , "\xe3\x81\x8f", "\xef\xbd\x88" },  // "く", "ｈ"
  { 'H' , "\xe3\x81\x8f", "\xef\xbc\xa8" },  // "く", "Ｈ"
  { 'j' , "\xe3\x81\x86", "\xef\xbd\x8a" },  // "う", "ｊ"
  { 'J' , "\xe3\x81\x86", "\xef\xbc\xaa" },  // "う", "Ｊ"
  { 'k' , "\xe3\x82\x98", "\xef\xbd\x8b" },  // "゘", "ｋ"
  { 'K' , "\xe3\x82\x98", "\xef\xbc\xab" },  // "゘", "Ｋ"
  { 'l' , "\xe3\x82\x9b", "\xef\xbd\x8c" },  // "゛", "ｌ"
  { 'L' , "\xe3\x82\x9b", "\xef\xbc\xac" },  // "゛", "Ｌ"
  { ';' , "\xe3\x81\x8d", "\xef\xbc\x9b" },  // "き", "；"
  { ':' , "\xe3\x81\x8d", "\xef\xbc\x9a" },  // "き", "："
  { '\'', "\xe3\x82\x8c", "\xe2\x80\x99" },  // "れ", "’"
  { '"' , "\xe3\x82\x8c", "\xef\xbc\x82" },  // "れ", "＂"
  { 'z' , "\xe3\x81\x99", "\xef\xbd\x9a" },  // "す", "ｚ"
  { 'Z' , "\xe3\x81\x99", "\xef\xbc\xba" },  // "す", "Ｚ"
  { 'x' , "\xe3\x81\x91", "\xef\xbd\x98" },  // "け", "ｘ"
  { 'X' , "\xe3\x81\x91", "\xef\xbc\xb8" },  // "け", "Ｘ"
  { 'c' , "\xe3\x81\xab", "\xef\xbd\x83" },  // "に", "ｃ"
  { 'C' , "\xe3\x81\xab", "\xef\xbc\xa3" },  // "に", "Ｃ"
  { 'v' , "\xe3\x81\xaa", "\xef\xbd\x96" },  // "な", "ｖ"
  { 'V' , "\xe3\x81\xaa", "\xef\xbc\xb6" },  // "な", "Ｖ"
  { 'b' , "\xe3\x81\x95", "\xef\xbd\x82" },  // "さ", "ｂ"
  { 'B' , "\xe3\x81\x95", "\xef\xbc\xa2" },  // "さ", "Ｂ"
  { 'n' , "\xe3\x81\xa3", "\xef\xbd\x8e" },  // "っ", "ｎ"
  { 'N' , "\xe3\x81\xa3", "\xef\xbc\xae" },  // "っ", "Ｎ"
  { 'm' , "\xe3\x82\x8b", "\xef\xbd\x8d" },  // "る", "ｍ"
  { 'M' , "\xe3\x82\x8b", "\xef\xbc\xad" },  // "る", "Ｍ"
  { ',' , "\xe3\x80\x81", "\xef\xbc\x8c" },  // "、", "，"
  { '<' , "\xe3\x80\x81", "\xef\xbc\x9c" },  // "、", "＜"
  { '.' , "\xe3\x80\x82", "\xef\xbc\x8e" },  // "。", "．"
  { '>' , "\xe3\x80\x82", "\xef\xbc\x9e" },  // "。", "＞"
  { '/' , "\xe3\x82\x9c", "\xef\xbc\x8f" },  // "゜", "／"
  { '?' , "\xe3\x82\x9c", "\xef\xbc\x9f" },  // "゜", "？"
};

}  // namespace

namespace mozc {
namespace ibus {

KeyTranslator::KeyTranslator() {
  Init();
}

KeyTranslator::~KeyTranslator() {
}

// TODO(nona): Fix 'Shift-0' behavior b/4338394
bool KeyTranslator::Translate(guint keyval,
                              guint keycode,
                              guint modifiers,
                              config::Config::PreeditMethod method,
                              bool layout_is_jp,
                              commands::KeyEvent *out_event) const {
  DCHECK(out_event) << "out_event is NULL";
  out_event->Clear();

  // Due to historical reasons, many linux ditributions set Hiragana_Katakana
  // key as Hiragana key (which is Katkana key with shift modifier). So, we
  // translate Hiragana_Katanaka key as Hiragana key by mapping table, and
  // Shift + Hiragana_Katakana key as Katakana key by functionally.
  // TODO(nona): Fix process modifier to handle right shift
  if (IsHiraganaKatakanaKeyWithShift(keyval, keycode, modifiers)) {
    modifiers &= ~IBUS_SHIFT_MASK;
    keyval = IBUS_Katakana;
  }
  string kana_key_string;
  if ((method == config::Config::KANA) && IsKanaAvailable(
          keyval, keycode, modifiers, layout_is_jp, &kana_key_string)) {
    out_event->set_key_code(keyval);
    out_event->set_key_string(kana_key_string);
  } else if ((method == config::Config::TSUKI) && IsTsukiAvailable(
          keyval, keycode, modifiers, layout_is_jp, &kana_key_string)) {
    out_event->set_key_code(keyval);
    out_event->set_key_string(kana_key_string);
  } else if (IsAscii(keyval, keycode, modifiers)) {
    if (IBUS_LOCK_MASK & modifiers) {
      out_event->add_modifier_keys(commands::KeyEvent::CAPS);
    }
    out_event->set_key_code(keyval);
  } else if (IsModifierKey(keyval, keycode, modifiers)) {
    ModifierKeyMap::const_iterator i = modifier_key_map_.find(keyval);
    DCHECK(i != modifier_key_map_.end());
    out_event->add_modifier_keys(i->second);
  } else if (IsSpecialKey(keyval, keycode, modifiers)) {
    SpecialKeyMap::const_iterator i = special_key_map_.find(keyval);
    DCHECK(i != special_key_map_.end());
    out_event->set_special_key(i->second);
  } else {
    VLOG(1) << "Unknown keyval: " << keyval;
    return false;
  }

  for (ModifierKeyMap::const_iterator i = modifier_mask_map_.begin();
       i != modifier_mask_map_.end(); ++i) {
    // Do not set a SHIFT modifier when |keyval| is a printable key by following
    // the Mozc's rule.
    if ((i->second == commands::KeyEvent::SHIFT) &&
        IsPrintable(keyval, keycode, modifiers)) {
      continue;
    }

    if (i->first & modifiers) {
      // Add a modifier key if doesn't exist.
      commands::KeyEvent::ModifierKey modifier = i->second;
      bool found = false;
      for (int i = 0; i < out_event->modifier_keys_size(); ++i) {
        if (modifier == out_event->modifier_keys(i)) {
          found = true;
          break;
        }
      }
      if (!found) {
        out_event->add_modifier_keys(modifier);
      }
    }
  }

  return true;
}

void KeyTranslator::Init() {
  for (int i = 0; i < arraysize(special_key_map); ++i) {
    CHECK(special_key_map_.insert(
        std::make_pair(special_key_map[i].from,
                       special_key_map[i].to)).second);
  }
  for (int i = 0; i < arraysize(modifier_key_map_data); ++i) {
    CHECK(modifier_key_map_.insert(
        std::make_pair(modifier_key_map_data[i].from,
                       modifier_key_map_data[i].to)).second);
  }
  for (int i = 0; i < arraysize(modifier_mask_map_data); ++i) {
    CHECK(modifier_mask_map_.insert(
        std::make_pair(modifier_mask_map_data[i].from,
                       modifier_mask_map_data[i].to)).second);
  }
  for (int i = 0; i < arraysize(kana_map_jp); ++i) {
    CHECK(kana_map_jp_.insert(
        std::make_pair(kana_map_jp[i].code,
                       std::make_pair(kana_map_jp[i].no_shift,
                                      kana_map_jp[i].shift))).second);
  }
  for (int i = 0; i < arraysize(kana_map_us); ++i) {
    CHECK(kana_map_us_.insert(
        std::make_pair(kana_map_us[i].code,
                       std::make_pair(kana_map_us[i].no_shift,
                                      kana_map_us[i].shift))).second);
  }
  for (int i = 0; i < arraysize(tsuki_map_jp); ++i) {
    CHECK(tsuki_map_jp_.insert(
	std::make_pair(tsuki_map_jp[i].code,
		       std::make_pair(tsuki_map_jp[i].no_shift,
				      tsuki_map_jp[i].shift))).second);
  }
  for (int i = 0; i < arraysize(tsuki_map_us); ++i) {
    CHECK(tsuki_map_us_.insert(
	std::make_pair(tsuki_map_us[i].code,
		       std::make_pair(tsuki_map_us[i].no_shift,
				      tsuki_map_us[i].shift))).second);
  }
}

bool KeyTranslator::IsModifierKey(guint keyval,
                                  guint keycode,
                                  guint modifiers) const {
  return modifier_key_map_.find(keyval) != modifier_key_map_.end();
}

bool KeyTranslator::IsSpecialKey(guint keyval,
                                 guint keycode,
                                 guint modifiers) const {
  return special_key_map_.find(keyval) != special_key_map_.end();
}

bool KeyTranslator::IsHiraganaKatakanaKeyWithShift(guint keyval,
                                                   guint keycode,
                                                   guint modifiers) {
  return ((modifiers & IBUS_SHIFT_MASK) && (keyval == IBUS_Hiragana_Katakana));
}

bool KeyTranslator::IsKanaAvailable(guint keyval,
                                    guint keycode,
                                    guint modifiers,
                                    bool layout_is_jp,
                                    string *out) const {
  if ((modifiers & IBUS_CONTROL_MASK) || (modifiers & IBUS_MOD1_MASK)) {
    return false;
  }
  const KanaMap &kana_map = layout_is_jp ? kana_map_jp_ : kana_map_us_;
  KanaMap::const_iterator iter = kana_map.find(keyval);
  if (iter == kana_map.end()) {
    return false;
  }

  if (out) {
    // When a Japanese keyboard is in use, the yen-sign key and the backslash
    // key generate the same |keyval|. In this case, we have to check |keycode|
    // to return an appropriate string. See the following IBus issue for
    // details: https://github.com/ibus/ibus/issues/73
    if (keyval == '\\' && layout_is_jp) {
      if (keycode == IBUS_bar) {
        *out = "ー";
      } else {
        *out = "ろ";
      }
    } else {
      *out = (modifiers & IBUS_SHIFT_MASK) ?
          iter->second.second : iter->second.first;
    }
  }
  return true;
}

bool KeyTranslator::IsTsukiAvailable(guint keyval,
                                    guint keycode,
                                    guint modifiers,
                                    bool layout_is_jp,
                                    string *out) const {
  if ((modifiers & IBUS_CONTROL_MASK) || (modifiers & IBUS_MOD1_MASK)) {
    return false;
  }
  const KanaMap &kana_map = layout_is_jp ? tsuki_map_jp_ : tsuki_map_us_;
  KanaMap::const_iterator iter = kana_map.find(keyval);
  if (iter == kana_map.end()) {
    return false;
  }

  if (out) {
    // When a Japanese keyboard is in use, the yen-sign key and the backslash
    // key generate the same |keyval|. In this case, we have to check |keycode|
    // to return an appropriate string. See the following IBus issue for
    // details: http://code.google.com/p/ibus/issues/detail?id=52
    if (keyval == '\\' && layout_is_jp) {
      if (keycode == IBUS_bar) {
        *out = "\xef\xbf\xa5";  // "￥"
      } else {
        *out = "\xe3\x83\xbb";  // "・"
      }
    } else {
      *out = (modifiers & IBUS_SHIFT_MASK) ?
          iter->second.second : iter->second.first;
    }
  }
  return true;
}

// TODO(nona): resolve S-'0' problem (b/4338394).
// TODO(nona): Current printable detection is weak. To enhance accuracy, use xkb
// key map
bool KeyTranslator::IsPrintable(guint keyval, guint keycode, guint modifiers) {
  if ((modifiers & IBUS_CONTROL_MASK) || (modifiers & IBUS_MOD1_MASK)) {
    return false;
  }
  return IsAscii(keyval, keycode, modifiers);
}

bool KeyTranslator::IsAscii(guint keyval, guint keycode, guint modifiers) {
  return (keyval > IBUS_space &&
          // Note: Space key (0x20) is a special key in Mozc.
          keyval <= IBUS_asciitilde);  // 0x7e.
}

}  // namespace ibus
}  // namespace mozc
