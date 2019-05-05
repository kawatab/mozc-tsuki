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

#ifndef MOZC_PREDICTION_ZERO_QUERY_DICT_H_
#define MOZC_PREDICTION_ZERO_QUERY_DICT_H_

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/port.h"
#include "base/serialized_string_array.h"

namespace mozc {

enum ZeroQueryType {
  ZERO_QUERY_NONE = 0,  // "☁" (symbol, non-unicode 6.0 emoji), and rule based.
  ZERO_QUERY_NUMBER_SUFFIX,  // "階" from "2"
  ZERO_QUERY_EMOTICON,  // "(>ω<)" from "うれしい"
  ZERO_QUERY_EMOJI,  // <umbrella emoji> from "かさ"
  // Following types are defined for usage stats.
  // The candidates of these types will not be stored at |ZeroQueryList|.
  // - "ヒルズ" from "六本木"
  // These candidates will be generated from dictionary entries
  // such as "六本木ヒルズ".
  ZERO_QUERY_BIGRAM,
  // - "に" from "六本木".
  // These candidates will be generated from suffix dictionary.
  ZERO_QUERY_SUFFIX,
};

// bit fields
enum ZeroQueryEmojiType {
  EMOJI_NONE = 0,
  EMOJI_UNICODE = 1,
  EMOJI_DOCOMO = 2,
  EMOJI_SOFTBANK = 4,
  EMOJI_KDDI = 8,
};

// Zero query dictionary is a multimap from string to a list of zero query
// entries, where each entry can be looked up by equal_range() method.  The data
// is serialized to two binary data: token array and string array.  Token array
// encodes an array of zero query entries, where each entry is encoded in 16
// bytes as follows:
//
// ZeroQueryEntry {
//   uint32 key_index:          4 bytes
//   uint32 value_index:        4 bytes
//   ZeroQueryType type:        2 bytes
//   uint16 emoji_type:         2 bytes
//   uint32 emoji_android_pua:  4 bytes
// }
//
// The token array is sorted in ascending order of key_index for binary search.
// String values of key and value are encoded separately in the string array,
// which can be extracted by using |key_index| and |value_index|.  The string
// array is also sorted in ascending order of strings.  For the serialization
// format of string array, see base/serialized_string_array.h".
class ZeroQueryDict {
 public:
  static const size_t kTokenByteSize = 16;

  class iterator : public std::iterator<std::random_access_iterator_tag,
                                        uint32> {
   public:
    iterator(const char *ptr, const SerializedStringArray *array)
        : ptr_(ptr), string_array_(array) {}
    iterator(const iterator& x) = default;
    iterator& operator=(const iterator& x) = default;

    uint32 operator*() const { return key_index(); }

    uint32 key_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_);
    }

    uint32 value_index() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 4);
    }

    ZeroQueryType type() const {
      const uint16 val = *reinterpret_cast<const uint16 *>(ptr_ + 8);
      return static_cast<ZeroQueryType>(val);
    }

    uint16 emoji_type() const {
      return *reinterpret_cast<const uint16 *>(ptr_ + 10);
    }

    uint32 emoji_android_pua() const {
      return *reinterpret_cast<const uint32 *>(ptr_ + 12);
    }

    StringPiece key() const { return (*string_array_)[key_index()]; }
    StringPiece value() const { return (*string_array_)[value_index()]; }

    iterator &operator++() {
      ptr_ += kTokenByteSize;
      return *this;
    }

    iterator operator++(int) {
      const iterator tmp(ptr_, string_array_);
      ptr_ += kTokenByteSize;
      return tmp;
    }

    iterator &operator+=(ptrdiff_t n) {
      ptr_ += n * kTokenByteSize;
      return *this;
    }

    friend iterator operator+(iterator iter, ptrdiff_t n) {
      iter += n;
      return iter;
    }

    friend iterator operator+(ptrdiff_t n, iterator iter) {
      iter += n;
      return iter;
    }

    iterator &operator-=(ptrdiff_t n) {
      ptr_ -= n * kTokenByteSize;
      return *this;
    }

    friend iterator operator-(iterator iter, ptrdiff_t n) {
      iter -= n;
      return iter;
    }

    friend ptrdiff_t operator-(iterator x, iterator y) {
      return (x.ptr_ - y.ptr_) / kTokenByteSize;
    }

    friend bool operator==(iterator x, iterator y) {
      return x.ptr_ == y.ptr_;
    }

    friend bool operator!=(iterator x, iterator y) {
      return x.ptr_ != y.ptr_;
    }

    friend bool operator<(iterator x, iterator y) {
      return x.ptr_ < y.ptr_;
    }

    friend bool operator<=(iterator x, iterator y) {
      return x.ptr_ <= y.ptr_;
    }

    friend bool operator>(iterator x, iterator y) {
      return x.ptr_ > y.ptr_;
    }

    friend bool operator>=(iterator x, iterator y) {
      return x.ptr_ >= y.ptr_;
    }

   private:
    const char *ptr_;
    const SerializedStringArray * string_array_;
  };

  void Init(StringPiece token_array_data, StringPiece string_array_data) {
    token_array_ = token_array_data;
    string_array_.Set(string_array_data);
  }

  iterator begin() const {
    return iterator(token_array_.data(), &string_array_);
  }

  iterator end() const {
    return iterator(token_array_.data() + token_array_.size(),
                    &string_array_);
  }

  std::pair<iterator, iterator> equal_range(StringPiece key) const {
    const auto iter = std::lower_bound(string_array_.begin(),
                                       string_array_.end(), key);
    if (iter == string_array_.end() || *iter != key) {
      return std::pair<iterator, iterator>(end(), end());
    }
    return std::equal_range(begin(), end(), iter.index());
  }

 private:
  StringPiece token_array_;
  SerializedStringArray string_array_;
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_ZERO_QUERY_DICT_H_
