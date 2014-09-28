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

#ifndef MOZC_REWRITER_EMOJI_REWRITER_H_
#define MOZC_REWRITER_EMOJI_REWRITER_H_

#include <stddef.h>

#include "base/scoped_ptr.h"
#include "converter/segments.h"
#include "rewriter/embedded_dictionary.h"
#include "rewriter/rewriter_interface.h"

namespace mozc {

class ConversionRequest;

// EmojiRewriter class adds UTF-8 emoji characters in converted candidates of
// given segments, if each segment has a special key to convert.
// Added emoji characters are chosen by Yomi (reading of it) registered in
// a dictionary. If a segment has a key "えもじ", all emoji characters are
// pushed to its candidate list.
//
// Usage:
//
//   mozc::Segments segments;
//   mozc::Segment *segment = segments.add_segment();
//   mozc::Segment::Candidate *candidate = segment->add_candidate();
//   candidate->set_key("えもじ");
//
//   // Use |kEmojiDataTokenData| and |kEmojiDataTokenSize| defined in
//   // mozc/rewriter/emoji_rewriter_data.h
//   mozc::EmojiRewriter rewriter(kEmojiDataTokenData,
//                                kEmojiDataTokenSize));
//   rewriter.Rewrite(mozc::ConvresionRequest(), &segments);
//
// Here, the first segment of segments is expected to have all emoji
// characters in its candidates' values.  You can see them as such:
//
//   for (size_t i = 0; i < segment->candidate_size(); ++i) {
//     LOG(INFO) << segment->candidate(i).value;
//   }

class EmojiRewriter : public RewriterInterface {
 public:
  struct EmojiData {
    // Utf-8 representation of the unicode data.
    const char *unicode;

    // The carrier dependent emoji code point on Android.
    uint32 android_pua;

    // Descriptions depend on platforms.
    const char *description_unicode;
    const char *description_docomo;
    const char *description_softbank;
    const char *description_kddi;
  };

  struct Token {
    // The emoji reading key.
    const char *key;

    // The list of index for the emoji data.
    const uint16 *value;
    size_t value_size;
  };

  // This class does not take an ownership of |emoji_data_list|, |token_list|
  // and |value_list|.  If NULL pointer is passed to it, Mozc process
  // terminates with an error.
  EmojiRewriter(
      const EmojiData *emoji_data_list, size_t emoji_data_size,
      const Token *token_list, size_t token_size,
      const uint16 *value_list);
  virtual ~EmojiRewriter();

  virtual int capability(const ConversionRequest &request) const;

  // Returns true if emoji candidates are added.  When user settings are set
  // not to use EmojiRewriter, does nothing other than returning false.
  // Otherwise, main process are done in ReriteCandidates().
  // A reference to a ConversionRequest instance is not used, but it is required
  // because of the interface.
  virtual bool Rewrite(const ConversionRequest &request,
                       Segments *segments) const;

  // Counts the number of segments in which emoji candidates are selected,
  // and stores the result as usage stats.
  // NOTE: This method is expected to be called after the segments are processed
  // with COMMIT command in a SessionConverter instance.  May record wrong
  // stats if you call this method in other situation.
  virtual void Finish(const ConversionRequest &request, Segments *segments);

  // Returns true if the given candidate includes emoji characters.
  // TODO(peria, hidehiko): Unify this checker and IsEmojiEntry defined in
  //     predictor/user_history_predictor.cc.  If you make similar functions
  //     before the merging in case, put a same note to avoid twisted
  //     dependency.
  static bool IsEmojiCandidate(const Segment::Candidate &candidate);

 private:
  // Adds emoji candidates on each segment of given segments, if it has a
  // specific string as a key based on a dictionary.  If a segment's value is
  // "えもじ", adds all emoji candidates.
  // Returns true if emoji candidates are added in any segment.
  bool RewriteCandidates(
      int32 available_emoji_carrier, Segments *segments) const;

  const Token *LookUpToken(const string &key) const;

  // Embedded dictionary data.
  const EmojiData *emoji_data_list_;
  size_t emoji_data_size_;
  const Token *token_list_;
  size_t token_size_;
  const uint16 *value_list_;

  DISALLOW_COPY_AND_ASSIGN(EmojiRewriter);
};

}  // namespace mozc

#endif  // MOZC_REWRITER_EMOJI_REWRITER_H_
