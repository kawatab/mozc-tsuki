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

#include "win32/tip/tip_ui_element_manager.h"

#define _ATL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_AUTOMATIC_NAMESPACE
// Workaround against KB813540
#include <atlbase_mozc.h>
#include <atlcom.h>
#include <atlstr.h>
#include <msctf.h>

#include "base/hash_tables.h"
#include "renderer/renderer_command.pb.h"
#include "session/commands.pb.h"
#include "win32/base/input_state.h"
#include "win32/tip/tip_input_mode_manager.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_ui_handler.h"

namespace mozc {
namespace win32 {
namespace tsf {

namespace {

using ATL::CComPtr;
using ATL::CComQIPtr;
using ::mozc::commands::Output;
typedef ::mozc::commands::RendererCommand_IndicatorInfo IndicatorInfo;

struct UIElementInfo {
  UIElementInfo()
      : id(TF_INVALID_UIELEMENTID) {
  }
  DWORD id;
  CComPtr<ITfUIElement> element;
};

HRESULT BeginUI(ITfUIElementMgr *ui_element_manager,
                ITfUIElement *ui_element,
                DWORD *new_element_id) {
  BOOL show = FALSE;
  *new_element_id = TF_INVALID_UIELEMENTID;
  CComPtr<ITfUIElement> element(ui_element);
  const HRESULT result = ui_element_manager->BeginUIElement(
      element, &show, new_element_id);
  if (FAILED(result)) {
    return result;
  }
  element->Show(show);
  return S_OK;
}

HRESULT EndUI(ITfUIElementMgr *ui_element_manager, DWORD element_id) {
  CComPtr<ITfUIElement> element;
  ui_element_manager->GetUIElement(element_id, &element);
  if (element) {
    element->Show(FALSE);
  }
  ui_element_manager->EndUIElement(element_id);
  return S_OK;
}

}  // namespace

class TipUiElementManager::UiElementMap
    : public hash_map<TipUiElementManager::UIElementFlags, UIElementInfo> {
};

TipUiElementManager::TipUiElementManager()
    : ui_element_map_(new UiElementMap) {}

TipUiElementManager::~TipUiElementManager() {}

ITfUIElement *TipUiElementManager::GetElement(UIElementFlags element) const {
  const UiElementMap::const_iterator it = ui_element_map_->find(element);
  if (it == ui_element_map_->end()) {
    return nullptr;
  }
  return it->second.element;
}

DWORD TipUiElementManager::GetElementId(UIElementFlags element) const {
  const UiElementMap::const_iterator it = ui_element_map_->find(element);
  if (it == ui_element_map_->end()) {
    return TF_INVALID_UIELEMENTID;
  }
  return it->second.id;
}

HRESULT TipUiElementManager::OnUpdate(
    TipTextService *text_service, ITfContext *context) {
  CComQIPtr<ITfUIElementMgr> ui_element_manager =
      text_service->GetThreadManager();
  if (!ui_element_manager) {
    return E_FAIL;
  }
  TipPrivateContext *private_context =
        text_service->GetPrivateContext(context);
  if (private_context == nullptr) {
    return E_FAIL;
  }

  const Output &output = private_context->last_output();

  uint32 existence_bits = kNoneWindow;
  if (output.has_candidates() && output.candidates().has_category()) {
    switch (output.candidates().category()) {
      case commands::SUGGESTION:
        existence_bits |= kSuggestWindow;
        break;
      case commands::PREDICTION:
      case commands::CONVERSION:
        existence_bits |= kCandidateWindow;
        break;
      default:
        break;
    }
  }
  if (private_context->input_behavior().use_mode_indicator &&
      text_service->GetThreadContext()->GetInputModeManager()->
          IsIndicatorVisible()) {
    existence_bits |= kIndicatorWindow;
  }

  DWORD suggest_ui_id = TF_INVALID_UIELEMENTID;
  CComPtr<ITfUIElement> suggest_ui;
  {
    const UiElementMap::const_iterator it =
        ui_element_map_->find(kSuggestWindow);
    if (it != ui_element_map_->end()) {
      suggest_ui_id = it->second.id;
      suggest_ui = it->second.element;
    }
  }
  DWORD candidate_ui_id = TF_INVALID_UIELEMENTID;
  CComPtr<ITfUIElement> candidate_ui;
  {
    const UiElementMap::const_iterator it =
        ui_element_map_->find(kCandidateWindow);
    if (it != ui_element_map_->end()) {
      candidate_ui_id = it->second.id;
      candidate_ui = it->second.element;
    }
  }
  DWORD indicator_ui_id = TF_INVALID_UIELEMENTID;
  CComPtr<ITfUIElement> indicator_ui;
  {
    const UiElementMap::const_iterator it =
        ui_element_map_->find(kIndicatorWindow);
    if (it != ui_element_map_->end()) {
      indicator_ui_id = it->second.id;
      indicator_ui = it->second.element;
    }
  }

  enum UpdateMode {
    kUINone,            // UI is not changed.
    kUIBeginAndUpdate,  // Begin() and Update() should be called.
    kUIEnd,             // End() should be called.
    kUIUpdate,          // Update() should be called.
  };

  UpdateMode suggest_mode = kUINone;
  if ((existence_bits & kSuggestWindow) == kSuggestWindow) {
    if (suggest_ui_id == TF_INVALID_UIELEMENTID) {
      suggest_mode = kUIBeginAndUpdate;
    } else {
      suggest_mode = kUIUpdate;
    }
  } else {
    if (suggest_ui_id != TF_INVALID_UIELEMENTID) {
      suggest_mode = kUIEnd;
    }
  }

  UpdateMode candidate_mode = kUINone;
  if ((existence_bits & kCandidateWindow) == kCandidateWindow) {
    if (candidate_ui_id == TF_INVALID_UIELEMENTID) {
      candidate_mode = kUIBeginAndUpdate;
    } else {
      candidate_mode = kUIUpdate;
    }
  } else {
    if (candidate_ui_id != TF_INVALID_UIELEMENTID) {
      candidate_mode = kUIEnd;
    }
  }

  UpdateMode indicator_mode = kUINone;
  if ((existence_bits & kIndicatorWindow) == kIndicatorWindow) {
    if (indicator_ui_id == TF_INVALID_UIELEMENTID) {
      indicator_mode = kUIBeginAndUpdate;
    } else {
      indicator_mode = kUIUpdate;
    }
  } else {
    if (indicator_ui_id != TF_INVALID_UIELEMENTID) {
      indicator_mode = kUIEnd;
    }
  }

  if (suggest_mode == kUIEnd) {
    EndUI(ui_element_manager, suggest_ui_id);
    suggest_ui_id = TF_INVALID_UIELEMENTID;
    ui_element_map_->erase(kSuggestWindow);
    if (suggest_ui) {
      TipUiHandler::OnDestroyElement(text_service, suggest_ui);
    }
  }
  if (candidate_mode == kUIEnd) {
    EndUI(ui_element_manager, candidate_ui_id);
    candidate_ui_id = TF_INVALID_UIELEMENTID;
    ui_element_map_->erase(kCandidateWindow);
    if (candidate_ui) {
      TipUiHandler::OnDestroyElement(text_service, candidate_ui);
    }
  }
  if (indicator_mode == kUIEnd) {
    EndUI(ui_element_manager, indicator_ui_id);
    indicator_ui_id = TF_INVALID_UIELEMENTID;
    ui_element_map_->erase(kIndicatorWindow);
    if (indicator_ui) {
      TipUiHandler::OnDestroyElement(text_service, indicator_ui);
    }
  }

  if (suggest_mode == kUIBeginAndUpdate) {
    CComPtr<ITfUIElement> suggest_ui = TipUiHandler::CreateUI(
        TipUiHandler::kSuggestWindow, text_service, context);
    if (suggest_ui) {
      DWORD new_suggest_ui_id = TF_INVALID_UIELEMENTID;
      if (SUCCEEDED(BeginUI(ui_element_manager,
                            suggest_ui,
                            &new_suggest_ui_id))) {
        (*ui_element_map_)[kSuggestWindow].element = suggest_ui;
        (*ui_element_map_)[kSuggestWindow].id = new_suggest_ui_id;
        suggest_ui_id = new_suggest_ui_id;
      }
    }
  }
  if (candidate_mode == kUIBeginAndUpdate) {
    CComPtr<ITfUIElement> candidate_ui = TipUiHandler::CreateUI(
        TipUiHandler::kCandidateWindow, text_service, context);
    if (candidate_ui) {
      DWORD new_candidate_ui_id = TF_INVALID_UIELEMENTID;
      if (SUCCEEDED(BeginUI(ui_element_manager,
                            candidate_ui,
                            &new_candidate_ui_id))) {
        (*ui_element_map_)[kCandidateWindow].element = candidate_ui;
        (*ui_element_map_)[kCandidateWindow].id = new_candidate_ui_id;
        candidate_ui_id = new_candidate_ui_id;
      }
    }
  }
  if (indicator_mode == kUIBeginAndUpdate) {
    CComPtr<ITfUIElement> indicator_ui = TipUiHandler::CreateUI(
        TipUiHandler::kIndicatorWindow, text_service, context);
    if (indicator_ui) {
      DWORD new_indicator_ui_id = TF_INVALID_UIELEMENTID;
      if (SUCCEEDED(BeginUI(ui_element_manager,
                            indicator_ui,
                            &new_indicator_ui_id))) {
        (*ui_element_map_)[kIndicatorWindow].element = indicator_ui;
        (*ui_element_map_)[kIndicatorWindow].id = new_indicator_ui_id;
        candidate_ui_id = new_indicator_ui_id;
      }
    }
  }

  if (suggest_mode == kUIUpdate || suggest_mode == kUIBeginAndUpdate) {
    ui_element_manager->UpdateUIElement(suggest_ui_id);
  }
  if (candidate_mode == kUIUpdate || candidate_mode == kUIBeginAndUpdate) {
    ui_element_manager->UpdateUIElement(candidate_ui_id);
  }
  if (indicator_mode == kUIUpdate || indicator_mode == kUIBeginAndUpdate) {
    ui_element_manager->UpdateUIElement(indicator_ui_id);
  }
  return S_OK;
}

bool TipUiElementManager::IsVisible(ITfUIElementMgr *ui_element_manager,
                                    UIElementFlags element) const {
  if (ui_element_manager == nullptr) {
    return false;
  }
  const UiElementMap::const_iterator it = ui_element_map_->find(element);
  if (it == ui_element_map_->end()) {
    return false;
  }

  BOOL shown = FALSE;
  if (FAILED(it->second.element->IsShown(&shown))) {
    return false;
  }
  return !!shown;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
