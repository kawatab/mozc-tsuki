// Copyright 2010-2018, Google Inc.
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

#include "rewriter/command_rewriter.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/logging.h"
#include "config/config_handler.h"
#include "converter/segments.h"
#include "protocol/config.pb.h"
#include "request/conversion_request.h"

namespace mozc {
namespace {

const char kPrefix[] = "【";
const char kSuffix[] = "】";
const char kDescription[] = "設定を変更します";

// Trigger CommandRewriter if and only if the Segment::key is one of
// kTriggerKeys[]
const char *kTriggerKeys[] = {
    "こまんど",
    "しーくれっと",
    "しーくれっともーど",
    "ひみつ",
    "ぷらいばしー",
    "ぷらいべーと",
    "さじぇすと",
    "ぷれぜんてーしょん",
    "ぷれぜん",
    "よそく",
    "よそくにゅうりょく",
    "よそくへんかん",
    "すいそくこうほ"
};

// Trigger Values for all commands
const char *kCommandValues[] = {
    "コマンド"
};

// Trigger Values for Incoginito Mode.
const char *kIncognitoModeValues[] = {
    "秘密",
    "シークレット",
    "シークレットモード",
    "プライバシー",
    "プライベート"
};

const char *kDisableAllSuggestionValues[] = {
    "サジェスト",
    "予測",
    "予測入力",
    "予測変換",
    "プレゼンテーション",
    "プレゼン"
};

const char kIncoginitoModeOn[] = "シークレットモードをオン";
const char kIncoginitoModeOff[] = "シークレットモードをオフ";
const char kDisableAllSuggestionOn[] = "サジェスト機能の一時停止";
const char kDisableAllSuggestionOff[] = "サジェスト機能を元に戻す";

bool FindString(const string &query, const char **values, size_t size) {
  DCHECK(values);
  DCHECK_GT(size, 0);
  for (size_t i = 0; i < size; ++i) {
    if (query == values[i]) {
      return true;
    }
  }
  return false;
}

Segment::Candidate *InsertCommandCandidate(
    Segment *segment, size_t reference_pos, size_t insert_pos) {
  DCHECK(segment);
  Segment::Candidate *candidate = segment->insert_candidate(
      std::min(segment->candidates_size(), insert_pos));
  DCHECK(candidate);
  candidate->CopyFrom(segment->candidate(reference_pos));
  candidate->attributes |= Segment::Candidate::COMMAND_CANDIDATE;
  candidate->attributes |= Segment::Candidate::NO_LEARNING;
  candidate->description = kDescription;
  candidate->prefix = kPrefix;
  candidate->suffix = kSuffix;
  candidate->inner_segment_boundary.clear();
  DCHECK(candidate->IsValid());
  return candidate;
}

bool IsSuggestionEnabled(const config::Config &config) {
  return config.use_history_suggest() ||
      config.use_dictionary_suggest() ||
      config.use_realtime_conversion();
}
}  // namespace

CommandRewriter::CommandRewriter() {}

CommandRewriter::~CommandRewriter() {}

void CommandRewriter::InsertIncognitoModeToggleCommand(
    const config::Config &config,
    Segment *segment, size_t reference_pos, size_t insert_pos) const {
  Segment::Candidate *candidate =
      InsertCommandCandidate(segment, reference_pos, insert_pos);
  DCHECK(candidate);
  if (config.incognito_mode()) {
    candidate->value = kIncoginitoModeOff;
    candidate->command = Segment::Candidate::DISABLE_INCOGNITO_MODE;
  } else {
    candidate->value = kIncoginitoModeOn;
    candidate->command = Segment::Candidate::ENABLE_INCOGNITO_MODE;
  }
  candidate->content_value = candidate->value;
}

void CommandRewriter::InsertDisableAllSuggestionToggleCommand(
    const config::Config &config,
    Segment *segment, size_t reference_pos, size_t insert_pos) const {
  if (!IsSuggestionEnabled(config)) {
    return;
  }

  Segment::Candidate *candidate =
      InsertCommandCandidate(segment, reference_pos, insert_pos);

  DCHECK(candidate);
  if (config.presentation_mode()) {
    candidate->value = kDisableAllSuggestionOff;
    candidate->command = Segment::Candidate::DISABLE_PRESENTATION_MODE;
  } else {
    candidate->value = kDisableAllSuggestionOn;
    candidate->command = Segment::Candidate::ENABLE_PRESENTATION_MODE;
  }
  candidate->content_value = candidate->value;
}

bool CommandRewriter::RewriteSegment(const config::Config &config,
                                     Segment *segment) const {
  DCHECK(segment);

  for (size_t i = 0; i < segment->candidates_size(); ++i) {
    const string &value = segment->candidate(i).value;
    if (FindString(value, kCommandValues, arraysize(kCommandValues))) {
      // insert command candidate at an fixed position.
      InsertDisableAllSuggestionToggleCommand(config, segment, i, 6);
      InsertIncognitoModeToggleCommand(config, segment, i, 6);
      return true;
    }
    if (FindString(value, kIncognitoModeValues,
                   arraysize(kIncognitoModeValues))) {
      InsertIncognitoModeToggleCommand(config, segment, i, i + 3);
      return true;
    }
    if (FindString(value, kDisableAllSuggestionValues,
                   arraysize(kDisableAllSuggestionValues))) {
      InsertDisableAllSuggestionToggleCommand(config, segment, i, i + 3);
      return true;
    }
  }

  return false;
}

bool CommandRewriter::Rewrite(const ConversionRequest &request,
                              Segments *segments) const {
  if (segments == NULL || segments->conversion_segments_size() != 1) {
    return false;
  }

  Segment *segment = segments->mutable_conversion_segment(0);
  DCHECK(segment);
  const string &key = segment->key();

  // TODO(taku): we want to replace the linear search with STL map when
  // kTriggerKeys become bigger.
  if (!FindString(key, kTriggerKeys, arraysize(kTriggerKeys))) {
    return false;
  }

  return RewriteSegment(request.config(), segment);
}
}  // namespace mozc
