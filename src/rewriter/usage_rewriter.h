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

#ifndef MOZC_REWRITER_USAGE_REWRITER_H_
#define MOZC_REWRITER_USAGE_REWRITER_H_

#ifndef NO_USAGE_REWRITER

#include <map>
#include <string>
#include <utility>

#include "base/port.h"
#include "base/serialized_string_array.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/rewriter_interface.h"
#include "testing/base/public/gunit_prod.h"  // for FRIEND_TEST()

namespace mozc {

class DataManagerInterface;

class UsageRewriter : public RewriterInterface  {
 public:
  UsageRewriter(const DataManagerInterface *data_manager,
                const dictionary::DictionaryInterface *dictionary);
  virtual ~UsageRewriter();
  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const;

  // better to show usage when user type "tab" key.
  virtual int capability(const ConversionRequest &request) const {
    return CONVERSION | PREDICTION;
  }

 private:
  FRIEND_TEST(UsageRewriterTest, GetKanjiPrefixAndOneHiragana);

  static const size_t kUsageItemByteLength = 20;

  class UsageDictItemIterator {
   public:
    UsageDictItemIterator() : ptr_(nullptr) {}
    explicit UsageDictItemIterator(const char *ptr) : ptr_(ptr) {}

    size_t usage_id() const { return *reinterpret_cast<const uint32 *>(ptr_); }
    size_t key_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 4);
    }
    size_t value_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 8);
    }
    size_t conjugation_id() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 12);
    }
    size_t meaning_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 16);
    }

    UsageDictItemIterator &operator++() {
      ptr_ += kUsageItemByteLength;
      return *this;
    }

    bool IsValid() const { return ptr_ != nullptr; }

    friend bool operator==(UsageDictItemIterator x, UsageDictItemIterator y) {
      return x.ptr_ == y.ptr_;
    }

    friend bool operator!=(UsageDictItemIterator x, UsageDictItemIterator y) {
      return x.ptr_ != y.ptr_;
    }

   private:
    const char *ptr_;
  };

  using StrPair = std::pair<string, string>;
  static string GetKanjiPrefixAndOneHiragana(const string &word);

  UsageDictItemIterator LookupUnmatchedUsageHeuristically(
      const Segment::Candidate &candidate) const;
  UsageDictItemIterator LookupUsage(
      const Segment::Candidate &candidate) const;

  std::map<StrPair, UsageDictItemIterator> key_value_usageitem_map_;
  const dictionary::POSMatcher pos_matcher_;
  const dictionary::DictionaryInterface *dictionary_;
  const uint32 *base_conjugation_suffix_;
  SerializedStringArray string_array_;
};

}  // namespace mozc

#endif  // NO_USAGE_REWRITER

#endif  // MOZC_REWRITER_USAGE_REWRITER_H_
