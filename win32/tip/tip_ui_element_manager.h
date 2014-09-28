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

#ifndef MOZC_WIN32_TIP_TIP_UI_ELEMENT_MANAGER_H_
#define MOZC_WIN32_TIP_TIP_UI_ELEMENT_MANAGER_H_

#include <Windows.h>
#include <msctf.h>

#include <memory>

#include "base/port.h"

namespace mozc {
namespace win32 {
namespace tsf {

class TipTextService;

class TipUiElementManager {
 public:
  enum UIElementFlags {
    kNoneWindow = 0,
    kSuggestWindow = 1 << 0,
    kCandidateWindow = 1 << 1,
    kIndicatorWindow = 1 << 2,
  };
  TipUiElementManager();
  ~TipUiElementManager();

  ITfUIElement *GetElement(UIElementFlags element) const;
  DWORD GetElementId(UIElementFlags element) const;
  HRESULT OnUpdate(TipTextService *text_service, ITfContext *context);
  bool IsVisible(ITfUIElementMgr *ui_element_manager,
                 UIElementFlags element) const;

 private:
  class UiElementMap;
  std::unique_ptr<UiElementMap> ui_element_map_;

  DISALLOW_COPY_AND_ASSIGN(TipUiElementManager);
};

}  // namespace tsf
}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_TIP_TIP_UI_ELEMENT_MANAGER_H_
