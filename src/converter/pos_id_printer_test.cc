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

#include "converter/pos_id_printer.h"

#include <memory>
#include <string>

#include "base/file_stream.h"
#include "base/file_util.h"
#include "testing/base/public/gunit.h"
#include "testing/base/public/mozctest.h"
#include "absl/flags/flag.h"

namespace mozc {
namespace internal {

class PosIdPrinterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const std::string test_id_def_path =
        testing::GetSourceFileOrDie({"data", "test", "dictionary", "id.def"});
    pos_id_ = std::make_unique<InputFileStream>(test_id_def_path.c_str());
    pos_id_printer_ = std::make_unique<PosIdPrinter>(pos_id_.get());
  }

  std::unique_ptr<InputFileStream> pos_id_;
  std::unique_ptr<PosIdPrinter> pos_id_printer_;
};

TEST_F(PosIdPrinterTest, BasicIdTest) {
  EXPECT_EQ("名詞,サ変接続,*,*,*,*,*", pos_id_printer_->IdToString(1934));
  EXPECT_EQ("名詞,サ変接続,*,*,*,*,*,使用", pos_id_printer_->IdToString(1935));
  EXPECT_EQ("BOS/EOS,*,*,*,*,*,*", pos_id_printer_->IdToString(0));
}

TEST_F(PosIdPrinterTest, InvalidId) {
  EXPECT_EQ("", pos_id_printer_->IdToString(-1));
}

TEST_F(PosIdPrinterTest, NullInput) {
  PosIdPrinter pos_id_printer(nullptr);
  EXPECT_EQ("", pos_id_printer.IdToString(-1));
  EXPECT_EQ("", pos_id_printer.IdToString(1934));
}

}  // namespace internal
}  // namespace mozc
