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

#include "base/config_file_stream.h"

#include <cstring>
#include <ios>
#include <istream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

namespace mozc {
namespace {

constexpr absl::string_view kSystemPrefix = "system://";
constexpr absl::string_view kUserPrefix = "user://";
constexpr absl::string_view kFilePrefix = "file://";
constexpr absl::string_view kMemoryPrefix = "memory://";

struct FileData {
  absl::string_view name;
  absl::string_view data;
};

#include "base/config_file_stream_data.inc"

class OnMemoryFileMap {
 public:
  const std::string &get(const absl::string_view key) const {
    auto it = map_.find(key);
    if (it != map_.end()) {
      return it->second;
    }
    return empty_string_;
  }

  void set(const absl::string_view key, const std::string &value) {
    map_[key] = value;
  }

  void clear() { map_.clear(); }

 private:
  absl::flat_hash_map<std::string, std::string> map_;
  const std::string empty_string_;
};
}  // namespace

std::unique_ptr<std::istream> ConfigFileStream::Open(
    const std::string &filename, std::ios_base::openmode mode) {
  // system://foo.bar.txt
  if (absl::StartsWith(filename, kSystemPrefix)) {
    const absl::string_view new_filename =
        absl::StripPrefix(filename, kSystemPrefix);
    for (size_t i = 0; i < std::size(kFileData); ++i) {
      if (new_filename == kFileData[i].name) {
        auto ifs = std::make_unique<std::istringstream>(
            std::string(kFileData[i].data), mode);
        CHECK(ifs);
        if (ifs->good()) {
          return ifs;
        }
        return nullptr;
      }
    }
    // user://foo.bar.txt
  } else if (absl::StartsWith(filename, kUserPrefix)) {
    const std::string new_filename =
        FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                           absl::StripPrefix(filename, kUserPrefix));
    auto ifs = std::make_unique<InputFileStream>(new_filename, mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    return nullptr;
    // file:///foo.map
  } else if (absl::StartsWith(filename, kFilePrefix)) {
    const absl::string_view new_filename =
        absl::StripPrefix(filename, kFilePrefix);
    auto ifs =
        std::make_unique<InputFileStream>(std::string(new_filename), mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    return nullptr;
  } else if (absl::StartsWith(filename, kMemoryPrefix)) {
    auto ifs = std::make_unique<std::istringstream>(
        Singleton<OnMemoryFileMap>::get()->get(filename), mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    return nullptr;
  } else {
    LOG(WARNING) << filename << " has no prefix. open from localfile";
    auto ifs = std::make_unique<InputFileStream>(filename, mode);
    CHECK(ifs);
    if (ifs->good()) {
      return ifs;
    }
    return nullptr;
  }

  return nullptr;
}

bool ConfigFileStream::AtomicUpdate(const std::string &filename,
                                    const std::string &new_binary_contens) {
  if (absl::StartsWith(filename, kMemoryPrefix)) {
    Singleton<OnMemoryFileMap>::get()->set(filename, new_binary_contens);
    return true;
  } else if (absl::StartsWith(filename, kSystemPrefix)) {
    LOG(ERROR) << "Cannot update system:// files.";
    return false;
  }
  // We should save the new config first,
  // as we may rewrite the original config according to platform.
  // The original config should be platform independent.
  const std::string real_filename = GetFileName(filename);
  if (real_filename.empty()) {
    return false;
  }

  const std::string tmp_filename = absl::StrCat(real_filename, ".tmp");
  if (absl::Status s = FileUtil::SetContents(tmp_filename, new_binary_contens);
      !s.ok()) {
    LOG(ERROR) << "Cannot write the contents to " << tmp_filename << ": " << s;
    return false;
  }

  if (absl::Status s = FileUtil::AtomicRename(tmp_filename, real_filename);
      !s.ok()) {
    LOG(ERROR) << "AtomicRename failed: " << s << "; from: " << tmp_filename
               << ", to: " << real_filename;
    return false;
  }

#ifdef _WIN32
  // If file name doesn't end with ".db", the file
  // is more likely a temporary file.
  if (!absl::EndsWith(real_filename, ".db")) {
    // TODO(yukawa): Provide a way to
    // integrate ::SetFileAttributesTransacted with
    // AtomicRename.
    if (!FileUtil::HideFile(real_filename)) {
      LOG(ERROR) << "Cannot make hidden: " << real_filename << " "
                 << ::GetLastError();
    }
  }
#endif  // _WIN32
  return true;
}

std::string ConfigFileStream::GetFileName(const absl::string_view filename) {
  if (absl::StartsWith(filename, kSystemPrefix) ||
      absl::StartsWith(filename, kMemoryPrefix)) {
    return "";
  } else if (absl::StartsWith(filename, kUserPrefix)) {
    return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                              absl::StripPrefix(filename, kUserPrefix));
  } else if (absl::StartsWith(filename, kFilePrefix)) {
    return std::string(absl::StripPrefix(filename, kUserPrefix));
  } else {
    LOG(WARNING) << filename << " has no prefix. open from localfile";
    return std::string(filename);
  }
  return "";
}

void ConfigFileStream::ClearOnMemoryFiles() {
  Singleton<OnMemoryFileMap>::get()->clear();
}
}  // namespace mozc
