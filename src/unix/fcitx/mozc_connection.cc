// Copyright 2010-2012, Google Inc.
// Copyright 2012~2013, Weng Xuetian <wengxt@gmail.com>
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

#include "unix/fcitx/mozc_connection.h"

#include <string>

#include "base/logging.h"
#include "base/util.h"
#include "client/client.h"
#include "ipc/ipc.h"
#include "protocol/commands.pb.h"
#include "session/ime_switch_util.h"
#include "unix/fcitx/fcitx_key_event_handler.h"
#include "unix/fcitx/surrounding_text_util.h"
#include "fcitx_mozc.h"

namespace mozc {
namespace fcitx {

MozcConnectionInterface::~MozcConnectionInterface() {
}

mozc::client::ClientInterface* CreateAndConfigureClient() {
  mozc::client::ClientInterface *client = client::ClientFactory::NewClient();
  // Currently client capability is fixed.
  commands::Capability capability;
  capability.set_text_deletion(commands::Capability::DELETE_PRECEDING_TEXT);
  client->set_client_capability(capability);
  return client;
}

MozcConnection::MozcConnection(
    mozc::client::ServerLauncherInterface *server_launcher,
    mozc::IPCClientFactoryInterface *client_factory)
    : handler_(new KeyEventHandler),
      preedit_method_(mozc::config::Config::ROMAN),
      client_factory_(client_factory) {
  VLOG(1) << "MozcConnection is created";
  mozc::client::ClientInterface *client = CreateAndConfigureClient();
  client->SetServerLauncher(server_launcher);
  client->SetIPCClientFactory(client_factory_.get());
  client_.reset(client);

  if (client_->EnsureConnection()) {
    UpdatePreeditMethod();
  }
  VLOG(1)
      << "Current preedit method is "
      << (preedit_method_ == mozc::config::Config::ROMAN ? "Roman" : "Kana");
}

MozcConnection::~MozcConnection() {
  client_->SyncData();
  VLOG(1) << "MozcConnection is destroyed";
}

void MozcConnection::UpdatePreeditMethod() {
  mozc::config::Config config;
  if (!client_->GetConfig(&config)) {
    LOG(ERROR) << "GetConfig failed";
    return;
  }
  preedit_method_ = config.has_preedit_method() ?
      config.preedit_method() : config::Config::ROMAN;
}

bool MozcConnection::TrySendKeyEvent(
    FcitxInstance* instance,
    FcitxKeySym sym, uint32 keycode, uint32 state,
    mozc::commands::CompositionMode composition_mode,
    bool layout_is_jp,
    bool is_key_up,
    mozc::commands::Output *out,
    string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  // Call EnsureConnection just in case MozcConnection::MozcConnection() fails
  // to establish the server connection.
  if (!client_->EnsureConnection()) {
    *out_error = "EnsureConnection failed";
    VLOG(1) << "EnsureConnection failed";
    return false;
  }

  mozc::commands::KeyEvent event;
  if (!handler_->GetKeyEvent(sym, keycode, state, preedit_method_, layout_is_jp, is_key_up, &event))
      return false;

  if ((composition_mode == mozc::commands::DIRECT) &&
      !mozc::config::ImeSwitchUtil::IsDirectModeCommand(event)) {
    VLOG(1) << "In DIRECT mode. Not consumed.";
    return false;  // not consumed.
  }

  commands::Context context;
  SurroundingTextInfo surrounding_text_info;
  if (GetSurroundingText(instance,
                         &surrounding_text_info)) {
    context.set_preceding_text(surrounding_text_info.preceding_text);
    context.set_following_text(surrounding_text_info.following_text);
  }

  VLOG(1) << "TrySendKeyEvent: " << std::endl << event.DebugString();
  if (!client_->SendKeyWithContext(event, context, out)) {
    *out_error = "SendKey failed";
    VLOG(1) << "ERROR";
    return false;
  }
  VLOG(1) << "OK: " << std::endl << out->DebugString();
  return true;
}

bool MozcConnection::TrySendClick(int32 unique_id,
                                  mozc::commands::Output *out,
                                  string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SELECT_CANDIDATE);
  command.set_id(unique_id);
  return TrySendRawCommand(command, out, out_error);
}

bool MozcConnection::TrySendCompositionMode(
    mozc::commands::CompositionMode mode,
    mozc::commands::Output *out,
    string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(mozc::commands::SessionCommand::SWITCH_INPUT_MODE);
  command.set_composition_mode(mode);
  return TrySendRawCommand(command, out, out_error);
}

bool MozcConnection::TrySendCommand(
    mozc::commands::SessionCommand::CommandType type,
    mozc::commands::Output *out,
    string *out_error) const {
  DCHECK(out);
  DCHECK(out_error);

  mozc::commands::SessionCommand command;
  command.set_type(type);
  return TrySendRawCommand(command, out, out_error);
}



bool MozcConnection::TrySendRawCommand(
    const mozc::commands::SessionCommand& command,
    mozc::commands::Output *out,
    string *out_error) const {
  VLOG(1) << "TrySendRawCommand: " << std::endl << command.DebugString();
  if (!client_->SendCommand(command, out)) {
    *out_error = "SendCommand failed";
    VLOG(1) << "ERROR";
    return false;
  }
  VLOG(1) << "OK: " << std::endl << out->DebugString();
  return true;
}

mozc::client::ClientInterface* MozcConnection::GetClient()
{
    return client_.get();
}

MozcConnection *MozcConnection::CreateMozcConnection() {
  mozc::client::ServerLauncher *server_launcher
      = new mozc::client::ServerLauncher;

  return new MozcConnection(server_launcher, new mozc::IPCClientFactory);
}

}  // namespace fcitx

}  // namespace mozc
