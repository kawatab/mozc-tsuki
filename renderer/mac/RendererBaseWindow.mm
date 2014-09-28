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

#include <Carbon/Carbon.h>
#include <Cocoa/Cocoa.h>
#include <objc/message.h>

#include "base/coordinates.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/port.h"
#include "session/commands.pb.h"
#include "renderer/mac/RendererBaseWindow.h"


namespace mozc {
namespace renderer{
namespace mac {

RendererBaseWindow::RendererBaseWindow()
    : window_level_(NSPopUpMenuWindowLevel) {
}

void RendererBaseWindow::InitWindow() {
  if (window_.get()) {
    LOG(ERROR) << "window is already initialized.";
    return;
  }
  const NSUInteger style_mask =
      NSUtilityWindowMask | NSDocModalWindowMask | NSNonactivatingPanelMask;
  window_.reset([[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 1, 1)
                                           styleMask:style_mask
                                             backing:NSBackingStoreBuffered
                                               defer:YES]);
  ResetView();
  [window_.get() setContentView:view_.get()];
  [window_.get() setDisplaysWhenScreenProfileChanges:YES];
  [window_.get() makeKeyAndOrderFront:nil];
  [window_.get() setFloatingPanel:YES];
  [window_.get() setWorksWhenModal:YES];
  [window_.get() setBackgroundColor:NSColor.whiteColor];
  [window_.get() setReleasedWhenClosed:NO];
  [window_.get() setLevel:window_level_];
  [window_.get() orderOut:window_.get()];
}

RendererBaseWindow::~RendererBaseWindow() {
  [window_.get() close];
  window_.reset();
}

Size RendererBaseWindow::GetWindowSize() const {
  if (!window_.get()) {
    return Size(0, 0);
  }
  NSRect rect = [window_.get() frame];
  return Size(rect.size.width, rect.size.height);
}

void RendererBaseWindow::Hide() {
  if (window_.get()) {
    [window_.get() orderOut:window_.get()];
  }
}

void RendererBaseWindow::Show() {
  if (!window_.get()) {
    InitWindow();
  }
  [window_.get() orderFront:window_.get()];
}

bool RendererBaseWindow::IsVisible() {
  if (!window_) {
    return false;
  }
  return ([window_.get() isVisible] == YES);
}

void RendererBaseWindow::ResetView() {
  view_.reset([[NSView alloc] initWithFrame:NSMakeRect(0, 0, 1, 1)]);
}

void RendererBaseWindow::MoveWindow(const NSPoint &point) {
  NSRect rect = [window_.get() frame];
  rect.origin.x = point.x;
  rect.origin.y = point.y;
  [window_.get() setFrame:rect display:FALSE];
}

void RendererBaseWindow::ResizeWindow(int32 width, int32 height) {
  NSRect rect = [window_.get() frame];
  rect.size.width = width;
  rect.size.height = height;
  [window_.get() setFrame:rect display:FALSE];
}

void RendererBaseWindow::SetWindowLevel(NSInteger window_level) {
  if (window_level_ != window_level) {
    window_level_ = window_level;
    [window_.get() setLevel:window_level_];
  }
}

}  // namespace mozc::renderer::mac
}  // namespace mozc::renderer
}  // namespace mozc
