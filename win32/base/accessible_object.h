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

#ifndef MOZC_WIN32_BASE_ACCESSIBLE_OBJECT_H_
#define MOZC_WIN32_BASE_ACCESSIBLE_OBJECT_H_

#include <windows.h>

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>

#include <vector>

#include "base/port.h"
#include "win32/base/accessible_object_info.h"

namespace mozc {
namespace win32 {

class AccessibleObject {
 public:
  AccessibleObject();
  explicit AccessibleObject(ATL::CComPtr<IAccessible> container);
  AccessibleObject(ATL::CComPtr<IAccessible> container, int32 child_id);

  AccessibleObjectInfo GetInfo() const;
  vector<AccessibleObject> GetChildren() const;
  AccessibleObject GetParent() const;
  AccessibleObject GetFocus() const;
  bool GetWindowHandle(HWND *window_handle) const;
  bool AccessibleObject::GetProcessId(DWORD *process_id) const;
  bool IsValid() const;

  static AccessibleObject FromWindow(HWND window_handle);

 private:
  ATL::CComPtr<IAccessible> container_;
  int32 child_id_;
  bool valid_;
};

}  // namespace win32
}  // namespace mozc

#endif  // MOZC_WIN32_BASE_ACCESSIBLE_OBJECT_H_
