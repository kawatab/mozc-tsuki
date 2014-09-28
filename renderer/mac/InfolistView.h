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

#ifndef MOZC_RENDERER_MAC_INFOLIST_VIEW_H_
#define MOZC_RENDERER_MAC_INFOLIST_VIEW_H_

#import <Cocoa/Cocoa.h>
#include "renderer/renderer_command.pb.h"

namespace mozc {
namespace renderer {
class RendererStyle;
}  // namespace mozc::renderer
}  // namespace mozc

// InfolistView is an NSView subclass to draw the infolist window
// according to the current candidates.
@interface InfolistView : NSView {
@private
  mozc::commands::Candidates candidates_;
  const mozc::renderer::RendererStyle *style_;
  // The row which has focused background.
  int focusedRow_;
}

// setCandidates: sets the candidates to be rendered.
- (void)setCandidates:(const mozc::commands::Candidates *)candidates;

// Checks the |candidates_| and recalculates the layout.
// It also returns the size which is necessary to draw all GUI elements.
- (NSSize)updateLayout;
@end

#endif  // MOZC_RENDERER_MAC_INFOLIST_VIEW_H_
