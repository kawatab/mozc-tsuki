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

#ifndef MOZC_RENDERER_RENDERER_STYLE_HANDLER_H_
#define MOZC_RENDERER_RENDERER_STYLE_HANDLER_H_

#include <string>

namespace mozc {
namespace renderer {
class RendererStyle;

// this is pure static class
class RendererStyleHandler{
 public:
  // return current Style
  static bool GetRendererStyle(RendererStyle *style);
  // set Style
  static bool SetRendererStyle(const RendererStyle &style);
  // get default Style
  static void GetDefaultRendererStyle(RendererStyle *style);

  // Returns DPI scaling factor on Windows.
  // On other platforms, always returns 1.0.
  static void GetDPIScalingFactor(double *factor_x, double *factor_y);

  // Do not allow instantiation
 private:
  RendererStyleHandler() {}
  virtual ~RendererStyleHandler() {}
};

} // renderer
} // mozc

#endif // MOZC_RENDERER_RENDERER_STYLE_HANDLER_H_
