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

#include <algorithm>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "base/config_file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/converter_interface.h"
#include "converter/segments.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "storage/lru_storage.h"
#include "usage_stats/usage_stats.h"

namespace mozc {

using storage::LRUStorage;

namespace {
const int kValueSize  = 4;
const uint32 kLRUSize = 5000;
const uint32 kSeedValue = 0x761fea81;

const char kFileName[] = "user://boundary.db";

enum { INSERT, RESIZE };

class LengthArray {
 public:
  void ToUCharArray(uint8 *array) const {
    array[0] = length0_;
    array[1] = length1_;
    array[2] = length2_;
    array[3] = length3_;
    array[4] = length4_;
    array[5] = length5_;
    array[6] = length6_;
    array[7] = length7_;
  }

  void CopyFromUCharArray(const uint8 *array) {
    length0_ = array[0];
    length1_ = array[1];
    length2_ = array[2];
    length3_ = array[3];
    length4_ = array[4];
    length5_ = array[5];
    length6_ = array[6];
    length7_ = array[7];
  }

  bool Equal(const LengthArray &r) const {
    return (length0_ == r.length0_ &&
            length1_ == r.length1_ &&
            length2_ == r.length2_ &&
            length3_ == r.length3_ &&
            length4_ == r.length4_ &&
            length5_ == r.length5_ &&
            length6_ == r.length6_ &&
            length7_ == r.length7_);
  }

 private:
  uint8 length0_ : 4;
  uint8 length1_ : 4;
  uint8 length2_ : 4;
  uint8 length3_ : 4;
  uint8 length4_ : 4;
  uint8 length5_ : 4;
  uint8 length6_ : 4;
  uint8 length7_ : 4;
};
}  // namespace

UserBoundaryHistoryRewriter::UserBoundaryHistoryRewriter(
    const ConverterInterface *parent_converter)
    : parent_converter_(parent_converter),
      storage_(new LRUStorage) {
  DCHECK(parent_converter_);
  Reload();
}

UserBoundaryHistoryRewriter::~UserBoundaryHistoryRewriter() {}

void UserBoundaryHistoryRewriter::Finish(const ConversionRequest &request,
                                         Segments *segments) {
  if (segments->request_type() != Segments::CONVERSION) {
    return;
  }

  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return;
  }

  if (GET_CONFIG(history_learning_level) !=
      config::Config::DEFAULT_HISTORY) {
    VLOG(2) << "history_learning_level is not DEFAULT_HISTORY";
    return;
  }

  if (!segments->user_history_enabled()) {
    VLOG(2) << "!user_history_enabled";
    return;
  }

  if (storage_.get() == NULL) {
    VLOG(2) << "storage is NULL";
    return;
  }

  if (segments->resized()) {
    ResizeOrInsert(segments, request, INSERT);
#ifdef OS_ANDROID
    // TODO(hidehiko): UsageStats requires some functionalities, e.g. network,
    // which are not needed for mozc's main features.
    // So, to focus on the main features' developping, we just skip it for now.
    // Note: we can #ifdef inside SetInteger, but to build it we need to build
    // other methods in usage_stats as well. So we'll exclude the method here
    // for now.
#else
    // update usage stats here
    usage_stats::UsageStats::SetInteger(
        "UserBoundaryHistoryEntrySize",
        static_cast<int>(storage_->used_size()));
#endif
  }
}

bool UserBoundaryHistoryRewriter::Rewrite(
    const ConversionRequest &request, Segments *segments) const {
  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return false;
  }

  if (GET_CONFIG(history_learning_level) == config::Config::NO_HISTORY) {
    VLOG(2) << "history_learning_level is NO_HISTORY";
    return false;
  }

  if (!segments->user_history_enabled()) {
    VLOG(2) << "!user_history_enabled";
    return false;
  }

  if (storage_.get() == NULL) {
    VLOG(2) << "storage is NULL";
    return false;
  }

  if (request.skip_slow_rewriters()) {
    return false;
  }

  if (!segments->resized()) {
    return ResizeOrInsert(segments, request, RESIZE);
  }

  return false;
}

bool UserBoundaryHistoryRewriter::Reload() {
  const string filename = ConfigFileStream::GetFileName(kFileName);
  if (!storage_->OpenOrCreate(filename.c_str(),
                              kValueSize, kLRUSize, kSeedValue)) {
    LOG(WARNING) << "cannot initialize UserBoundaryHistoryRewriter";
    storage_.reset(NULL);
    return false;
  }

  const char kFileSuffix[] = ".merge_pending";
  const string merge_pending_file = filename + kFileSuffix;

  // merge pending file does not always exist.
  if (FileUtil::FileExists(merge_pending_file)) {
    storage_->Merge(merge_pending_file.c_str());
    FileUtil::Unlink(merge_pending_file);
  }

  return true;
}

// TODO(taku): split Reize/Insert into different functions
bool UserBoundaryHistoryRewriter::ResizeOrInsert(
    Segments *segments, const ConversionRequest &request, int type) const {
  bool result = false;
  uint8 length_array[8];

  const size_t history_segments_size = segments->history_segments_size();

  // resize segments in [history_segments_size .. target_segments_size - 1]
  size_t target_segments_size = segments->segments_size();

  // when INSERTING new history,
  // Get the prefix of segments having FIXED_VALUE state.
  if (type == INSERT) {
    target_segments_size = history_segments_size;
    for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
      const Segment &segment = segments->segment(i);
      if (segment.segment_type() == Segment::FIXED_VALUE) {
        ++target_segments_size;
      }
    }
  }

  // No effective segments found
  if (target_segments_size <= history_segments_size) {
    return false;
  }

  deque<pair<string, size_t> > keys(target_segments_size -
                                    history_segments_size);
  for (size_t i = history_segments_size; i < target_segments_size; ++i) {
    const Segment &segment = segments->segment(i);
    keys[i - history_segments_size].first = segment.key();
    const size_t length = Util::CharsLen(segment.key());
    if (length > 255) {   // too long segment
      VLOG(2) << "too long segment";
      return false;
    }
    keys[i - history_segments_size].second = length;
  }

  for (size_t i = history_segments_size; i < target_segments_size; ++i) {
    const size_t kMaxKeysSize = 5;
    const size_t keys_size = min(kMaxKeysSize, keys.size());
    string key;
    memset(length_array, 0, sizeof(length_array));
    for (size_t k = 0; k < keys_size; ++k) {
      key += keys[k].first;
      length_array[k] = static_cast<uint8>(keys[k].second);
    }
    for (int j = static_cast<int>(keys_size) - 1; j >= 0; --j) {
      if (type == RESIZE) {
        const LengthArray *value =
            reinterpret_cast<const LengthArray *>(storage_->Lookup(key));
        if (value != NULL) {
          LengthArray orig_value;
          orig_value.CopyFromUCharArray(length_array);
          if (!value->Equal(orig_value)) {
            value->ToUCharArray(length_array);
            const int old_segments_size =
                static_cast<int>(target_segments_size);
            VLOG(2) << "ResizeSegment key: " << key << " "
                    << i - history_segments_size << " " << j + 1
                    << " " << static_cast<int>(length_array[0])
                    << " " << static_cast<int>(length_array[1])
                    << " " << static_cast<int>(length_array[2])
                    << " " << static_cast<int>(length_array[3])
                    << " " << static_cast<int>(length_array[4])
                    << " " << static_cast<int>(length_array[5])
                    << " " << static_cast<int>(length_array[6])
                    << " " << static_cast<int>(length_array[7]);
            parent_converter_->ResizeSegment(segments,
                                             request,
                                             i - history_segments_size,
                                             j + 1,
                                             length_array, 8);
            i += (j + target_segments_size - old_segments_size);
            result = true;
            break;
          }
        }
      } else if (type == INSERT) {
        VLOG(2) << "InserteSegment key: " << key << " "
                << i - history_segments_size << " " << j + 1
                << " " << static_cast<int>(length_array[0])
                << " " << static_cast<int>(length_array[1])
                << " " << static_cast<int>(length_array[2])
                << " " << static_cast<int>(length_array[3])
                << " " << static_cast<int>(length_array[4])
                << " " << static_cast<int>(length_array[5])
                << " " << static_cast<int>(length_array[6])
                << " " << static_cast<int>(length_array[7]);
        LengthArray inserted_value;
        inserted_value.CopyFromUCharArray(length_array);
        storage_->Insert(key, reinterpret_cast<const char *>(&inserted_value));
      }

      length_array[j] = 0;
      key.erase(key.size() - keys[j].first.size());
    }

    keys.pop_front();  // delete first item
  }

  return result;
}

void UserBoundaryHistoryRewriter::Clear() {
  if (storage_.get() != NULL) {
    VLOG(1) << "Clearing user segment data";
    storage_->Clear();
  }
}

}  // namespace mozc
