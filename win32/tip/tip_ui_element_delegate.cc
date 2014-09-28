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

#include "win32/tip/tip_ui_element_delegate.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <atlstr.h>
#include <msctf.h>

#include <string>

#include "base/util.h"
#include "renderer/renderer_command.pb.h"
#include "session/commands.pb.h"
#include "win32/base/conversion_mode_util.h"
#include "win32/base/input_state.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_edit_session.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_resource.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ATL::CComBSTR;
using ATL::CComPtr;
using ATL::CComQIPtr;
using ATL::CComVariant;
using ATL::CStringW;
using mozc::commands::CompositionMode;
using ::mozc::commands::CandidateList;
using ::mozc::commands::Output;
using ::mozc::commands::Status;
typedef mozc::commands::RendererCommand_IndicatorInfo IndicatorInfo;

const size_t kPageSize = 9;

// This GUID is used in Windows Vista/7/8 by MS-IME to represents if the
// candidate window is visible or not.
// TODO(yukawa): Make sure if it is safe to use this GUID.
// {B7A578D2-9332-438A-A403-4057D05C3958}
const GUID kGuidCUASCandidateMessageCompartment = {
  0xb7a578d2, 0x9332, 0x438a, {0xa4, 0x03, 0x40, 0x57, 0xd0, 0x5c, 0x39, 0x58}
};

#ifdef GOOGLE_JAPANESE_INPUT_BUILD

// {8F51B5E5-5CF9-45D8-83B3-53CE203354C2}
const GUID KGuidNonobservableSuggestWindow = {
  0x8f51b5e5, 0x5cf9, 0x45d8, {0x83, 0xb3, 0x53, 0xce, 0x20, 0x33, 0x54, 0xc2}
};

// {3D53878A-8596-4689-B50D-3338D52B2EFB}
const GUID KGuidObservableSuggestWindow = {
  0x3d53878a, 0x8596, 0x4689, {0xb5, 0xd, 0x33, 0x38, 0xd5, 0x2b, 0x2e, 0xfb}
};

// {FED897F2-940C-40F1-B149-A931E03FB821}
const GUID KGuidCandidateWindow = {
  0xfed897f2, 0x940c, 0x40f1, {0xb1, 0x49, 0xa9, 0x31, 0xe0, 0x3f, 0xb8, 0x21}
};

// {170F6CC4-913D-4FF9-9DEA-432D08DCB0FF}
const GUID KGuidIndicatorWindow = {
  0x170f6cc4, 0x913d, 0x4ff9, { 0x9d, 0xea, 0x43, 0x2d, 0x8, 0xdc, 0xb0, 0xff}
};

#else

// {AD2489FB-D4C4-4632-85A9-7F9F917AB0FD}
const GUID KGuidNonobservableSuggestWindow = {
  0xad2489fb, 0xd4c4, 0x4632, {0x85, 0xa9, 0x7f, 0x9f, 0x91, 0x7a, 0xb0, 0xfd}
};

// {0E2D447F-9B4A-490C-9C4D-61A6A707BE26}
const GUID KGuidObservableSuggestWindow = {
  0xe2d447f, 0x9b4a, 0x490c, {0x9c, 0x4d, 0x61, 0xa6, 0xa7, 0x7, 0xbe, 0x26}
};

// {ED70ECDE-C8AA-4170-96CC-0090DEA8AEC2}
const GUID KGuidCandidateWindow = {
  0xed70ecde, 0xc8aa, 0x4170, {0x96, 0xcc, 0x0, 0x90, 0xde, 0xa8, 0xae, 0xc2}
};

// {0090BF80-5F33-41B1-843C-E3EC79ED25F9}
const GUID KGuidIndicatorWindow = {
  0x90bf80, 0x5f33, 0x41b1, { 0x84, 0x3c, 0xe3, 0xec, 0x79, 0xed, 0x25, 0xf9}
};

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

CComBSTR GetResourceString(UINT resource_id) {
  CStringW str;
  str.LoadStringW(TipDllModule::module_handle(), resource_id);
  return CComBSTR(str.GetLength(), str.GetBuffer());
}

const bool kIsIndicator = true;
const bool kIsNotIndicator = false;

class TipUiElementDelegateImpl : public TipUiElementDelegate {
 public:
  TipUiElementDelegateImpl(
      TipTextService *text_service, ITfContext *context,
      TipUiElementDelegateFactory::ElementType type)
      : text_service_(text_service),
        context_(context),
        type_(type) {
  }

 private:
  ~TipUiElementDelegateImpl() {}

  virtual bool IsObservable() const {
    switch (type_) {
      case TipUiElementDelegateFactory::kConventionalObservableSuggestWindow:
      case TipUiElementDelegateFactory::kConventionalCandidateWindow:
        return true;
      default:
        return false;
    }
  }

  // The ITfUIElement interface methods
  virtual HRESULT GetDescription(BSTR *description) {
    if (description == nullptr) {
      return E_INVALIDARG;
    }
    *description = nullptr;
    switch (type_) {
      case TipUiElementDelegateFactory::kConventionalUnobservableSuggestWindow:
        *description =
            GetResourceString(IDS_UNOBSERVABLE_SUGGEST_WINDOW).Detach();
        return S_OK;
      case TipUiElementDelegateFactory::kConventionalObservableSuggestWindow:
        *description =
            GetResourceString(IDS_OBSERVABLE_SUGGEST_WINDOW).Detach();
        return S_OK;
      case TipUiElementDelegateFactory::kConventionalCandidateWindow:
        *description = GetResourceString(IDS_CANDIDATE_WINDOW).Detach();
        return S_OK;
      case TipUiElementDelegateFactory::kConventionalIndicatorWindow:
        *description = GetResourceString(IDS_INDICATOR_WINDOW).Detach();
        return S_OK;
      case TipUiElementDelegateFactory::kImmersiveCandidateWindow:
        *description = GetResourceString(IDS_CANDIDATE_WINDOW).Detach();
        return S_OK;
      case TipUiElementDelegateFactory::kImmersiveIndicatorWindow:
        *description = GetResourceString(IDS_INDICATOR_WINDOW).Detach();
        return S_OK;
      default:
        return E_UNEXPECTED;
    }
  }

  virtual HRESULT GetGUID(GUID *guid) {
    if (guid == nullptr) {
      return E_INVALIDARG;
    }
    switch (type_) {
      case TipUiElementDelegateFactory::kConventionalUnobservableSuggestWindow:
        *guid = KGuidNonobservableSuggestWindow;
        return S_OK;
      case TipUiElementDelegateFactory::kConventionalObservableSuggestWindow:
        *guid = KGuidObservableSuggestWindow;
        return S_OK;
      case TipUiElementDelegateFactory::kConventionalCandidateWindow:
        *guid = KGuidCandidateWindow;
        return S_OK;
      case TipUiElementDelegateFactory::kConventionalIndicatorWindow:
        *guid = KGuidIndicatorWindow;
        return S_OK;
      case TipUiElementDelegateFactory::kImmersiveCandidateWindow:
        *guid = KGuidCandidateWindow;
        return S_OK;
      case TipUiElementDelegateFactory::kImmersiveIndicatorWindow:
        *guid = KGuidIndicatorWindow;
        return S_OK;
      default:
        *guid = GUID_NULL;
        return E_UNEXPECTED;
    }
  }

  virtual HRESULT Show(BOOL show) {
    const bool old_shown = shown_;
    shown_ = !!show;
    if (old_shown != shown_ && IsObservable()) {
      CComQIPtr<ITfCompartmentMgr> compartment_mgr = context_;
      if (compartment_mgr) {
        // Update a hidden compartment to generate
        // IMN_OPENCANDIDATE/IMN_CLOSECANDIDATE notifications for the
        // application compatibility.
        CComQIPtr<ITfCompartment> compartment;
        if (SUCCEEDED(compartment_mgr->GetCompartment(
                kGuidCUASCandidateMessageCompartment, &compartment))) {
          CComVariant var;
          var.vt = VT_I4;
          var.lVal = shown_ ? TRUE : FALSE;
          compartment->SetValue(text_service_->GetClientID(), &var);
        }
      }
      // TODO(yukawa): Update UI.
    }
    return S_OK;
  }

  virtual HRESULT IsShown(BOOL *show) {
    if (show == nullptr) {
      return E_INVALIDARG;
    }
    *show = (shown_ ? TRUE : FALSE);
    return S_OK;
  }

  // The ITfCandidateListUIElement interface methods
  virtual HRESULT GetUpdatedFlags(DWORD *flags) {
    DCHECK(IsCandidateWindowLike());

    if (flags == nullptr) {
      return E_INVALIDARG;
    }
    *flags = 0;
    // If TF_CLUIE_STRING is included into |flags|, TSF calls back
    // ITfCandidateListUIElement::GetString for all the candidates,
    // which might be a huge bottleneck. So do not include this flag
    // whenever possible.
    if (TestModifiedAndUpdateLastCandidate()) {
      *flags |= (TF_CLUIE_STRING | TF_CLUIE_COUNT);
    }
    *flags |= (TF_CLUIE_SELECTION |
               TF_CLUIE_CURRENTPAGE |
               TF_CLUIE_PAGEINDEX);
    return S_OK;
  }

  virtual HRESULT GetDocumentMgr(ITfDocumentMgr **document_manager) {
    DCHECK(IsCandidateWindowLike());

    if (document_manager == nullptr) {
      return E_INVALIDARG;
    }
    return context_->GetDocumentMgr(document_manager);
  }

  virtual HRESULT GetCount(UINT *count) {
    DCHECK(IsCandidateWindowLike());

    if (count == nullptr) {
      return E_INVALIDARG;
    }
    *count = 0;
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      return E_FAIL;
    }
    const Output &output = private_context->last_output();
    if (!output.has_all_candidate_words()) {
      return S_OK;
    }
    *count = output.all_candidate_words().candidates_size();
    return S_OK;
  }

  virtual HRESULT GetSelection(UINT *index) {
    DCHECK(IsCandidateWindowLike());

    if (index == nullptr) {
      return E_INVALIDARG;
    }
    *index = 0;
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      return E_FAIL;
    }
    const Output &output = private_context->last_output();
    if (!output.has_all_candidate_words()) {
      return S_OK;
    }
    *index = output.all_candidate_words().focused_index();
    return S_OK;
  }

  virtual HRESULT GetString(UINT index, BSTR *text) {
    DCHECK(IsCandidateWindowLike());

    if (text == nullptr) {
      return E_INVALIDARG;
    }
    *text = nullptr;
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      return E_FAIL;
    }
    const Output &output = private_context->last_output();
    if (!output.has_all_candidate_words()) {
      return E_FAIL;
    }
    const CandidateList &list = output.all_candidate_words();
    // Convert |index| to the index within output_->candidates().
    const int visible_index = index;
    if (visible_index >= list.candidates_size()) {
      return E_FAIL;
    }
    wstring wide_text;
    Util::UTF8ToWide(list.candidates(visible_index).value(), &wide_text);
    *text = CComBSTR(wide_text.size(), wide_text.data()).Detach();
    return S_OK;
  }

  virtual HRESULT GetPageIndex(UINT *index, UINT size, UINT *page_count) {
    DCHECK(IsCandidateWindowLike());

    if (page_count == nullptr) {
      return E_INVALIDARG;
    }
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      return E_FAIL;
    }
    const Output &output = private_context->last_output();
    if (!output.has_all_candidate_words()) {
      return E_FAIL;
    }
    const CandidateList &list = output.all_candidate_words();
    const size_t max_page = (list.candidates_size() / kPageSize);
    *page_count = max_page + 1;

    if (index == nullptr) {
      // An application can pass nullptr as |index| to obtain only page_count.
      return S_OK;
    }

    if (size < *page_count) {
      return E_NOT_SUFFICIENT_BUFFER;
    }
    for (size_t i = 0; i < *page_count; ++i) {
      index[i] = i * kPageSize;
    }
    return S_OK;
  }

  virtual HRESULT SetPageIndex(UINT *index, UINT page_count) {
    DCHECK(IsCandidateWindowLike());

    return E_NOTIMPL;
  }

  virtual HRESULT GetCurrentPage(UINT *current_page) {
    DCHECK(IsCandidateWindowLike());

    if (current_page == nullptr) {
      return E_INVALIDARG;
    }
    *current_page = 0;
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      return E_FAIL;
    }
    const Output &output = private_context->last_output();
    if (!output.has_all_candidate_words()) {
      return S_OK;
    }
    *current_page = output.all_candidate_words().focused_index() / kPageSize;
    return S_OK;
  }

  // The ITfCandidateListUIElementBehavior interface methods
  virtual HRESULT SetSelection(UINT index) {
    DCHECK(IsCandidateWindowLike());

    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      return E_FAIL;
    }
    const Output &output = private_context->last_output();
    if (!output.has_all_candidate_words()) {
      return E_FAIL;
    }
    const CandidateList &list = output.all_candidate_words();
    if (list.candidates_size() <= index) {
      return E_INVALIDARG;
    }
    const int id = list.candidates(index).id();
    if (!TipEditSession::SelectCandidateAsync(text_service_, context_, id)) {
      return E_FAIL;
    }
    return S_OK;
  }

  virtual HRESULT Finalize() {
    DCHECK(IsCandidateWindowLike());

    if (!TipEditSession::SubmitAsync(text_service_, context_)) {
      return E_FAIL;
    }
    return S_OK;
  }

  virtual HRESULT Abort() {
    DCHECK(IsCandidateWindowLike());

    // Currently equals to Finalize().
    if (!TipEditSession::SubmitAsync(text_service_, context_)) {
      return E_FAIL;
    }
    return S_OK;
  }

  virtual HRESULT GetString(BSTR *text) {
    DCHECK(IsIndicator());

    if (text == nullptr) {
      return E_INVALIDARG;
    }

    CStringW msg = L"";

    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      *text = CComBSTR(msg.GetLength(), msg.GetBuffer()).Detach();
      return S_OK;
    }
    if (!private_context->last_output().has_status()) {
      *text = CComBSTR(msg.GetLength(), msg.GetBuffer()).Detach();
      return S_OK;
    }
    const Status &status = private_context->last_output().status();
    if (status.has_activated() && !status.activated()) {
      msg = L"A";
      *text = CComBSTR(msg.GetLength(), msg.GetBuffer()).Detach();
      return S_OK;
    }
    if (!status.has_mode()) {
      *text = CComBSTR(msg.GetLength(), msg.GetBuffer()).Detach();
      return S_OK;
    }

    switch (status.mode()) {
      case commands::DIRECT:
        DLOG(FATAL) << "Must not reach here.";
        break;
      case commands::HIRAGANA:
        msg = L"\u3042";
        break;
      case commands::FULL_KATAKANA:
        msg = L"\u30AB";
        break;
      case commands::HALF_ASCII:
        msg = L"_A";
        break;
      case commands::FULL_ASCII:
        msg = L"\uFF21";
        break;
      case commands::HALF_KATAKANA:
        msg = L"_\uFF76";
        break;
      default:
        break;
    }

    *text = CComBSTR(msg.GetLength(), msg.GetBuffer()).Detach();
    return S_OK;
  }

  // Returns true if the candidate list is updated. When this function returns
  // false, you need not update the list of candidate strings at this time.
  // Note that this function updates |last_candidate_list_| internally.
  bool TestModifiedAndUpdateLastCandidate() {
    TipPrivateContext *private_context =
        text_service_->GetPrivateContext(context_);
    if (private_context == nullptr) {
      return true;
    }
    const Output &output = private_context->last_output();
    if (!output.has_all_candidate_words()) {
      return true;
    }
    const CandidateList &list = output.all_candidate_words();
    if (last_candidate_list_.candidates_size() != list.candidates_size()) {
      last_candidate_list_.CopyFrom(list);
      return true;
    }
    for (int i = 0; i < list.candidates_size(); ++i) {
      if (last_candidate_list_.candidates(i).value() !=
          list.candidates(i).value()) {
        last_candidate_list_.CopyFrom(list);
        return true;
      }
    }
    return false;
  }

  bool IsCandidateWindowLike() const {
    switch (type_) {
      case TipUiElementDelegateFactory::kConventionalUnobservableSuggestWindow:
        return true;
      case TipUiElementDelegateFactory::kConventionalObservableSuggestWindow:
        return true;
      case TipUiElementDelegateFactory::kConventionalCandidateWindow:
        return true;
      case TipUiElementDelegateFactory::kConventionalIndicatorWindow:
        return false;
      case TipUiElementDelegateFactory::kImmersiveCandidateWindow:
        return true;
      case TipUiElementDelegateFactory::kImmersiveIndicatorWindow:
        return false;
      default:
        return false;
    }
  }

  bool IsIndicator() const {
    switch (type_) {
      case TipUiElementDelegateFactory::kConventionalUnobservableSuggestWindow:
        return false;
      case TipUiElementDelegateFactory::kConventionalObservableSuggestWindow:
        return false;
      case TipUiElementDelegateFactory::kConventionalCandidateWindow:
        return false;
      case TipUiElementDelegateFactory::kConventionalIndicatorWindow:
        return true;
      case TipUiElementDelegateFactory::kImmersiveCandidateWindow:
        return false;
      case TipUiElementDelegateFactory::kImmersiveIndicatorWindow:
        return true;
      default:
        return false;
    }
  }

  CComPtr<TipTextService> text_service_;
  CComPtr<ITfContext> context_;
  const TipUiElementDelegateFactory::ElementType type_;
  CandidateList last_candidate_list_;
  bool shown_;
  DISALLOW_COPY_AND_ASSIGN(TipUiElementDelegateImpl);
};

}  // namespace

TipUiElementDelegate::TipUiElementDelegate() {}
TipUiElementDelegate::~TipUiElementDelegate() {}

TipUiElementDelegate *TipUiElementDelegateFactory::Create(
      TipTextService *text_service, ITfContext *context, ElementType type) {
  return new TipUiElementDelegateImpl(text_service, context, type);
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
