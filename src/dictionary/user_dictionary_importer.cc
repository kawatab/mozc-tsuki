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

#include "dictionary/user_dictionary_importer.h"

#ifdef OS_WIN
#include <windows.h>
#endif  // OS_WIN

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/win_util.h"
#include "dictionary/user_dictionary_util.h"

namespace mozc {

using user_dictionary::UserDictionary;
using user_dictionary::UserDictionaryCommandStatus;

namespace {

uint64 EntryFingerprint(const UserDictionary::Entry &entry) {
  DCHECK_LE(0, entry.pos());
MOZC_CLANG_PUSH_WARNING();
#if MOZC_CLANG_HAS_WARNING(tautological-constant-out-of-range-compare)
MOZC_CLANG_DISABLE_WARNING(tautological-constant-out-of-range-compare);
#endif  // MOZC_CLANG_HAS_WARNING(tautological-constant-out-of-range-compare)
  DCHECK_LE(entry.pos(), 255);
MOZC_CLANG_POP_WARNING();
  return Hash::Fingerprint(entry.key() + "\t" +
                           entry.value() + "\t" +
                           static_cast<char>(entry.pos()));
}

void NormalizePOS(const string &input, string *output) {
  string tmp;
  output->clear();
  Util::FullWidthAsciiToHalfWidthAscii(input, &tmp);
  Util::HalfWidthKatakanaToFullWidthKatakana(tmp, output);
}

// A data type to hold conversion rules of POSes. If mozc_pos is set to be an
// empty string (""), it means that words of the POS should be ignored in Mozc.
struct POSMap {
  const char *source_pos;  // POS string of a third party IME.
  UserDictionary::PosType mozc_pos;  // POS of Mozc.
};

// Include actual POS mapping rules defined outside the file.
#include "dictionary/pos_map.inc"

// A functor for searching an array of POSMap for the given POS. The class is
// used with std::lower_bound().
class POSMapCompare {
 public:
  bool operator() (const POSMap &l_pos_map, const POSMap &r_pos_map) const {
    return (strcmp(l_pos_map.source_pos, r_pos_map.source_pos) < 0);
  }
};

// Convert POS of a third party IME to that of Mozc using the given mapping.
bool ConvertEntryInternal(
    const POSMap *pos_map,
    size_t map_size,
    const UserDictionaryImporter::RawEntry &from,
    UserDictionary::Entry *to) {
  if (to == NULL) {
    LOG(ERROR) << "Null pointer is passed.";
    return false;
  }

  to->Clear();

  if (from.pos.empty()) {
    return false;
  }

  // Normalize POS (remove full width ascii and half width katakana)
  string pos;
  NormalizePOS(from.pos, &pos);

  // ATOK's POS has a special marker for distinguishing auto-registered
  // words/manually-registered words. Remove the mark here.
  // TODO(yukawa): Use string::back once C++11 is enabled on Mac.
  if (!pos.empty() && (*pos.rbegin() == '$' || *pos.rbegin() == '*')) {
    // TODO(matsuzakit): Use pop_back instead when C++11 is ready on Android.
    pos.resize(pos.size() - 1);
  }

  POSMap key;
  key.source_pos = pos.c_str();
  key.mozc_pos = static_cast<UserDictionary::PosType>(0);

  // Search for mapping for the given POS.
  const POSMap *found =
      std::lower_bound(pos_map, pos_map + map_size, key, POSMapCompare());
  if (found == pos_map + map_size ||
      strcmp(found->source_pos, key.source_pos) != 0) {
    LOG(WARNING) << "Invalid POS is passed: " << from.pos;
    return false;
  }
  if (!UserDictionary::PosType_IsValid(found->mozc_pos)) {
    to->clear_key();
    to->clear_value();
    to->clear_pos();
    return false;
  }

  to->set_key(from.key);
  to->set_value(from.value);
  to->set_pos(found->mozc_pos);

  // Normalize reading.
  string normalized_key;
  UserDictionaryUtil::NormalizeReading(to->key(), &normalized_key);
  to->set_key(normalized_key);

  // Copy comment.
  if (!from.comment.empty()) {
    to->set_comment(from.comment);
  }

  // Validation.
  if (UserDictionaryUtil::ValidateEntry(*to) !=
      UserDictionaryCommandStatus::USER_DICTIONARY_COMMAND_SUCCESS) {
    return false;
  }

  return true;
}

}  // namespace

UserDictionaryImporter::ErrorType UserDictionaryImporter::ImportFromIterator(
    InputIteratorInterface *iter, UserDictionary *user_dic) {
  if (iter == NULL || user_dic == NULL) {
    LOG(ERROR) << "iter or user_dic is NULL";
    return IMPORT_FATAL;
  }

  const size_t max_size = UserDictionaryUtil::max_entry_size();

  ErrorType ret = IMPORT_NO_ERROR;

  std::set<uint64> existent_entries;
  for (size_t i = 0; i < user_dic->entries_size(); ++i) {
    existent_entries.insert(EntryFingerprint(user_dic->entries(i)));
  }

  UserDictionary::Entry entry;
  RawEntry raw_entry;
  while (iter->Next(&raw_entry)) {
    if (user_dic->entries_size() >= max_size) {
      LOG(WARNING) << "Too many words in one dictionary";
      return IMPORT_TOO_MANY_WORDS;
    }

    if (raw_entry.key.empty() &&
        raw_entry.value.empty() &&
        raw_entry.comment.empty()) {
      // Empty entry is just skipped. It could be annoying if we show a
      // warning dialog when these empty candidates exist.
      continue;
    }

    if (!ConvertEntry(raw_entry, &entry)) {
      LOG(WARNING) << "Entry is not valid";
      ret = IMPORT_INVALID_ENTRIES;
      continue;
    }

    // Don't register words if it is aleady in the current dictionary.
    if (!existent_entries.insert(EntryFingerprint(entry)).second) {
      continue;
    }

    UserDictionary::Entry *new_entry = user_dic->add_entries();
    DCHECK(new_entry);
    new_entry->CopyFrom(entry);
  }

  return ret;
}

UserDictionaryImporter::ErrorType
UserDictionaryImporter::ImportFromTextLineIterator(
    IMEType ime_type,
    TextLineIteratorInterface *iter,
    UserDictionary *user_dic) {
  TextInputIterator text_iter(ime_type, iter);
  if (text_iter.ime_type() == NUM_IMES) {
    return IMPORT_NOT_SUPPORTED;
  }

  return ImportFromIterator(&text_iter, user_dic);
}

UserDictionaryImporter::StringTextLineIterator::StringTextLineIterator(
    StringPiece data) : data_(data), position_(0) {}

UserDictionaryImporter::StringTextLineIterator::~StringTextLineIterator() {}

bool UserDictionaryImporter::StringTextLineIterator::IsAvailable() const {
  return position_ < data_.length();
}

bool UserDictionaryImporter::StringTextLineIterator::Next(string *line) {
  if (!IsAvailable()) {
    return false;
  }

  const StringPiece crlf("\r\n");
  for (size_t i = position_; i < data_.length(); ++i) {
    if (data_[i] == '\n' || data_[i] == '\r') {
      const StringPiece next_line =
          ClippedSubstr(data_, position_, i - position_);
      line->assign(next_line.data(), next_line.size());
      // Handles CR/LF issue.
      const StringPiece possible_crlf = ClippedSubstr(data_, i, 2);
      position_ = possible_crlf.compare(crlf) == 0 ? (i + 2) : (i + 1);
      return true;
    }
  }

  const StringPiece next_line =
      ClippedSubstr(data_, position_, data_.size() - position_);
  line->assign(next_line.data(), next_line.size());
  position_ = data_.length();
  return true;
}

void UserDictionaryImporter::StringTextLineIterator::Reset() {
  position_ = 0;
}

UserDictionaryImporter::TextInputIterator::TextInputIterator(
    IMEType ime_type,
    TextLineIteratorInterface *iter)
    : ime_type_(NUM_IMES), iter_(iter) {
  CHECK(iter_);
  if (!iter_->IsAvailable()) {
    return;
  }

  IMEType guessed_type = NUM_IMES;
  string line;
  if (iter_->Next(&line)) {
    guessed_type = GuessIMEType(line);
    iter_->Reset();
  }

  ime_type_ = DetermineFinalIMEType(ime_type, guessed_type);

  VLOG(1) << "Setting type to: " << static_cast<int>(ime_type_);
}

UserDictionaryImporter::TextInputIterator::~TextInputIterator() {}

bool UserDictionaryImporter::TextInputIterator::IsAvailable() const {
  DCHECK(iter_);
  return (iter_->IsAvailable() &&
          ime_type_ != IME_AUTO_DETECT &&
          ime_type_ != NUM_IMES);
}

bool UserDictionaryImporter::TextInputIterator::Next(RawEntry *entry) {
  DCHECK(iter_);
  if (!IsAvailable()) {
    LOG(ERROR) << "iterator is not available";
    return false;
  }

  if (entry == NULL) {
    LOG(ERROR) << "Entry is NULL";
    return false;
  }

  entry->Clear();

  string line;
  while (iter_->Next(&line)) {
    Util::ChopReturns(&line);
    // Skip empty lines.
    if (line.empty()) {
      continue;
    }
    // Skip comment lines.
    // TODO(yukawa): Use string::front once C++11 is enabled on Mac.
    if (((ime_type_ == MSIME || ime_type_ == ATOK) && line[0] == '!') ||
        (ime_type_ == MOZC && line[0] == '#') ||
        (ime_type_ == KOTOERI && line.find("//") == 0)) {
      continue;
    }

    VLOG(2) << line;

    std::vector<string> values;
    switch (ime_type_) {
      case MSIME:
      case ATOK:
      case MOZC:
        Util::SplitStringAllowEmpty(line, "\t", &values);
        if (values.size() < 3) {
          continue;  // Ignore this line.
        }
        entry->key = values[0];
        entry->value = values[1];
        entry->pos = values[2];
        if (values.size() >= 4) {
          entry->comment = values[3];
        }
        return true;
        break;
      case KOTOERI:
        Util::SplitCSV(line, &values);
        if (values.size() < 3) {
          continue;  // Ignore this line.
        }
        entry->key = values[0];
        entry->value = values[1];
        entry->pos = values[2];
        return true;
        break;
      default:
        LOG(ERROR) << "Unknown format: " << static_cast<int>(ime_type_);
        return false;
    }
  }

  return false;
}

bool UserDictionaryImporter::ConvertEntry(
    const RawEntry &from, UserDictionary::Entry *to) {
  return ConvertEntryInternal(kPOSMap, arraysize(kPOSMap), from, to);
}

UserDictionaryImporter::IMEType
UserDictionaryImporter::GuessIMEType(StringPiece line) {
  if (line.empty()) {
    return NUM_IMES;
  }

  string lower = line.as_string();
  Util::LowerString(&lower);

  if (lower.find("!microsoft ime") == 0) {
    return MSIME;
  }

  // Old ATOK format (!!DICUT10) is not supported for now
  // http://b/2455897
  if (lower.find("!!dicut") == 0 && lower.size() > 7) {
    const string version(lower, 7, lower.size() - 7);
    if (NumberUtil::SimpleAtoi(version) >= 11) {
      return ATOK;
    } else {
      return NUM_IMES;
    }
  }

  if (lower.find("!!atok_tango_text_header") == 0) {
    return ATOK;
  }

  if (*line.begin() == '"' && *line.rbegin() == '"' &&
      line.find("\t") == string::npos) {
    return KOTOERI;
  }

  if (*line.begin() == '#' || line.find("\t") != string::npos) {
    return MOZC;
  }

  return NUM_IMES;
}

UserDictionaryImporter::IMEType UserDictionaryImporter::DetermineFinalIMEType(
    IMEType user_ime_type, IMEType guessed_ime_type) {
  IMEType result_ime_type = NUM_IMES;

  if (user_ime_type == IME_AUTO_DETECT) {
    // Trust guessed type.
    result_ime_type = guessed_ime_type;
  } else if (user_ime_type == MOZC) {
    // MOZC is compatible with MS-IME and ATOK.
    // Even if the auto detection failed, try to use Mozc format.
    if (guessed_ime_type != KOTOERI) {
      result_ime_type = user_ime_type;
    }
  } else {
    // ATOK, MS-IME and Kotoeri can be detected with 100% accuracy.
    if (guessed_ime_type == user_ime_type) {
      result_ime_type = user_ime_type;
    }
  }

  return result_ime_type;
}

UserDictionaryImporter::EncodingType
UserDictionaryImporter::GuessEncodingType(StringPiece str) {
  // Unicode BOM.
  if (str.size() >= 2 &&
      ((static_cast<uint8>(str[0]) == 0xFF &&
        static_cast<uint8>(str[1]) == 0xFE) ||
       (static_cast<uint8>(str[0]) == 0xFE &&
        static_cast<uint8>(str[1]) == 0xFF))) {
    return UTF16;
  }

  // UTF-8 BOM.
  if (str.size() >= 3 &&
      static_cast<uint8>(str[0]) == 0xEF &&
      static_cast<uint8>(str[1]) == 0xBB &&
      static_cast<uint8>(str[2]) == 0xBF) {
    return UTF8;
  }

  // Count valid UTF8 characters.
  // TODO(taku): Improve the accuracy by making a DFA.
  const char *begin = str.data();
  const char *end = str.data() + str.size();
  size_t valid_utf8 = 0;
  size_t valid_script = 0;
  while (begin < end) {
    size_t mblen = 0;
    const char32 ucs4 = Util::UTF8ToUCS4(begin, end, &mblen);
    if (mblen == 0) {
      break;
    }
    ++valid_utf8;
    for (size_t i = 1; i < mblen; ++i) {
      if (begin[i] >= 0x80 && begin[i] <= 0xBF) {
        ++valid_utf8;
      }
    }

    // "\n\r\t " or Japanese code point
    if (ucs4 == 0x000A || ucs4 == 0x000D ||
        ucs4 == 0x0020 || ucs4 == 0x0009 ||
        Util::GetScriptType(ucs4) != Util::UNKNOWN_SCRIPT) {
      valid_script += mblen;
    }

    begin += mblen;
  }

  // TODO(taku): No theoretical justification for these parameters.
  if (1.0 * valid_utf8 / str.size() >= 0.9 &&
      1.0 * valid_script / str.size() >= 0.5) {
    return UTF8;
  }

  return SHIFT_JIS;
}

UserDictionaryImporter::EncodingType
UserDictionaryImporter::GuessFileEncodingType(const string &filename) {
  Mmap mmap;
  if (!mmap.Open(filename.c_str(), "r")) {
    LOG(ERROR) << "cannot open: " << filename;
    return NUM_ENCODINGS;
  }
  const size_t kMaxCheckSize = 1024;
  const size_t size = std::min(kMaxCheckSize, static_cast<size_t>(mmap.size()));
  const StringPiece mapped_data(static_cast<const char *>(mmap.begin()), size);
  return GuessEncodingType(mapped_data);
}

}  // namespace mozc
