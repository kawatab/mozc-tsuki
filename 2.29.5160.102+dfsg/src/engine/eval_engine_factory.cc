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

#include "engine/eval_engine_factory.h"

#include <memory>
#include <string>
#include <utility>

#include "data_manager/data_manager.h"
#include "engine/engine.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"

namespace mozc {

absl::StatusOr<std::unique_ptr<Engine>> CreateEvalEngine(
    absl::string_view data_file_path, absl::string_view data_type,
    absl::string_view engine_type) {
  const absl::string_view magic_number =
      DataManager::GetDataSetMagicNumber(data_type);
  absl::StatusOr<std::unique_ptr<DataManager>> data_manager =
      DataManager::CreateFromFile(std::string(data_file_path), magic_number);
  if (!data_manager.ok()) {
    return std::move(data_manager).status();
  }
  if (engine_type == "desktop") {
    return Engine::CreateDesktopEngine(*std::move(data_manager));
  }
  if (engine_type == "mobile") {
    return Engine::CreateMobileEngine(*std::move(data_manager));
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid engine type: ", engine_type));
}

}  // namespace mozc
