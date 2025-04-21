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

#ifndef MOZC_DICTIONARY_SUPPRESSION_DICTIONARY_H_
#define MOZC_DICTIONARY_SUPPRESSION_DICTIONARY_H_

#include <atomic>
#include <functional>
#include <string>
#include <utility>

#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace dictionary {

// Provides a functionality to test if a word should be suppressed in conversion
// results. This class is not thread safe in general use but is safe under
// single-producer single-consumer model, provided that the usage is correct. In
// our usage, the producer is UserDictionary::UserDictionaryReloader thread and
// the consumer is the main converter thread.
class SuppressionDictionary final {
 public:
  SuppressionDictionary() = default;
  SuppressionDictionary(const SuppressionDictionary &) = delete;
  SuppressionDictionary &operator=(const SuppressionDictionary &) = delete;

  // Methods for the producer thread. The thread must obey this edit pattern:
  //
  // Lock();
  // Calls of AddEntry() and/or Clear()
  // Unlock();
  //
  // The producer thread must not call the other methods.

  // Locks the dictionary (the producer thread is blocked until it gets the
  // lock). Should not be called recursively.
  void Lock();

  // Unlocks the dictionary.
  void UnLock();

  // Adds an entry into the dictionary.
  bool AddEntry(std::string key, std::string value);

  // Clears the dictionary.
  void Clear();

  // Returns true if the dictionary is locked. This method is for debugging.
  bool IsLocked() const { return locked_.load(std::memory_order_relaxed); }

  // Methods for the consumer thread. If the producer thread is updating the
  // dictionary contents, the following methods behave as if the dictionary is
  // empty.

  // Returns true if SuppressionDictionary doesn't have any entries.
  bool IsEmpty() const;

  // Returns true if a word having `key` and `value` should be suppressed.
  bool SuppressEntry(absl::string_view key, absl::string_view value) const;

 private:
  using KeyValue = std::pair<std::string, std::string>;
  using KeyValueView = std::pair<absl::string_view, absl::string_view>;
  struct KeyValueHash : public absl::Hash<KeyValueView> {
    using is_transparent = void;
  };
  struct KeyValueEq : public std::equal_to<KeyValueView> {
    using is_transparent = void;
  };

  absl::flat_hash_set<KeyValue, KeyValueHash, KeyValueEq> keys_values_;
  absl::flat_hash_set<std::string> keys_only_;
  absl::flat_hash_set<std::string> values_only_;

  mutable std::atomic<bool> locked_ = false;
  // TODO(noriyukit): Check if this mutex is still necessary.
  absl::Mutex mutex_;
};

class SuppressionDictionaryLock final {
 public:
  explicit SuppressionDictionaryLock(SuppressionDictionary *dic) : dic_{dic} {
    dic_->Lock();
  }
  ~SuppressionDictionaryLock() { dic_->UnLock(); }

 private:
  SuppressionDictionary *dic_;
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_SUPPRESSION_DICTIONARY_H_
