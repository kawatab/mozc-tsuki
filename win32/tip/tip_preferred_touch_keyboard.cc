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

#include "win32/tip/tip_preferred_touch_keyboard.h"

#include <Windows.h>
#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <Ctffunc.h>

#include "win32/tip/tip_ref_count.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ATL::CComPtr;

// SPI_GETTHREADLOCALINPUTSETTINGS is available on Windows 8 SDK and later.
#ifndef SPI_SETTHREADLOCALINPUTSETTINGS
#define SPI_SETTHREADLOCALINPUTSETTINGS 0x104F
#endif  // SPI_SETTHREADLOCALINPUTSETTINGS

// ITfFnGetPreferredTouchKeyboardLayout is available on Windows 8 SDK and later.
#ifndef TKBL_UNDEFINED
#define TKBL_UNDEFINED                             0x0000
#define TKBL_CLASSIC_TRADITIONAL_CHINESE_PHONETIC  0x0404
#define TKBL_CLASSIC_TRADITIONAL_CHINESE_CHANGJIE  0xF042
#define TKBL_CLASSIC_TRADITIONAL_CHINESE_DAYI      0xF043
#define TKBL_OPT_JAPANESE_ABC                      0x0411
#define TKBL_OPT_KOREAN_HANGUL_2_BULSIK            0x0412
#define TKBL_OPT_SIMPLIFIED_CHINESE_PINYIN         0x0804
#define TKBL_OPT_TRADITIONAL_CHINESE_PHONETIC      0x0404

enum TKBLayoutType {
  TKBLT_UNDEFINED = 0,
  TKBLT_CLASSIC = 1,
  TKBLT_OPTIMIZED = 2
};

// {5F309A41-590A-4ACC-A97F-D8EFFF13FDFC}
const IID IID_ITfFnGetPreferredTouchKeyboardLayout = {
  0x5f309a41, 0x590a, 0x4acc, {0xa9, 0x7f, 0xd8, 0xef, 0xff, 0x13, 0xfd, 0xfc}
};

// Note: "5F309A41-590A-4ACC-A97F-D8EFFF13FDFC" is equivalent to
// IID_ITfFnGetPreferredTouchKeyboardLayout
struct __declspec(uuid("5F309A41-590A-4ACC-A97F-D8EFFF13FDFC"))
ITfFnGetPreferredTouchKeyboardLayout : public ITfFunction {
 public:
  virtual HRESULT STDMETHODCALLTYPE GetLayout(TKBLayoutType *layout_type,
                                              WORD *preferred_layout_id) = 0;
};
#endif  // !TKBL_UNDEFINED

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
const wchar_t kGetPreferredTouchKeyboardLayoutDisplayName[] =
    L"Google Japanese Input: GetPreferredTouchKeyboardLayout Function";
#else
const wchar_t kGetPreferredTouchKeyboardLayoutDisplayName[] =
    L"Mozc: GetPreferredTouchKeyboardLayout Function";
#endif

class GetPreferredTouchKeyboardLayoutImpl
    : public ITfFnGetPreferredTouchKeyboardLayout {
 public:
  GetPreferredTouchKeyboardLayoutImpl() {
  }

  // The IUnknown interface methods.
  virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interface_id,
                                                   void **object) {
    if (!object) {
      return E_INVALIDARG;
    }

    // Find a matching interface from the ones implemented by this object.
    // This object implements IUnknown and ITfEditSession.
    if (::IsEqualIID(interface_id, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
    } else if (IsEqualIID(interface_id, IID_ITfFunction)) {
      *object = static_cast<ITfFunction *>(this);
    } else if (IsEqualIID(interface_id,
                          IID_ITfFnGetPreferredTouchKeyboardLayout)) {
      *object = static_cast<ITfFnGetPreferredTouchKeyboardLayout *>(this);
    } else {
      *object = nullptr;
      return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
  }

  virtual ULONG STDMETHODCALLTYPE AddRef() {
    return ref_count_.AddRefImpl();
  }

  virtual ULONG STDMETHODCALLTYPE Release() {
    const ULONG count = ref_count_.ReleaseImpl();
    if (count == 0) {
      delete this;
    }
    return count;
  }

 private:
  // The ITfFunction interface method.
  virtual HRESULT STDMETHODCALLTYPE GetDisplayName(BSTR *name) {
    if (name == nullptr) {
      return E_INVALIDARG;
    }
    *name = ::SysAllocString(kGetPreferredTouchKeyboardLayoutDisplayName);
    return S_OK;
  }

  // ITfFnGetPreferredTouchKeyboardLayout
  virtual HRESULT STDMETHODCALLTYPE GetLayout(TKBLayoutType *layout_type,
                                              WORD *preferred_layout_id) {
    if (layout_type != nullptr) {
      *layout_type = TKBLT_OPTIMIZED;
    }
    if (preferred_layout_id != nullptr) {
      *preferred_layout_id = TKBL_OPT_JAPANESE_ABC;
    }
    return S_OK;
  }

  TipRefCount ref_count_;

  DISALLOW_COPY_AND_ASSIGN(GetPreferredTouchKeyboardLayoutImpl);
};

}  // namespace

// static
IUnknown *TipPreferredTouchKeyboard::New() {
  return new GetPreferredTouchKeyboardLayoutImpl();
}

// static
const IID &TipPreferredTouchKeyboard::GetIID() {
  return IID_ITfFnGetPreferredTouchKeyboardLayout;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
