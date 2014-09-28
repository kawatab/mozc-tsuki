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

#ifndef MOZC_RENDERER_UNIX_DRAW_TOOL_MOCK_H_
#define MOZC_RENDERER_UNIX_DRAW_TOOL_MOCK_H_

#include "renderer/unix/draw_tool_interface.h"
#include "testing/base/public/gmock.h"

namespace mozc {
namespace renderer {
namespace gtk {

class DrawToolMock : public DrawToolInterface {
 public:
  MOCK_METHOD1(Reset, void((CairoWrapperInterface *cairo)));
  MOCK_METHOD0(Save, void());
  MOCK_METHOD0(Restore, void());
  MOCK_METHOD2(FillRect, void(const Rect &rect, const RGBA &color));
  MOCK_METHOD3(FrameRect, void(const Rect &rect, const RGBA &color,
                               const uint32 line_width));
  MOCK_METHOD4(DrawLine, void(const Point &from, const Point &to,
                              const RGBA &color, const uint32 line_width));
};

}  // namespace gtk
}  // namespace renderer
}  // namespace mozc
#endif  // MOZC_RENDERER_UNIX_DRAW_TOOL_MOCK_H_
