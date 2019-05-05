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

#ifndef MOZC_TESTING_BASE_PUBLIC_MOZCTEST_H_
#define MOZC_TESTING_BASE_PUBLIC_MOZCTEST_H_

#include <string>
#include <vector>

#include "base/port.h"
#include "base/string_piece.h"

namespace mozc {
namespace testing {

// Gets an absolute path of test resource from path components relative to
// mozc's root directory.
//
// Example:
//
// string path = GetSourcePath({"data", "test", "dictionary", "id.def"});
//
// This call gives the absolute path to data/test/dictionary/id.def. (Note that
// the actual result is separated by OS-specific path separator.)
string GetSourcePath(const std::vector<StringPiece> &components);

// Gets an absolute path of test resource file.  If the file doesn't exist,
// terminates the program.
string GetSourceFileOrDie(const std::vector<StringPiece> &components);

// Gets an absolute path of test resource directory.  If the directory doesn't
// exist, terminates the program.
string GetSourceDirOrDie(const std::vector<StringPiece> &components);

// Gets absolute paths of test resource files in a directory.  If one of files
// don't exit, terminates the program.
//
// vector<string> paths = GetSourceDirOrDie({"my", "dir"}, {"file1", "file2"});
// paths = {
//   "/test/srcdir/my/dir/file1",
//   "/test/srcdir/my/dir/file2",
// };
std::vector<string> GetSourceFilesInDirOrDie(
    const std::vector<StringPiece> &dir_components,
    const std::vector<StringPiece> &filenames);

// Temporarily sets the user profile directory to FLAGS_test_tmpdir during the
// scope.  The original directory is restored at the end of the scope.
class ScopedTmpUserProfileDirectory {
 public:
  ScopedTmpUserProfileDirectory();
  ~ScopedTmpUserProfileDirectory();

 private:
  const string original_dir_;
};

}  // namespace testing
}  // namespace mozc

#endif  // MOZC_TESTING_BASE_PUBLIC_MOZCTEST_H_
