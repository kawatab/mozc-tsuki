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

#ifndef MOZC_BASE_FILE_TEMP_DIR_H_
#define MOZC_BASE_FILE_TEMP_DIR_H_

#include <string>
#include <type_traits>
#include <utility>

#include "absl/status/statusor.h"

namespace mozc {

class TempDirectory;

// TempFile represents a temporary file created by TempDirectory.
// Deletes the file when this object goes out of scope.
// It doesn't result in an error when the file doesn't exit unlike FileUnlinker.
class TempFile {
 public:
  // Creates a new TempFile for path.
  explicit TempFile(std::string path) : path_(std::move(path)) {}
  TempFile(TempFile &&other) noexcept
      : path_(std::move(other.path_)), keep_(other.keep_) {
    other.keep_ = true;
  }
  TempFile &operator=(TempFile &&other) noexcept {
    this->swap(other);
    return *this;
  }

  // The destructor deletes the file.
  ~TempFile();

  constexpr const std::string &path() const { return path_; }

  void swap(TempFile &other) noexcept {
    static_assert(std::is_nothrow_swappable_v<decltype(path_)>);
    using std::swap;
    swap(path_, other.path_);
    swap(keep_, other.keep_);
  }

 private:
  std::string path_;
  // Delete a temp file by default.
  bool keep_ = false;
};

class TempDirectory {
 public:
  TempDirectory(TempDirectory &&other) noexcept
      : path_(std::move(other.path_)), keep_(other.keep_) {
    other.keep_ = true;
  }
  TempDirectory &operator=(TempDirectory &&other) noexcept {
    this->swap(other);
    return *this;
  }

  // The destructor deletes the directory if created by CreateTempDirectory.
  ~TempDirectory();

  // Default tries several common temporary paths and returns the path to the
  // first found. It returns an empty string when it couldn't find a directory.
  // We don't use absl::StattusOr<> here because it's expected to succeed most
  // of the time, and the next CreateTempFile or CreateTempDirectory call will
  // immediately fail.
  static TempDirectory Default();

  // Creates a unique temporary file in the directory and returns the path.
  // A bit long name but this is to avoid conflict with windows.h macros.
  absl::StatusOr<TempFile> CreateTempFile() const;
  // Creates a unique temporary directory in the directory and returns the path.
  absl::StatusOr<TempDirectory> CreateTempDirectory() const;

  constexpr const std::string &path() const { return path_; }
  constexpr bool keep() const { return keep_; }
  constexpr void set_keep(bool keep) { keep_ = keep; }

  void swap(TempDirectory &other) noexcept {
    static_assert(std::is_nothrow_swappable_v<decltype(path_)>);
    using std::swap;
    swap(path_, other.path_);
    swap(keep_, other.keep_);
  }

 private:
  explicit TempDirectory(std::string path) : path_(std::move(path)) {}
  TempDirectory(std::string path, bool keep)
      : path_(std::move(path)), keep_(keep) {}

  // path_ is the temporary directory path.
  std::string path_;
  // The default value for keep_ is true here because there are more code paths
  // where we don't want to delete the directory (also for safety).
  bool keep_ = true;
};

inline void swap(TempFile &lhs, TempFile &rhs) noexcept { lhs.swap(rhs); }

inline void swap(TempDirectory &lhs, TempDirectory &rhs) noexcept {
  lhs.swap(rhs);
}

}  // namespace mozc

#endif  // MOZC_BASE_FILE_TEMP_DIR_H_
