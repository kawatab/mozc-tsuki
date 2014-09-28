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

#import "mac/GoogleJapaneseInputServer.h"

#import "mac/GoogleJapaneseInputController.h"

#include <string>

#include "base/const.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "session/commands.pb.h"

GoogleJapaneseInputServer *g_imkServer = nil;

namespace {
void InitializeServer() {
  NSBundle *bundle = [NSBundle mainBundle];
  NSDictionary *infoDictionary = [bundle infoDictionary];
  NSString *connectionName =
      [infoDictionary objectForKey:@"InputMethodConnectionName"];
  if (connectionName == nil ||
      ![connectionName isKindOfClass:[NSString class]]) {
    LOG(ERROR) << "InputMethodConnectionName is not found or incorrect. "
               << "Possibly Info.plist is broken.";
    return;
  }

  g_imkServer = [[[GoogleJapaneseInputServer alloc]
                   initWithName:connectionName
               bundleIdentifier:[bundle bundleIdentifier]]
                  autorelease];
  [g_imkServer registerRendererConnection];
}
mozc::once_t gOnceForServer = MOZC_ONCE_INIT;
}

@implementation GoogleJapaneseInputServer
- (void)dealloc {
  [renderer_conection_ release];
  [super dealloc];
}

- (BOOL)registerRendererConnection {
  NSString *connectionName = @ kProductPrefix "_Renderer_Connection";
  renderer_conection_ = [[NSConnection alloc] init];
  [renderer_conection_ setRootObject:g_imkServer];
  return [renderer_conection_ registerName:connectionName];
}

- (void)sendData:(NSData *)data {
  if (current_controller_ == nil) {
    return;
  }

  mozc::commands::SessionCommand command;
  if (!command.ParseFromArray([data bytes], [data length])) {
    return;
  }

  [current_controller_ sendCommand:command];
}

- (void)outputResult:(NSData *)data {
  mozc::commands::Output output;
  if (!output.ParseFromArray([data bytes], [data length])) {
    return;
  }

  [current_controller_ outputResult:&output];
}

- (void)setCurrentController:(id<ControllerCallback>)controller {
  current_controller_ = controller;
}

+ (GoogleJapaneseInputServer *)getServer {
  mozc::CallOnce(&gOnceForServer, InitializeServer);
  return g_imkServer;
}
@end
