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

#include "data_manager/serialized_dictionary.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/number_util.h"
#include "base/port.h"
#include "base/serialized_string_array.h"
#include "base/string_piece.h"
#include "base/util.h"

namespace mozc {
namespace {

using CompilerToken = SerializedDictionary::CompilerToken;
using TokenList = SerializedDictionary::TokenList;

struct CompareByCost {
  bool operator()(const std::unique_ptr<CompilerToken> &t1,
                  const std::unique_ptr<CompilerToken> &t2) const {
    return t1->cost < t2->cost;
  }
};

void LoadTokens(std::istream *ifs, std::map<string, TokenList> *dic) {
  dic->clear();
  string line;
  std::vector<string> fields;
  while (!getline(*ifs, line).fail()) {
    fields.clear();
    Util::SplitStringUsing(line, "\t", &fields);
    CHECK_GE(fields.size(), 4);
    std::unique_ptr<CompilerToken> token(new CompilerToken);
    const string &key = fields[0];
    token->value = fields[4];
    CHECK(NumberUtil::SafeStrToUInt16(fields[1], &token->lid));
    CHECK(NumberUtil::SafeStrToUInt16(fields[2], &token->rid));
    CHECK(NumberUtil::SafeStrToInt16(fields[3], &token->cost));
    token->description = (fields.size() > 5) ? fields[5] : "";
    token->additional_description = (fields.size() > 6) ? fields[6] : "";
    (*dic)[key].push_back(std::move(token));
  }

  for (auto &kv : *dic) {
    std::sort(kv.second.begin(), kv.second.end(), CompareByCost());
  }
}

}  // namespace

SerializedDictionary::SerializedDictionary(StringPiece token_array,
                                           StringPiece string_array_data)
    : token_array_(token_array) {
  DCHECK(VerifyData(token_array, string_array_data));
  string_array_.Set(string_array_data);
}

SerializedDictionary::~SerializedDictionary() {}

SerializedDictionary::IterRange SerializedDictionary::equal_range(
    StringPiece key) const {
  // TODO(noriyukit): Instead of comparing key as string, we can do binary
  // search using key index to minimize string comparison cost.
  return std::equal_range(begin(), end(), key);
}

std::pair<StringPiece, StringPiece> SerializedDictionary::Compile(
    std::istream *input,
    std::unique_ptr<uint32[]> *output_token_array_buf,
    std::unique_ptr<uint32[]> *output_string_array_buf) {
  std::map<string, TokenList> dic;
  LoadTokens(input, &dic);
  return Compile(dic, output_token_array_buf, output_string_array_buf);
}

std::pair<StringPiece, StringPiece> SerializedDictionary::Compile(
    const std::map<string, TokenList> &dic,
    std::unique_ptr<uint32[]> *output_token_array_buf,
    std::unique_ptr<uint32[]> *output_string_array_buf) {
  CHECK(Util::IsLittleEndian());

  // Build a mapping from string to its index in a serialized string array.
  // Note that duplicate keys share the same index, so data is slightly
  // compressed.
  std::map<string, uint32> string_index;
  for (const auto &kv : dic) {
    // This phase just collects all the strings and temporarily assigns 0 as
    // index.
    string_index[kv.first] = 0;
    for (const auto &token_ptr : kv.second) {
      string_index[token_ptr->value] = 0;
      string_index[token_ptr->description] = 0;
      string_index[token_ptr->additional_description] = 0;
    }
  }
  {
    // This phase assigns index in ascending order of strings.
    uint32 index = 0;
    for (auto &kv : string_index) {
      kv.second = index++;
    }
  }

  // Build a token array binary data.
  StringPiece token_array;
  {
    string buf;
    for (const auto &kv : dic) {
      const uint32 key_index = string_index[kv.first];
      for (const auto &token_ptr : kv.second) {
        const uint32 value_index = string_index[token_ptr->value];
        const uint32 desc_index = string_index[token_ptr->description];
        const uint32 adddesc_index =
            string_index[token_ptr->additional_description];
        buf.append(reinterpret_cast<const char *>(&key_index), 4);
        buf.append(reinterpret_cast<const char *>(&value_index), 4);
        buf.append(reinterpret_cast<const char *>(&desc_index), 4);
        buf.append(reinterpret_cast<const char *>(&adddesc_index), 4);
        buf.append(reinterpret_cast<const char *>(&token_ptr->lid), 2);
        buf.append(reinterpret_cast<const char *>(&token_ptr->rid), 2);
        buf.append(reinterpret_cast<const char *>(&token_ptr->cost), 2);
        buf.append("\x00\x00", 2);
      }
    }
    output_token_array_buf->reset(new uint32[(buf.size() + 3) / 4]);
    memcpy(output_token_array_buf->get(), buf.data(), buf.size());
    token_array = StringPiece(
        reinterpret_cast<const char*>(output_token_array_buf->get()),
        buf.size());
  }

  // Build a string array.
  StringPiece string_array;
  {
    // Copy the map keys to vector.  Note: since map's iteration is ordered,
    // each string is placed at the desired index.
    std::vector<StringPiece> strings;
    for (const auto &kv : string_index) {
      // Guarantee that the string is inserted at its indexed position.
      CHECK_EQ(strings.size(), kv.second);
      strings.emplace_back(kv.first);
    }
    string_array = SerializedStringArray::SerializeToBuffer(
        strings, output_string_array_buf);
  }

  return std::pair<StringPiece, StringPiece>(token_array, string_array);
}

void SerializedDictionary::CompileToFiles(const string &input,
                                          const string &output_token_array,
                                          const string &output_string_array) {
  InputFileStream ifs(input.c_str());
  CHECK(ifs.good());
  std::map<string, TokenList> dic;
  LoadTokens(&ifs, &dic);
  CompileToFiles(dic, output_token_array, output_string_array);
}

void SerializedDictionary::CompileToFiles(
    const std::map<string, TokenList> &dic, const string &output_token_array,
    const string &output_string_array) {
  std::unique_ptr<uint32[]> buf1, buf2;
  const std::pair<StringPiece, StringPiece> data = Compile(dic, &buf1, &buf2);
  CHECK(VerifyData(data.first, data.second));

  OutputFileStream token_ofs(output_token_array.c_str(),
                             std::ios_base::out | std::ios_base::binary);
  CHECK(token_ofs.good());
  CHECK(token_ofs.write(data.first.data(), data.first.size()));

  OutputFileStream string_ofs(output_string_array.c_str(),
                              std::ios_base::out | std::ios_base::binary);
  CHECK(string_ofs.good());
  CHECK(string_ofs.write(data.second.data(), data.second.size()));
}

bool SerializedDictionary::VerifyData(StringPiece token_array_data,
                                      StringPiece string_array_data) {
  if (token_array_data.size() % kTokenByteLength != 0) {
    return false;
  }
  SerializedStringArray string_array;
  if (!string_array.Init(string_array_data)) {
    return false;
  }
  for (const char *ptr = token_array_data.data();
       ptr != token_array_data.data() + token_array_data.size();
       ptr += kTokenByteLength) {
    const uint32 *u32_ptr = reinterpret_cast<const uint32 *>(ptr);
    if (u32_ptr[0] >= string_array.size() ||
        u32_ptr[1] >= string_array.size() ||
        u32_ptr[2] >= string_array.size() ||
        u32_ptr[3] >= string_array.size()) {
      return false;
    }
  }
  return true;
}

}  // namespace mozc
