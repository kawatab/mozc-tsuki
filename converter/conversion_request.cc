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

#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "base/logging.h"
#include "session/commands.pb.h"

namespace mozc {

ConversionRequest::ConversionRequest()
    : composer_(NULL),
      request_(&commands::Request::default_instance()),
      use_actual_converter_for_realtime_conversion_(false),
      composer_key_selection_(CONVERSION_KEY),
      skip_slow_rewriters_(false),
      create_partial_candidates_(false) {}

ConversionRequest::ConversionRequest(const composer::Composer *c,
                                     const commands::Request *request)
    : composer_(c),
      request_(request),
      use_actual_converter_for_realtime_conversion_(false),
      composer_key_selection_(CONVERSION_KEY),
      skip_slow_rewriters_(false),
      create_partial_candidates_(false) {}

ConversionRequest::~ConversionRequest() {}

bool ConversionRequest::has_composer() const {
  return composer_ != NULL;
}

const composer::Composer &ConversionRequest::composer() const {
  DCHECK(composer_);
  return *composer_;
}

void ConversionRequest::set_composer(const composer::Composer *c) {
  composer_ = c;
}

const commands::Request &ConversionRequest::request() const {
  DCHECK(request_);
  return *request_;
}

bool ConversionRequest::use_actual_converter_for_realtime_conversion() const {
  return use_actual_converter_for_realtime_conversion_;
}

void ConversionRequest::set_use_actual_converter_for_realtime_conversion(
    bool value) {
  use_actual_converter_for_realtime_conversion_ = value;
}

ConversionRequest::ComposerKeySelection
ConversionRequest::composer_key_selection() const {
  return composer_key_selection_;
}

void ConversionRequest::set_composer_key_selection(
    ComposerKeySelection selection) {
  composer_key_selection_ = selection;
}

bool ConversionRequest::skip_slow_rewriters() const {
  return skip_slow_rewriters_;
}

void ConversionRequest::set_skip_slow_rewriters(bool value) {
  skip_slow_rewriters_ = value;
}

bool ConversionRequest::create_partial_candidates() const {
  return create_partial_candidates_;
}

void ConversionRequest::set_create_partial_candidates(bool value) {
  create_partial_candidates_ = value;
}

bool ConversionRequest::IsKanaModifierInsensitiveConversion() const {
  return request_->kana_modifier_insensitive_conversion() &&
         GET_CONFIG(use_kana_modifier_insensitive_conversion);
}

void ConversionRequest::CopyFrom(const ConversionRequest &request) {
  composer_ = request.composer_;
  request_ = request.request_;
  use_actual_converter_for_realtime_conversion_ =
      request.use_actual_converter_for_realtime_conversion_;
  composer_key_selection_ = request.composer_key_selection_;
  skip_slow_rewriters_ = request.skip_slow_rewriters_;
  create_partial_candidates_ = request.create_partial_candidates_;
}

}  // namespace mozc
