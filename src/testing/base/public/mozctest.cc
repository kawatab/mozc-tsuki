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

#include "testing/base/public/mozctest.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/system_util.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"

namespace mozc {
namespace testing {

string GetSourcePath(const std::vector<StringPiece> &components) {
  std::vector<StringPiece> abs_components = {
    FLAGS_test_srcdir,
  };
  abs_components.insert(abs_components.end(),
                        components.begin(), components.end());
  return FileUtil::JoinPath(abs_components);
}

string GetSourceFileOrDie(const std::vector<StringPiece> &components) {
  const string path = GetSourcePath(components);
  CHECK(FileUtil::FileExists(path)) << "File doesn't exist: " << path;
  return path;
}

string GetSourceDirOrDie(const std::vector<StringPiece> &components) {
  const string path = GetSourcePath(components);
  CHECK(FileUtil::DirectoryExists(path)) << "Directory doesn't exist: " << path;
  return path;
}

std::vector<string> GetSourceFilesInDirOrDie(
    const std::vector<StringPiece> &dir_components,
    const std::vector<StringPiece> &filenames) {
  const string dir = GetSourceDirOrDie(dir_components);
  std::vector<string> paths;
  for (size_t i = 0; i < filenames.size(); ++i) {
    paths.push_back(FileUtil::JoinPath({dir, filenames[i]}));
    CHECK(FileUtil::FileExists(paths.back()))
        << "File doesn't exist: " << paths.back();
  }
  return paths;
}

ScopedTmpUserProfileDirectory::ScopedTmpUserProfileDirectory()
    : original_dir_(SystemUtil::GetUserProfileDirectory()) {
  SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
}

ScopedTmpUserProfileDirectory::~ScopedTmpUserProfileDirectory() {
  SystemUtil::SetUserProfileDirectory(original_dir_);
}

}  // namespace testing
}  // namespace mozc
