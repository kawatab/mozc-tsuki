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

#include "base/multifile.h"


#include <string>
#include <vector>

#include "base/flags.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/util.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {


TEST(InputMultiFileTest, OpenNonexistentFilesTest) {
  // Empty string
  {
    InputMultiFile multfile("");
    string line;
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
  }

  // Signle path
  {
    const string path = FileUtil::JoinPath(FLAGS_test_tmpdir,
                                           "this_file_does_not_exist");
    InputMultiFile multfile(path);
    string line;
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
  }

  // Multiple paths
  {
    vector<string> filenames;
    filenames.push_back(FileUtil::JoinPath(FLAGS_test_tmpdir, "these_files"));
    filenames.push_back(FileUtil::JoinPath(FLAGS_test_tmpdir, "do_not"));
    filenames.push_back(FileUtil::JoinPath(FLAGS_test_tmpdir, "exists"));

    string joined_path;
    Util::JoinStrings(filenames, ",", &joined_path);
    InputMultiFile multfile(joined_path);
    string line;
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
    EXPECT_FALSE(multfile.ReadLine(&line));
  }
}


TEST(InputMultiFileTest, ReadSingleFileTest) {
  EXPECT_TRUE(FileUtil::DirectoryExists(FLAGS_test_tmpdir));
  const string path = FileUtil::JoinPath(FLAGS_test_tmpdir, "i_am_a_test_file");

  // Create a test file
  vector<string> expected_lines;
  const int kNumLines = 10;
  {
    OutputFileStream ofs(path.c_str());
    for (int i = 0; i < kNumLines; ++i) {
      string line = Util::StringPrintf("Hi, line %d", i);
      expected_lines.push_back(line);
      ofs << line << endl;;
    }
  }
  EXPECT_EQ(kNumLines, expected_lines.size());

  // Read lines
  InputMultiFile multfile(path);
  string line;
  for (int i = 0; i < kNumLines; ++i) {
    EXPECT_TRUE(multfile.ReadLine(&line));
    EXPECT_EQ(expected_lines[i], line);
  }
  EXPECT_FALSE(multfile.ReadLine(&line));  // Check if no more lines remain
  EXPECT_FALSE(multfile.ReadLine(&line));
  EXPECT_FALSE(multfile.ReadLine(&line));
}


TEST(InputMultiFileTest, ReadMultipleFilesTest) {
  EXPECT_TRUE(FileUtil::DirectoryExists(FLAGS_test_tmpdir));

  const int kNumFile = 3;
  const int kNumLinesPerFile = 10;

  // Create test files
  vector<string> paths;
  vector<string> expected_lines;
  {
    int serial_line_no = 0;
    for (int fileno = 0; fileno < kNumFile; ++fileno) {
      string filename = Util::StringPrintf("testfile%d", fileno);
      string path = FileUtil::JoinPath(FLAGS_test_tmpdir, filename);
      paths.push_back(path);

      OutputFileStream ofs(path.c_str());
      for (int i = 0; i < kNumLinesPerFile; ++i) {
        string line = Util::StringPrintf("Hi, line %d", ++serial_line_no);
        expected_lines.push_back(line);
        ofs << line << endl;;
      }
    }
  }
  EXPECT_EQ(kNumLinesPerFile * kNumFile, expected_lines.size());

  // Read lines
  string joined_path;
  Util::JoinStrings(paths, ",", &joined_path);
  InputMultiFile multfile(joined_path);
  string line;
  for (int i = 0; i < kNumFile * kNumLinesPerFile; ++i) {
    EXPECT_TRUE(multfile.ReadLine(&line));
    EXPECT_EQ(expected_lines[i], line);
  }
  EXPECT_FALSE(multfile.ReadLine(&line));  // Check if no more lines remain
  EXPECT_FALSE(multfile.ReadLine(&line));
  EXPECT_FALSE(multfile.ReadLine(&line));
}

}  // namespace mozc
