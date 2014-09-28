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

// Main purpose of this is to see behaviour of system dictionary builder
// like speed or memory cosumption.

#include "dictionary/system/system_dictionary_builder.h"

#include <map>
#include <string>
#include <vector>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/flags.h"
#include "base/logging.h"
#include "data_manager/user_pos_manager.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/text_dictionary_loader.h"
#include "testing/base/public/googletest.h"
#include "testing/base/public/gunit.h"

DEFINE_string(input,
              "data/dictionary/dictionary00.txt",
              "input file");
DEFINE_int32(dictionary_test_size, 10000,
             "Dictionary size for this test");

namespace mozc {
namespace dictionary {


class SystemDictionaryBuilderTest : public testing::Test {
 protected:
  SystemDictionaryBuilderTest() {
  }
};

TEST_F(SystemDictionaryBuilderTest, test) {
  // This test is only testing that system dicionary does not make any errors.
  // Dictionary itself will be tested at sytem_dicitonary_test.
  const POSMatcher *pos_matcher =
      UserPosManager::GetUserPosManager()->GetPOSMatcher();
  TextDictionaryLoader loader(*pos_matcher);
  const string dic_path = FileUtil::JoinPath(FLAGS_test_srcdir, FLAGS_input);
  LOG(INFO) << "Reading " << dic_path;
  // TODO(noriyukit): Add test case for reading correction file.
  loader.LoadWithLineLimit(dic_path, "", FLAGS_dictionary_test_size);
  const vector<Token *> &tokens = loader.tokens();
  LOG(INFO) << "Read " << tokens.size() << "tokens";
  SystemDictionaryBuilder builder;
  builder.BuildFromTokens(tokens);
}

}  // namespace dictionary
}  // namespace mozc
