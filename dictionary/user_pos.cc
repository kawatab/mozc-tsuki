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

#include "dictionary/user_pos.h"

#include <algorithm>
#include <map>

#include "base/logging.h"
#include "base/util.h"

namespace mozc {

UserPOS::UserPOS(const POSToken *pos_token_array)
    : pos_token_array_(pos_token_array) {
  DCHECK(pos_token_array_);
  for (size_t i = 0; pos_token_array_[i].pos != NULL; ++i) {
    pos_map_.insert(make_pair(string(pos_token_array_[i].pos),
                              &pos_token_array_[i]));
  }
  CHECK_GT(pos_map_.size(), 1);
}

void UserPOS::GetPOSList(vector<string> *pos_list) const {
  pos_list->clear();
  for (size_t i = 0; pos_token_array_[i].pos != NULL; ++i) {
    pos_list->push_back(pos_token_array_[i].pos);
  }
}

bool UserPOS::IsValidPOS(const string &pos) const {
  map<string, const POSToken*>::const_iterator it = pos_map_.find(pos);
  return it != pos_map_.end();
}

bool UserPOS::GetPOSIDs(const string &pos, uint16 *id) const {
  map<string, const POSToken*>::const_iterator it = pos_map_.find(pos);
  if (it == pos_map_.end()) {
    return false;
  }

  const ConjugationType *conjugation_form = it->second->conjugation_form;
  CHECK(conjugation_form);

  *id = conjugation_form[0].id;

  return true;
}

bool UserPOS::GetTokens(const string &key,
                        const string &value,
                        const string &pos,
                        vector<Token> *tokens) const {
  if (key.empty() ||
      value.empty() ||
      pos.empty() ||
      tokens == NULL) {
    return false;
  }

  tokens->clear();
  map<string, const POSToken*>::const_iterator it = pos_map_.find(pos);
  if (it == pos_map_.end()) {
    return false;
  }

  const ConjugationType *conjugation_form = it->second->conjugation_form;
  CHECK(conjugation_form);

  const size_t size = static_cast<size_t>(it->second->conjugation_size);
  CHECK_GE(size, 1);
  tokens->resize(size);

  // TODO(taku)  Change the cost by seeing cost_type
  const int16 kDefaultCost = 5000;

  // Set smaller cost for "短縮よみ" in order to make
  // the rank of the word higher than others.
  const int16 kIsolatedWordCost = 200;
  const char kIsolatedWordPOS[]
      = "\xE7\x9F\xAD\xE7\xB8\xAE\xE3\x82\x88\xE3\x81\xBF";

  if (size == 1) {  // no conjugation
    (*tokens)[0].key = key;
    (*tokens)[0].value = value;
    (*tokens)[0].id = conjugation_form[0].id;
    if (pos == kIsolatedWordPOS) {
      (*tokens)[0].cost= kIsolatedWordCost;
    } else {
      (*tokens)[0].cost= kDefaultCost;
    }
  } else {
    // expand all other forms
    string key_stem = key;
    string value_stem = value;
    // assume that conjugation_form[0] contains the suffix of "base form".
    const string base_key_suffix = conjugation_form[0].key_suffix;
    const string base_value_suffix = conjugation_form[0].value_suffix;
    if (base_key_suffix.size() < key.size() &&
        base_value_suffix.size() < value.size() &&
        Util::EndsWith(key, base_key_suffix) &&
        Util::EndsWith(value, base_value_suffix)) {
      key_stem.assign(key, 0, key.size() - base_key_suffix.size());
      value_stem.assign(value, 0, value.size() - base_value_suffix.size());
    }
    for (size_t i = 0; i < size; ++i) {
      (*tokens)[i].key   = key_stem   + conjugation_form[i].key_suffix;
      (*tokens)[i].value = value_stem + conjugation_form[i].value_suffix;
      (*tokens)[i].id    = conjugation_form[i].id;
      (*tokens)[i].cost  = kDefaultCost;
    }
  }

  return true;
}

}  // namespace mozc
