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

#include "dictionary/user_pos.h"

#include <algorithm>
#include <set>

#include "base/logging.h"
#include "base/util.h"

namespace mozc {
namespace dictionary {

UserPOS::UserPOS(StringPiece token_array_data, StringPiece string_array_data)
    : token_array_data_(token_array_data) {
  DCHECK_EQ(token_array_data.size() % 8, 0);
  DCHECK(SerializedStringArray::VerifyData(string_array_data));
  string_array_.Set(string_array_data);
}

UserPOS::~UserPOS() = default;

void UserPOS::GetPOSList(std::vector<string> *pos_list) const {
  pos_list->clear();
  std::set<uint16> seen;
  for (auto iter = begin(); iter != end(); ++iter) {
    if (!seen.insert(iter.pos_index()).second) {
      continue;
    }
    const StringPiece pos = string_array_[iter.pos_index()];
    pos_list->emplace_back(pos.data(), pos.size());
  }
}

bool UserPOS::IsValidPOS(const string &pos) const {
  const auto iter =
      std::lower_bound(string_array_.begin(), string_array_.end(), pos);
  if (iter == string_array_.end()) {
    return false;
  }
  return std::binary_search(begin(), end(), iter.index());
}

bool UserPOS::GetPOSIDs(const string &pos, uint16 *id) const {
  const auto str_iter =
      std::lower_bound(string_array_.begin(), string_array_.end(), pos);
  if (str_iter == string_array_.end() || *str_iter != pos) {
    return false;
  }
  const auto token_iter = std::lower_bound(begin(), end(), str_iter.index());
  if (token_iter == end() || token_iter.pos_index() != str_iter.index()) {
    return false;
  }
  *id = token_iter.conjugation_id();
  return true;
}

bool UserPOS::GetTokens(const string &key, const string &value,
                        const string &pos, std::vector<Token> *tokens) const {
  if (key.empty() || value.empty() || pos.empty() || tokens == nullptr) {
    return false;
  }

  tokens->clear();
  const auto str_iter =
      std::lower_bound(string_array_.begin(), string_array_.end(), pos);
  if (str_iter == string_array_.end() || *str_iter != pos) {
    return false;
  }
  std::pair<iterator, iterator> range =
      std::equal_range(begin(), end(), str_iter.index());
  if (range.first == range.second) {
    return false;
  }
  const size_t size = range.second - range.first;
  CHECK_GE(size, 1);
  tokens->resize(size);

  // TODO(taku)  Change the cost by seeing cost_type
  const int16 kDefaultCost = 5000;

  // Set smaller cost for "短縮よみ" in order to make
  // the rank of the word higher than others.
  const int16 kIsolatedWordCost = 200;
  const char kIsolatedWordPOS[] = "短縮よみ";

  if (size == 1) {  // no conjugation
    const auto &token_iter = range.first;
    (*tokens)[0].key = key;
    (*tokens)[0].value = value;
    (*tokens)[0].id = token_iter.conjugation_id();
    if (pos == kIsolatedWordPOS) {
      (*tokens)[0].cost = kIsolatedWordCost;
    } else {
      (*tokens)[0].cost = kDefaultCost;
    }
  } else {
    const auto &base_form_token_iter = range.first;
    // expand all other forms
    string key_stem = key;
    string value_stem = value;
    // assume that conjugation_form[0] contains the suffix of "base form".
    const StringPiece base_key_suffix =
        string_array_[base_form_token_iter.key_suffix_index()];
    const StringPiece base_value_suffix =
        string_array_[base_form_token_iter.value_suffix_index()];

    if (base_key_suffix.size() < key.size() &&
        base_value_suffix.size() < value.size() &&
        Util::EndsWith(key, base_key_suffix) &&
        Util::EndsWith(value, base_value_suffix)) {
      key_stem.assign(key, 0, key.size() - base_key_suffix.size());
      value_stem.assign(value, 0, value.size() - base_value_suffix.size());
    }
    for (size_t i = 0; i < size; ++i, ++range.first) {
      const auto &token_iter = range.first;
      const StringPiece key_suffix =
          string_array_[token_iter.key_suffix_index()];
      const StringPiece value_suffix =
          string_array_[token_iter.value_suffix_index()];
      Util::ConcatStrings(key_stem, key_suffix, &(*tokens)[i].key);
      Util::ConcatStrings(value_stem, value_suffix, &(*tokens)[i].value);
      (*tokens)[i].id = token_iter.conjugation_id();
      (*tokens)[i].cost = kDefaultCost;
    }
    DCHECK(range.first == range.second);
  }

  return true;
}

UserPOS *UserPOS::CreateFromDataManager(const DataManagerInterface &manager) {
  StringPiece token_array_data, string_array_data;
  manager.GetUserPOSData(&token_array_data, &string_array_data);
  return new UserPOS(token_array_data, string_array_data);
}

}  // namespace dictionary
}  // namespace mozc
