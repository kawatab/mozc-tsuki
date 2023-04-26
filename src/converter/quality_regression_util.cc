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

#include "converter/quality_regression_util.h"

#include <cstdint>
#include <sstream>  // NOLINT
#include <string>
#include <utility>
#include <vector>

#include "base/file_stream.h"
#include "base/logging.h"
#include "base/port.h"
#include "base/text_normalizer.h"
#include "base/util.h"
#include "composer/composer.h"
#include "composer/table.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "protocol/commands.pb.h"
#include "request/conversion_request.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"

namespace mozc {
namespace quality_regression {
namespace {

constexpr char kConversionExpect[] = "Conversion Expected";
constexpr char kConversionNotExpect[] = "Conversion Not Expected";
constexpr char kConversionMatch[] = "Conversion Match";
constexpr char kConversionNotMatch[] = "Conversion Not Match";
constexpr char kReverseConversionExpect[] = "ReverseConversion Expected";
constexpr char kReverseConversionNotExpect[] = "ReverseConversion Not Expected";
// For now, suggestion and prediction are using same implementation
constexpr char kPredictionExpect[] = "Prediction Expected";
constexpr char kPredictionNotExpect[] = "Prediction Not Expected";
constexpr char kSuggestionExpect[] = "Suggestion Expected";
constexpr char kSuggestionNotExpect[] = "Suggestion Not Expected";
// Zero query
constexpr char kZeroQueryExpect[] = "ZeroQuery Expected";
constexpr char kZeroQueryNotExpect[] = "ZeroQuery Not Expected";

// copied from evaluation/quality_regression/evaluator.cc
int GetRank(const std::string &value, const Segments *segments,
            size_t current_pos, size_t current_segment) {
  if (current_segment == segments->segments_size()) {
    if (current_pos == value.size()) {
      return 0;
    } else {
      return -1;
    }
  }
  const Segment &seg = segments->segment(current_segment);
  for (size_t i = 0; i < seg.candidates_size(); ++i) {
    const std::string &cand_value = seg.candidate(i).value;
    const size_t len = cand_value.size();
    if (current_pos + len > value.size()) {
      continue;
    }
    if (strncmp(cand_value.c_str(), value.c_str() + current_pos, len) != 0) {
      continue;
    }
    const int rest =
        GetRank(value, segments, current_pos + len, current_segment + 1);
    if (rest == -1) {
      continue;
    }
    return i + rest;
  }
  return -1;
}

absl::StatusOr<uint32_t> GetPlatformFromString(absl::string_view str) {
  std::string lower;
  lower.assign(str.data(), str.size());
  Util::LowerString(&lower);
  if (str == "desktop") {
    return QualityRegressionUtil::DESKTOP;
  }
  if (str == "oss") {
    return QualityRegressionUtil::OSS;
  }
  if (str == "mobile") {
    return QualityRegressionUtil::MOBILE;
  }
  if (str == "mobile_ambiguous") {
    return QualityRegressionUtil::MOBILE_AMBIGUOUS;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown platform name: ", str));
}
}  // namespace

std::string QualityRegressionUtil::TestItem::OutputAsTSV() const {
  std::ostringstream os;
  os << label << '\t' << key << '\t' << expected_value << '\t' << command
     << '\t' << expected_rank << '\t' << accuracy << '\t' << platform;
  // TODO(toshiyuki): platform enum to string
  return os.str();
}

absl::Status QualityRegressionUtil::TestItem::ParseFromTSV(
    const std::string &line) {
  std::vector<absl::string_view> tokens =
      absl::StrSplit(line, '\t', absl::SkipEmpty());
  if (tokens.size() < 4) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid token size: ", line));
  }
  label.assign(tokens[0].data(), tokens[0].size());
  key.assign(tokens[1].data(), tokens[1].size());
  expected_value = TextNormalizer::NormalizeText(tokens[2]);
  command.assign(tokens[3].data(), tokens[3].size());

  if (tokens.size() == 4) {
    if (absl::StartsWith(command, kConversionExpect) &&
        command != kConversionExpect) {
      constexpr int kSize = std::size(kConversionExpect);  // Size with '\0'.
      expected_rank = NumberUtil::SimpleAtoi(command.substr(kSize));
      command = kConversionExpect;
    } else {
      expected_rank = 0;
    }
  } else {
    expected_rank = NumberUtil::SimpleAtoi(tokens[4]);
  }

  if (tokens.size() > 5) {
    NumberUtil::SafeStrToDouble(tokens[5], &accuracy);
  } else {
    accuracy = 1.0;
  }
  platform = 0;
  if (tokens.size() >= 7) {
    std::vector<absl::string_view> platforms =
        absl::StrSplit(tokens[6], ',', absl::SkipEmpty());
    for (size_t i = 0; i < platforms.size(); ++i) {
      auto result = GetPlatformFromString(platforms[i]);
      if (!result.ok()) {
        return std::move(result.status());
      }
      platform |= *result;
    }
  } else {
    // Default platform: desktop
    platform = QualityRegressionUtil::DESKTOP;
  }
  return absl::OkStatus();
}

QualityRegressionUtil::QualityRegressionUtil(ConverterInterface *converter)
    : converter_(converter),
      request_(new commands::Request),
      config_(new config::Config),
      segments_(new Segments) {}

QualityRegressionUtil::~QualityRegressionUtil() {}

namespace {
absl::Status ParseFileInternal(
    const std::string &filename,
    std::vector<QualityRegressionUtil::TestItem> *outputs) {
  // TODO(taku): support an XML file of Mozcsu.
  InputFileStream ifs(filename.c_str());
  if (!ifs.good()) {
    return absl::UnavailableError(absl::StrCat("Failed to read: ", filename));
  }
  std::string line;
  while (!std::getline(ifs, line).fail()) {
    if (line.empty() || line.c_str()[0] == '#') {
      continue;
    }
    QualityRegressionUtil::TestItem item;
    if (!item.ParseFromTSV(line).ok()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Failed to parse: ", line));
    }
    outputs->push_back(item);
  }
  return absl::OkStatus();
}
}  // namespace

// static
absl::Status QualityRegressionUtil::ParseFile(const std::string &filename,
                                              std::vector<TestItem> *outputs) {
  outputs->clear();
  return ParseFileInternal(filename, outputs);
}

// static
absl::Status QualityRegressionUtil::ParseFiles(
    const std::vector<std::string> &filenames, std::vector<TestItem> *outputs) {
  outputs->clear();
  for (const std::string &filename : filenames) {
    const absl::Status result = ParseFileInternal(filename, outputs);
    if (!result.ok()) {
      return result;
    }
  }
  return absl::OkStatus();
}

// static

absl::StatusOr<bool> QualityRegressionUtil::ConvertAndTest(
    const TestItem &item, std::string *actual_value) {
  const std::string &key = item.key;
  const std::string &expected_value = item.expected_value;
  const std::string &command = item.command;
  const int expected_rank = item.expected_rank;

  CHECK(actual_value);
  segments_->Clear();
  converter_->ResetConversion(segments_.get());
  actual_value->clear();

  composer::Table table;

  if (command == kConversionExpect || command == kConversionNotExpect ||
      command == kConversionMatch || command == kConversionNotMatch) {
    composer::Composer composer(&table, request_.get(), config_.get());
    composer.SetPreeditTextForTestOnly(key);
    ConversionRequest conversion_request(&composer, request_.get(),
                                         config_.get());
    converter_->StartConversionForRequest(conversion_request, segments_.get());
  } else if (command == kReverseConversionExpect ||
             command == kReverseConversionNotExpect) {
    converter_->StartReverseConversion(segments_.get(), key);
  } else if (command == kPredictionExpect || command == kPredictionNotExpect) {
    composer::Composer composer(&table, request_.get(), config_.get());
    composer.SetPreeditTextForTestOnly(key);
    ConversionRequest conversion_request(&composer, request_.get(),
                                         config_.get());
    converter_->StartPredictionForRequest(conversion_request, segments_.get());

  } else if (command == kSuggestionExpect || command == kSuggestionNotExpect) {
    composer::Composer composer(&table, request_.get(), config_.get());
    composer.SetPreeditTextForTestOnly(key);
    ConversionRequest conversion_request(&composer, request_.get(),
                                         config_.get());
    converter_->StartSuggestionForRequest(conversion_request, segments_.get());
  } else if (command == kZeroQueryExpect || command == kZeroQueryNotExpect) {
    commands::Request request = *request_;
    request.set_zero_query_suggestion(true);
    request.set_mixed_conversion(true);
    {
      composer::Composer composer(&table, &request, config_.get());
      composer.SetPreeditTextForTestOnly(key);
      ConversionRequest conversion_request(&composer, &request, config_.get());
      conversion_request.set_max_conversion_candidates_size(10);
      converter_->StartSuggestionForRequest(conversion_request,
                                            segments_.get());
      converter_->CommitSegmentValue(segments_.get(), 0, 0);
      converter_->FinishConversion(conversion_request, segments_.get());
    }
    {
      // Issues zero-query request.
      composer::Composer composer(&table, &request, config_.get());
      ConversionRequest conversion_request(&composer, &request, config_.get());
      conversion_request.set_max_conversion_candidates_size(10);
      converter_->StartPredictionForRequest(conversion_request,
                                            segments_.get());
      segments_->clear_history_segments();
    }
  } else {
    return absl::InvalidArgumentError(
        absl::StrCat("Unknown command: ", item.OutputAsTSV()));
  }

  // No results is OK if "prediction not expect" command
  if ((command == kPredictionNotExpect || command == kSuggestionNotExpect) &&
      (segments_->segments_size() == 0 ||
       (segments_->segments_size() >= 1 &&
        segments_->segment(0).candidates_size() == 0))) {
    return true;
  }

  for (size_t i = 0; i < segments_->segments_size(); ++i) {
    *actual_value += segments_->segment(i).candidate(0).value;
  }

  if (command == kConversionMatch) {
    return (actual_value->find(expected_value) != std::string::npos);
  }
  if (command == kConversionNotMatch) {
    return (actual_value->find(expected_value) == std::string::npos);
  }

  const int32_t actual_rank = GetRank(expected_value, segments_.get(), 0, 0);

  bool result = (actual_rank >= 0 && actual_rank <= expected_rank);

  if (command == kConversionNotExpect ||
      command == kReverseConversionNotExpect ||
      command == kPredictionNotExpect || command == kSuggestionNotExpect ||
      command == kZeroQueryNotExpect) {
    result = !result;
  }

  return result;
}

void QualityRegressionUtil::SetRequest(const commands::Request &request) {
  *request_ = request;
}

void QualityRegressionUtil::SetConfig(const config::Config &config) {
  *config_ = config;
}

std::string QualityRegressionUtil::GetPlatformString(
    uint32_t platform_bitfiled) {
  std::vector<std::string> v;
  if (platform_bitfiled & DESKTOP) {
    v.push_back("DESKTOP");
  }
  if (platform_bitfiled & OSS) {
    v.push_back("OSS");
  }
  if (platform_bitfiled & MOBILE) {
    v.push_back("MOBILE");
  }
  if (platform_bitfiled & MOBILE_AMBIGUOUS) {
    v.push_back("MOBILE_AMBIGUOUS");
  }
  if (v.empty()) {
    v.push_back("UNKNOWN");
  }
  return absl::StrJoin(v, "|");
}

}  // namespace quality_regression
}  // namespace mozc
