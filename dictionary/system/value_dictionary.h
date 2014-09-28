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

// This dictionary supports system dicitonary that will belookup from
// value, rather than key.
// Token's key, cost, ids will not be looked up due to performance concern

#ifndef MOZC_DICTIONARY_SYSTEM_VALUE_DICTIONARY_H_
#define MOZC_DICTIONARY_SYSTEM_VALUE_DICTIONARY_H_

#include <string>

#include "base/port.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "dictionary/dictionary_interface.h"

namespace mozc {

namespace dictionary {
class SystemDictionaryCodecInterface;
}  // namespace dictionary

namespace storage {
namespace louds {
class LoudsTrie;
}  // namespace louds
}  // namespace storage

class DictionaryFile;
class POSMatcher;

namespace dictionary {

class ValueDictionary : public DictionaryInterface {
 public:
  virtual ~ValueDictionary();

  static ValueDictionary *CreateValueDictionaryFromFile(
      const POSMatcher& pos_matcher, const string &filename);

  static ValueDictionary *CreateValueDictionaryFromImage(
      const POSMatcher& pos_matcher, const char *ptr, int len);

  // Implementation of DictionaryInterface
  virtual bool HasValue(StringPiece value) const;
  virtual void LookupPredictive(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const;
  virtual void LookupPrefix(
      StringPiece key, bool use_kana_modifier_insensitive_lookup,
      Callback *callback) const;
  virtual void LookupExact(StringPiece key, Callback *callback) const;
  virtual void LookupReverse(
      StringPiece str, NodeAllocatorInterface *allocator,
      Callback *callback) const;

 private:
  explicit ValueDictionary(const POSMatcher& pos_matcher);

  bool OpenDictionaryFile();

  scoped_ptr<mozc::storage::louds::LoudsTrie> value_trie_;
  scoped_ptr<DictionaryFile> dictionary_file_;
  const SystemDictionaryCodecInterface *codec_;
  const uint16 suggestion_only_word_id_;

  DISALLOW_COPY_AND_ASSIGN(ValueDictionary);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SYSTEM_VALUE_DICTIONARY_H_
