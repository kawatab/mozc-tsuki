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

#ifndef MOZC_RENDERER_WIN32_COMPOSITION_WINDOW_H_
#define MOZC_RENDERER_WIN32_COMPOSITION_WINDOW_H_

#include <vector>

#include "base/coordinates.h"
#include "base/port.h"
#include "renderer/win32/win32_renderer_util.h"

namespace mozc {

namespace renderer {
namespace win32 {

class CompositionWindowList {
 public:
  CompositionWindowList() {}
  virtual ~CompositionWindowList() {}

  virtual void Initialize() = 0;
  virtual void AsyncHide() = 0;
  virtual void AsyncQuit() = 0;
  virtual void Destroy() = 0;
  virtual void Hide() = 0;
  virtual void UpdateLayout(
      const vector<CompositionWindowLayout> &layouts) = 0;

  static CompositionWindowList *CreateInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(CompositionWindowList);
};
}  // namespace win32
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_WIN32_COMPOSITION_WINDOW_H_
