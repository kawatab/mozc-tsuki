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

// This code manages ar/cpio/tar like file structure that contains
// multiple sections in a file. Each section has a name and size.

#ifndef MOZC_DICTIONARY_FILE_DICTIONARY_FILE_H_
#define MOZC_DICTIONARY_FILE_DICTIONARY_FILE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/mmap.h"
#include "base/port.h"
#include "dictionary/file/codec_interface.h"

namespace mozc {
namespace dictionary {

struct DictionaryFileSection;

class DictionaryFile {
 public:
  explicit DictionaryFile(const DictionaryFileCodecInterface *file_codec);
  ~DictionaryFile();

  // Open from file
  bool OpenFromFile(const string &file);

  // Open from memory
  bool OpenFromImage(const char* image, int len);

  // Get pointer section from name
  // Image size is set to |len|
  // Return NULL when not found
  const char *GetSection(const string &section_name, int *len) const;

 private:
  // DictionaryFile does not take the ownership of |file_codec_|.
  const DictionaryFileCodecInterface *file_codec_;
  // This will be nullptr if the mapping source is given as a pointer.
  std::unique_ptr<Mmap> mapping_;
  std::vector<DictionaryFileSection> sections_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryFile);
};

}  // namespace dictionary
}  // namespace mozc

#endif  // MOZC_DICTIONARY_FILE_DICTIONARY_FILE_H_
