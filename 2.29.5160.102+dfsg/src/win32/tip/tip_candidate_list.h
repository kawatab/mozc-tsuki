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

#ifndef MOZC_WIN32_TIP_TIP_CANDIDATE_LIST_H_
#define MOZC_WIN32_TIP_TIP_CANDIDATE_LIST_H_

#include <ctffunc.h>
#include <guiddef.h>
#include <wil/com.h>

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mozc {
namespace win32 {
namespace tsf {

class TipCandidateListCallback {
 public:
  virtual ~TipCandidateListCallback() = default;
  virtual void OnFinalize(size_t index, const std::wstring &candidate) = 0;
};

class TipCandidateList {
 public:
  TipCandidateList() = delete;
  TipCandidateList(const TipCandidateList &) = delete;
  TipCandidateList &operator=(const TipCandidateList &) = delete;

  // Returns an object that implements ITfFnSearchCandidateProvider.
  // |callback| will be called back when ITfCandidateList::SetResult
  // is called with CAND_FINALIZED. TipCandidateList will take the
  // ownership of |callback|. |callback| can be nullptr.
  static wil::com_ptr_nothrow<ITfCandidateList> New(
      std::vector<std::wstring> candidates,
      std::unique_ptr<TipCandidateListCallback> callback);
  static const IID &GetIID();
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_CANDIDATE_LIST_H_
