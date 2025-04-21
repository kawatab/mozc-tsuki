// Copyright 2010-2021, Google Inc.
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

#include "win32/tip/tip_ui_handler_conventional.h"

#include <msctf.h>
#include <wil/com.h>
#include <windows.h>

#include <cstddef>
#include <utility>

#include "base/logging.h"
#include "base/util.h"
#include "base/win32/com.h"
#include "base/win32/win_util.h"
#include "protocol/candidates.pb.h"
#include "protocol/commands.pb.h"
#include "protocol/renderer_command.pb.h"
#include "renderer/win32/win32_renderer_client.h"
#include "win32/base/input_state.h"
#include "win32/tip/tip_composition_util.h"
#include "win32/tip/tip_dll_module.h"
#include "win32/tip/tip_input_mode_manager.h"
#include "win32/tip/tip_private_context.h"
#include "win32/tip/tip_range_util.h"
#include "win32/tip/tip_text_service.h"
#include "win32/tip/tip_thread_context.h"
#include "win32/tip/tip_ui_element_manager.h"

namespace mozc {
namespace win32 {
namespace tsf {
namespace {

using ::mozc::commands::CompositionMode;
using ::mozc::commands::Preedit;
using ::mozc::renderer::win32::Win32RendererClient;
using Segment = ::mozc::commands::Preedit_Segment;
using Annotation = ::mozc::commands::Preedit_Segment::Annotation;
using IndicatorInfo = ::mozc::commands::RendererCommand_IndicatorInfo;
using RendererCommand = ::mozc::commands::RendererCommand;
using ApplicationInfo = ::mozc::commands::RendererCommand::ApplicationInfo;

size_t GetTargetPos(const commands::Output &output) {
  if (!output.has_candidates() || !output.candidates().has_category()) {
    return 0;
  }
  switch (output.candidates().category()) {
    case commands::PREDICTION:
    case commands::SUGGESTION:
      return 0;
    case commands::CONVERSION: {
      const Preedit &preedit = output.preedit();
      size_t offset = 0;
      for (int i = 0; i < preedit.segment_size(); ++i) {
        const Segment &segment = preedit.segment(i);
        const Annotation &annotation = segment.annotation();
        if (annotation == Segment::HIGHLIGHT) {
          return offset;
        }
        offset += Util::WideCharsLen(segment.value());
      }
      return offset;
    }
    default:
      return 0;
  }
}

bool FillVisibility(ITfUIElementMgr *ui_element_manager,
                    TipPrivateContext *private_context,
                    RendererCommand *command) {
  command->set_visible(false);

  if (private_context == nullptr) {
    return false;
  }

  const bool show_suggest_window =
      private_context->GetUiElementManager()->IsVisible(
          ui_element_manager, TipUiElementManager::kSuggestWindow);
  const bool show_candidate_window =
      private_context->GetUiElementManager()->IsVisible(
          ui_element_manager, TipUiElementManager::kCandidateWindow);

  const commands::Output &output = private_context->last_output();

  bool suggest_window_visible = false;
  bool candidate_window_visible = false;

  // Check if suggest window and candidate window are actually visible.
  if (output.has_candidates() && output.candidates().has_category()) {
    switch (output.candidates().category()) {
      case commands::SUGGESTION:
        suggest_window_visible = show_suggest_window;
        break;
      case commands::CONVERSION:
      case commands::PREDICTION:
        candidate_window_visible = show_candidate_window;
        break;
      default:
        // do nothing.
        break;
    }
  }

  if (candidate_window_visible || suggest_window_visible) {
    command->set_visible(true);
  }

  ApplicationInfo *app_info = command->mutable_application_info();

  int visibility = ApplicationInfo::ShowUIDefault;
  if (show_candidate_window) {
    // Note that |ApplicationInfo::ShowCandidateWindow| represents that the
    // application does not mind the IME showing its own candidate window.
    // This bit does not mean that |command| requires the suggest window.
    visibility |= ApplicationInfo::ShowCandidateWindow;
  }
  if (show_suggest_window) {
    // Note that |ApplicationInfo::ShowCandidateWindow| represents that the
    // application does not mind the IME showing its own candidate window.
    // This bit does not mean that |command| requires the suggest window.
    visibility |= ApplicationInfo::ShowSuggestWindow;
  }
  app_info->set_ui_visibilities(visibility);

  return true;
}

bool FillWindowHandle(ITfContext *context, ApplicationInfo *app_info) {
  wil::com_ptr_nothrow<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view)) || !context_view) {
    return false;
  }

  HWND window_handle = nullptr;
  if (FAILED(context_view->GetWnd(&window_handle))) {
    return false;
  }
  app_info->set_target_window_handle(
      WinUtil::EncodeWindowHandle(window_handle));
  return true;
}

wil::com_ptr_nothrow<ITfRange> GetCompositionRange(ITfContext *context,
                                                   TfEditCookie read_cookie) {
  wil::com_ptr_nothrow<ITfCompositionView> composition_view =
      TipCompositionUtil::GetCompositionView(context, read_cookie);
  if (!composition_view) {
    return nullptr;
  }

  wil::com_ptr_nothrow<ITfRange> composition_range;
  if (FAILED(composition_view->GetRange(&composition_range))) {
    return nullptr;
  }
  return composition_range;
}

wil::com_ptr_nothrow<ITfRange> GetSelectionRange(ITfContext *context,
                                                 TfEditCookie read_cookie) {
  wil::com_ptr_nothrow<ITfRange> selection_range;
  TfActiveSelEnd sel_end = TF_AE_NONE;
  if (FAILED(TipRangeUtil::GetDefaultSelection(context, read_cookie,
                                               &selection_range, &sel_end))) {
    return nullptr;
  }
  return selection_range;
}

// This function updates RendererCommand::CharacterPosition to emulate
// IMM32-based client. Ideally we'd better to define new field for TSF Mozc
// into which the result of ITfContextView::GetTextExt is stored.
// TODO(yukawa): Replace FillCharPosition with new one designed for TSF.
bool FillCharPosition(TipPrivateContext *private_context, ITfContext *context,
                      TfEditCookie read_cookie, bool has_composition,
                      ApplicationInfo *app_info, bool *no_layout) {
  if (private_context == nullptr) {
    return false;
  }
  bool dummy_no_layout = false;
  if (no_layout == nullptr) {
    no_layout = &dummy_no_layout;
  }
  *no_layout = false;

  if (!app_info->has_target_window_handle()) {
    return false;
  }

  const HWND window_handle =
      WinUtil::DecodeWindowHandle(app_info->target_window_handle());

  wil::com_ptr_nothrow<ITfRange> range =
      has_composition ? GetCompositionRange(context, read_cookie)
                      : GetSelectionRange(context, read_cookie);
  if (!range) {
    return false;
  }
  wil::com_ptr_nothrow<ITfRange> target_range;
  if (FAILED(range->Clone(&target_range))) {
    return false;
  }
  if (!target_range) {
    return false;
  }

  const commands::Output &output = private_context->last_output();
  LONG shifted = 0;
  if (FAILED(target_range->Collapse(read_cookie, TF_ANCHOR_START))) {
    return false;
  }
  const size_t target_pos = GetTargetPos(output);
  if (FAILED(target_range->ShiftStart(read_cookie, target_pos, &shifted,
                                      nullptr))) {
    return false;
  }
  if (FAILED(target_range->ShiftEnd(read_cookie, target_pos + 1, &shifted,
                                    nullptr))) {
    return false;
  }

  wil::com_ptr_nothrow<ITfContextView> context_view;
  if (FAILED(context->GetActiveView(&context_view)) || !context_view) {
    return false;
  }

  RECT document_rect = {};
  if (FAILED(context_view->GetScreenExt(&document_rect))) {
    return false;
  }

  RECT text_rect = {};
  bool clipped = false;
  const HRESULT hr =
      TipRangeUtil::GetTextExt(context_view.get(), read_cookie,
                               target_range.get(), &text_rect, &clipped);
  if (hr == TF_E_NOLAYOUT) {
    // This is not a critical error but the layout information is not available.
    *no_layout = true;
    return true;
  }
  if (FAILED(hr)) {
    // Any other errors are unexpected.
    return false;
  }

  RendererCommand::Point *top_left =
      app_info->mutable_composition_target()->mutable_top_left();
  top_left->set_x(text_rect.left);
  top_left->set_y(text_rect.top);
  app_info->mutable_composition_target()->set_position(0);
  app_info->mutable_composition_target()->set_line_height(text_rect.bottom -
                                                          text_rect.top);

  RendererCommand::Rectangle *area =
      app_info->mutable_composition_target()->mutable_document_area();
  area->set_left(document_rect.left);
  area->set_top(document_rect.top);
  area->set_right(document_rect.right);
  area->set_bottom(document_rect.bottom);

  return true;
}

void UpdateCommand(TipTextService *text_service, ITfContext *context,
                   TfEditCookie read_cookie, RendererCommand *command,
                   bool *no_layout) {
  command->Clear();
  command->set_type(RendererCommand::UPDATE);

  TipPrivateContext *private_context = text_service->GetPrivateContext(context);
  if (private_context != nullptr) {
    *command->mutable_output() = private_context->last_output();
    private_context->GetUiElementManager()->OnUpdate(text_service, context);
  }

  ApplicationInfo *app_info = command->mutable_application_info();
  app_info->set_input_framework(ApplicationInfo::TSF);
  app_info->set_process_id(::GetCurrentProcessId());
  app_info->set_thread_id(::GetCurrentThreadId());
  app_info->set_receiver_handle(WinUtil::EncodeWindowHandle(
      text_service->renderer_callback_window_handle()));

  auto ui_element_manager =
      ComQuery<ITfUIElementMgr>(text_service->GetThreadManager());
  DCHECK(ui_element_manager);
  FillVisibility(ui_element_manager.get(), private_context, command);
  FillWindowHandle(context, app_info);
  FillCharPosition(private_context, context, read_cookie,
                   command->output().has_preedit(), app_info, no_layout);

  if (private_context != nullptr) {
    const TipInputModeManager *input_mode_manager =
        text_service->GetThreadContext()->GetInputModeManager();
    if (private_context->input_behavior().use_mode_indicator &&
        input_mode_manager->IsIndicatorVisible()) {
      command->set_visible(true);
      IndicatorInfo *info = app_info->mutable_indicator_info();
      info->mutable_status()->set_activated(
          input_mode_manager->GetEffectiveOpenClose());
      info->mutable_status()->set_mode(static_cast<CompositionMode>(
          input_mode_manager->GetEffectiveConversionMode()));
    }
  }

  // Regardless of the value of |command->visible()| here, we should hide
  // all the UI elements whenever the current threads is not focused.
  BOOL thread_focus = FALSE;
  const HRESULT hr =
      text_service->GetThreadManager()->IsThreadFocus(&thread_focus);
  if (SUCCEEDED(hr) && (thread_focus == FALSE)) {
    command->set_visible(false);
  }
}

// This class is an implementation class for the ITfEditSession classes, which
// is an observer for exclusively read the date from the text store.
class UpdateUiEditSessionImpl final : public TipComImplements<ITfEditSession> {
 public:
  // The ITfEditSession interface method.
  // This function is called back by the TSF thread manager when an edit
  // request is granted.
  STDMETHODIMP DoEditSession(TfEditCookie edit_cookie) override {
    RendererCommand command;
    bool no_layout = false;
    UpdateCommand(text_service_.get(), context_.get(), edit_cookie, &command,
                  &no_layout);
    if (!no_layout || !command.visible()) {
      Win32RendererClient::OnUpdated(command);
    }
    return S_OK;
  }

  static bool BeginRequest(TipTextService *text_service, ITfContext *context) {
    // When RequestEditSession fails, it does not maintain the reference count.
    // So we need to ensure that AddRef/Release should be called at least once
    // per object.
    wil::com_ptr_nothrow<ITfEditSession> edit_session(
        new UpdateUiEditSessionImpl(text_service, context));

    HRESULT edit_session_result = S_OK;
    const HRESULT result = context->RequestEditSession(
        text_service->GetClientID(), edit_session.get(),
        TF_ES_ASYNCDONTCARE | TF_ES_READ, &edit_session_result);
    return SUCCEEDED(result);
  }

 private:
  UpdateUiEditSessionImpl(wil::com_ptr_nothrow<TipTextService> text_service,
                          wil::com_ptr_nothrow<ITfContext> context)
      : text_service_(std::move(text_service)), context_(std::move(context)) {}

  wil::com_ptr_nothrow<TipTextService> text_service_;
  wil::com_ptr_nothrow<ITfContext> context_;
};

}  // namespace

void TipUiHandlerConventional::OnActivate(TipTextService *text_service) {
  ITfThreadMgr *thread_mgr = text_service->GetThreadManager();
  wil::com_ptr_nothrow<ITfDocumentMgr> document;
  if (FAILED(thread_mgr->GetFocus(&document))) {
    return;
  }
  OnFocusChange(text_service, document.get());
}

void TipUiHandlerConventional::OnDeactivate() {
  Win32RendererClient::OnUIThreadUninitialized();
}

void TipUiHandlerConventional::OnFocusChange(
    TipTextService *text_service, ITfDocumentMgr *focused_document_manager) {
  if (!focused_document_manager) {
    // Empty document. Hide the renderer.
    RendererCommand command;
    command.set_type(RendererCommand::UPDATE);
    command.set_visible(false);
    Win32RendererClient::OnUpdated(command);
    return;
  }

  wil::com_ptr_nothrow<ITfContext> context;
  if (FAILED(focused_document_manager->GetBase(&context))) {
    return;
  }
  if (!context) {
    return;
  }
  UpdateUiEditSessionImpl::BeginRequest(text_service, context.get());
}

bool TipUiHandlerConventional::Update(TipTextService *text_service,
                                      ITfContext *context,
                                      TfEditCookie read_cookie) {
  RendererCommand command;
  bool no_layout = false;
  UpdateCommand(text_service, context, read_cookie, &command, &no_layout);
  if (!no_layout || !command.visible()) {
    Win32RendererClient::OnUpdated(command);
  }
  return true;
}

bool TipUiHandlerConventional::OnDllProcessAttach(HINSTANCE module_handle,
                                                  bool static_loading) {
  Win32RendererClient::OnModuleLoaded(module_handle);
  return true;
}

void TipUiHandlerConventional::OnDllProcessDetach(HINSTANCE module_handle,
                                                  bool process_shutdown) {
  Win32RendererClient::OnModuleUnloaded();
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
