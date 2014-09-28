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

#include "win32/tip/tip_class_factory.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>

#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ATL::CComPtr;

TipClassFactory::TipClassFactory() {}

HRESULT STDMETHODCALLTYPE TipClassFactory::QueryInterface(
  REFIID interface_id, void **object) {
  if (object == nullptr) {
    return E_INVALIDARG;
  }
  if (::IsEqualIID(interface_id, IID_IUnknown)) {
    *object = static_cast<IUnknown *>(this);
  } else if (::IsEqualIID(interface_id, IID_IClassFactory)) {
    *object = static_cast<IClassFactory *>(this);
  } else {
    *object = nullptr;
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG STDMETHODCALLTYPE TipClassFactory::AddRef() {
  return ref_count_.AddRefImpl();
}

ULONG STDMETHODCALLTYPE TipClassFactory::Release() {
  const ULONG count = ref_count_.ReleaseImpl();
  if (count == 0) {
    delete this;
  }
  return count;
}

HRESULT STDMETHODCALLTYPE TipClassFactory::CreateInstance(
    IUnknown *unknown, REFIID interface_id, void **object) {
  HRESULT result = S_OK;

  if (object == nullptr) {
    return E_INVALIDARG;
  }
  *object = nullptr;
  if (unknown != nullptr) {
    return CLASS_E_NOAGGREGATION;
  }

  // Create an TipTextService object and initialize it.
  CComPtr<TipTextService> text_service(TipTextServiceFactory::Create());

  // Retrieve the requested interface from the TipTextService object.
  // If this TipTextService object implements the given interface, the
  // TipTextService::QueryInterface() function increments its reference count
  // and copies its interface pointer to the |object|.
  // On the other hand, this object does not implement the given interface,
  // the TipTextService::Release() function deletes this object.
  return text_service->QueryInterface(interface_id, object);
}

HRESULT STDMETHODCALLTYPE TipClassFactory::LockServer(BOOL lock) {
  if (lock) {
    TipDllModule::AddRef();
  } else {
    TipDllModule::Release();
  }
  return S_OK;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
