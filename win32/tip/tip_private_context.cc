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

#include "win32/tip/tip_private_context.h"

#include <msctf.h>

#include <memory>

#include "base/win_util.h"
#include "client/client_interface.h"
#include "session/commands.pb.h"
#include "session/key_info_util.h"
#include "win32/base/config_snapshot.h"
#include "win32/base/deleter.h"
#include "win32/base/input_state.h"
#include "win32/base/keyboard.h"
#include "win32/base/surrogate_pair_observer.h"
#include "win32/tip/tip_ui_element_manager.h"
#include "win32/tip/tip_text_service.h"

namespace mozc {
namespace win32 {
namespace tsf {

using ::mozc::client::ClientFactory;
using ::mozc::client::ClientInterface;
using ::mozc::commands::Capability;
using ::mozc::commands::Output;
using ::std::unique_ptr;

class TipPrivateContext::InternalState {
 public:
  InternalState(DWORD text_edit_sink_cookie,
                DWORD text_layout_sink_cookie)
    : client_(ClientFactory::NewClient()),
      text_edit_sink_cookie_(text_edit_sink_cookie),
      text_layout_sink_cookie_(text_layout_sink_cookie) {
  }
  unique_ptr<client::ClientInterface> client_;
  SurrogatePairObserver surrogate_pair_observer_;
  commands::Output last_output_;
  VirtualKey last_down_key_;
  InputBehavior input_behavior_;
  TipUiElementManager ui_element_manager_;
  VKBackBasedDeleter deleter_;

  const DWORD text_edit_sink_cookie_;
  const DWORD text_layout_sink_cookie_;
};

TipPrivateContext::TipPrivateContext(DWORD text_edit_sink_cookie,
                                     DWORD text_layout_sink_cookie)
    : state_(new InternalState(text_edit_sink_cookie,
                               text_layout_sink_cookie)) {
  Capability capability;
  capability.set_text_deletion(Capability::DELETE_PRECEDING_TEXT);
  state_->client_->set_client_capability(capability);

  // Try to reflect the current config to the IME behavior.
  const auto &snapshot = ConfigSnapshot::Get(state_->client_.get());
  auto *behavior = &state_->input_behavior_;
  behavior->prefer_kana_input = snapshot.use_kana_input;
  behavior->use_romaji_key_to_toggle_input_style =
      snapshot.use_keyboard_to_change_preedit_method;
  behavior->use_mode_indicator = snapshot.use_mode_indicator;
  behavior->direct_mode_keys = snapshot.direct_mode_keys;
}

TipPrivateContext::~TipPrivateContext() {}

ClientInterface *TipPrivateContext::GetClient() {
  return state_->client_.get();
}

SurrogatePairObserver *TipPrivateContext::GetSurrogatePairObserver() {
  return &state_->surrogate_pair_observer_;
}

TipUiElementManager *TipPrivateContext::GetUiElementManager() {
  return &state_->ui_element_manager_;
}

VKBackBasedDeleter *TipPrivateContext::GetDeleter() {
  return &state_->deleter_;
}

const Output &TipPrivateContext::last_output() const {
  return state_->last_output_;
}

Output *TipPrivateContext::mutable_last_output() {
  return &state_->last_output_;
}

const VirtualKey &TipPrivateContext::last_down_key() const {
  return state_->last_down_key_;
}

VirtualKey *TipPrivateContext::mutable_last_down_key() {
  return &state_->last_down_key_;
}

const InputBehavior &TipPrivateContext::input_behavior() const {
  return state_->input_behavior_;
}

InputBehavior *TipPrivateContext::mutable_input_behavior() {
  return &state_->input_behavior_;
}

DWORD TipPrivateContext::text_edit_sink_cookie() const {
  return state_->text_edit_sink_cookie_;
}

DWORD TipPrivateContext::text_layout_sink_cookie() const {
  return state_->text_layout_sink_cookie_;
}

}  // namespace tsf
}  // namespace win32
}  // namespace mozc
