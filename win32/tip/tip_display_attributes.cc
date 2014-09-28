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

#include "win32/tip/tip_display_attributes.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

const wchar_t kInputDescription[] =
    L"TextService Display Attribute Input";
const TF_DISPLAYATTRIBUTE kInputAttribute = {
  { TF_CT_NONE, 0 },        // text color
  { TF_CT_NONE, 0 },        // background color
  TF_LS_DOT,                // underline style
  FALSE,                    // underline boldness
  { TF_CT_NONE, 0 },        // underline color
  TF_ATTR_INPUT             // attribute info
};

const wchar_t kConvertedDescription[] =
    L"TextService Display Attribute Converted";
const TF_DISPLAYATTRIBUTE kConvertedAttribute = {
  { TF_CT_NONE, 0 },        // text color
  { TF_CT_NONE, 0 },        // background color
  TF_LS_SOLID,              // underline style
  TRUE,                     // underline boldness
  { TF_CT_NONE, 0 },        // underline color
  TF_ATTR_TARGET_CONVERTED  // attribute info
};

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

// {DDF5CDBA-C3FF-4BAF-B817-CC9210FAD27E}
const GUID kDisplayAttributeInput = {
  0xddf5cdba, 0xc3ff, 0x4baf, {0xb8, 0x17, 0xcc, 0x92, 0x10, 0xfa, 0xd2, 0x7e}
};

// {F829C8C0-0EBB-4D29-BD2F-E413A944B7E4}
const GUID kDisplayAttributeConverted = {
  0xf829c8c0, 0x0ebb, 0x4d29, {0xbd, 0x2f, 0xe4, 0x13, 0xa9, 0x44, 0xb7, 0xe4}
};

#else

// {84CA1E7E-3020-4D1C-8968-DDA372D1E067}
const GUID kDisplayAttributeInput = {
  0x84ca1e7e, 0x3020, 0x4d1c, {0x89, 0x68, 0xdd, 0xa3, 0x72, 0xd1, 0xe0, 0x67}
};

// {8A4028E5-2DCD-4365-A5DC-71F67E797437}
const GUID kDisplayAttributeConverted = {
  0x8a4028e5, 0x2dcd, 0x4365, {0xa5, 0xdc, 0x71, 0xf6, 0x7e, 0x79, 0x74, 0x37}
};

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

}  // namespace

TipDisplayAttribute::TipDisplayAttribute(const GUID &guid,
                                         const TF_DISPLAYATTRIBUTE &attribute,
                                         const wstring &description)
    : guid_(guid),
      description_(description) {
  ::CopyMemory(&original_attribute_, &attribute, sizeof(original_attribute_));
  ::CopyMemory(&attribute_, &attribute, sizeof(attribute_));
}

TipDisplayAttribute::~TipDisplayAttribute() {}

STDAPI TipDisplayAttribute::QueryInterface(REFIID interface_id,
                                           void **object) {
  if (object == nullptr) {
    return E_INVALIDARG;
  }

  // Find a matching interface from the ones implemented by this object.
  if (::IsEqualIID(interface_id, IID_IUnknown)) {
    *object = static_cast<IUnknown *>(this);
    AddRef();
    return S_OK;
  } else if (::IsEqualIID(interface_id, IID_ITfDisplayAttributeInfo)) {
    *object = static_cast<ITfDisplayAttributeInfo *>(this);
    AddRef();
    return S_OK;
  }

  *object = nullptr;
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE TipDisplayAttribute::AddRef() {
  return ref_count_.AddRefImpl();
}

ULONG STDMETHODCALLTYPE TipDisplayAttribute::Release() {
  const ULONG count = ref_count_.ReleaseImpl();
  if (count == 0) {
    delete this;
  }
  return count;
}

HRESULT STDMETHODCALLTYPE TipDisplayAttribute::GetGUID(GUID *guid) {
  if (guid == nullptr) {
    return E_INVALIDARG;
  }
  *guid = guid_;
  return S_OK;
}

HRESULT STDMETHODCALLTYPE TipDisplayAttribute::GetDescription(
    BSTR* description) {
  if (description == nullptr) {
    return E_INVALIDARG;
  }

  *description = ::SysAllocString(description_.c_str());
  return (*description != nullptr) ? S_OK : E_OUTOFMEMORY;
}

HRESULT STDMETHODCALLTYPE TipDisplayAttribute::GetAttributeInfo(
  TF_DISPLAYATTRIBUTE *attribute) {
  if (attribute == nullptr) {
    return E_INVALIDARG;
  }
  ::CopyMemory(attribute, &attribute_, sizeof(attribute_));
  return S_OK;
}

HRESULT STDMETHODCALLTYPE TipDisplayAttribute::SetAttributeInfo(
    const TF_DISPLAYATTRIBUTE* attribute) {
  if (attribute == nullptr) {
    return E_INVALIDARG;
  }
  ::CopyMemory(&attribute_, attribute, sizeof(attribute_));

  return S_OK;
}

HRESULT STDMETHODCALLTYPE TipDisplayAttribute::Reset() {
  return SetAttributeInfo(&original_attribute_);
}

TipDisplayAttributeInput::TipDisplayAttributeInput()
    : TipDisplayAttribute(kDisplayAttributeInput,
                          kInputAttribute,
                          kInputDescription) {
}

const GUID &TipDisplayAttributeInput::guid() {
  return kDisplayAttributeInput;
}

TipDisplayAttributeConverted::TipDisplayAttributeConverted()
    : TipDisplayAttribute(kDisplayAttributeConverted,
                          kConvertedAttribute,
                          kConvertedDescription) {
}

const GUID &TipDisplayAttributeConverted::guid() {
  return kDisplayAttributeConverted;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
