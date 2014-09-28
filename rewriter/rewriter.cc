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

#include "rewriter/rewriter.h"

#include "base/logging.h"
#include "converter/converter_interface.h"
#include "data_manager/data_manager_interface.h"
#include "dictionary/pos_group.h"
#include "dictionary/pos_matcher.h"
#include "rewriter/calculator_rewriter.h"
#include "rewriter/collocation_rewriter.h"
#include "rewriter/command_rewriter.h"
#include "rewriter/correction_rewriter.h"
#include "rewriter/date_rewriter.h"
#include "rewriter/dice_rewriter.h"
#include "rewriter/embedded_dictionary.h"
#include "rewriter/emoji_rewriter.h"
#include "rewriter/emoticon_rewriter.h"
#include "rewriter/english_variants_rewriter.h"
#include "rewriter/focus_candidate_rewriter.h"
#include "rewriter/fortune_rewriter.h"
#include "rewriter/language_aware_rewriter.h"
#include "rewriter/merger_rewriter.h"
#include "rewriter/normalization_rewriter.h"
#include "rewriter/number_rewriter.h"
#include "rewriter/remove_redundant_candidate_rewriter.h"
#include "rewriter/rewriter_interface.h"
#include "rewriter/single_kanji_rewriter.h"
#include "rewriter/symbol_rewriter.h"
#include "rewriter/transliteration_rewriter.h"
#include "rewriter/unicode_rewriter.h"
#include "rewriter/user_boundary_history_rewriter.h"
#include "rewriter/user_dictionary_rewriter.h"
#include "rewriter/user_segment_history_rewriter.h"
#include "rewriter/variants_rewriter.h"
#include "rewriter/version_rewriter.h"
#include "rewriter/zipcode_rewriter.h"
#ifndef NO_USAGE_REWRITER
#include "rewriter/usage_rewriter.h"
#endif  // NO_USAGE_REWRITER

DEFINE_bool(use_history_rewriter, true, "Use history rewriter or not.");

namespace {
// When updating the emoji dictionary,
// 1. Edit mozc/data/emoji/emoji_data.tsv,
// 2. Run gen_emoji_rewriter_data.py and make emoji_rewriter_data.h,
// 3. Make sure generated emoji_rewriter_data.h is correct.

// This generated header file defines |kEmojiDataList|, |kEmojiValueList|
// and |kEmojiTokenList|.
#include "rewriter/emoji_rewriter_data.h"
}  // namespace

namespace mozc {

RewriterImpl::RewriterImpl(const ConverterInterface *parent_converter,
                           const DataManagerInterface *data_manager,
                           const PosGroup *pos_group,
                           const DictionaryInterface *dictionary) {
  DCHECK(parent_converter);
  DCHECK(data_manager);
  DCHECK(pos_group);
  const POSMatcher *pos_matcher = data_manager->GetPOSMatcher();
  DCHECK(pos_matcher);
  // |dictionary| can be NULL

  AddRewriter(new UserDictionaryRewriter);
  AddRewriter(new FocusCandidateRewriter(data_manager));
  AddRewriter(new LanguageAwareRewriter(*pos_matcher, dictionary));
  AddRewriter(new TransliterationRewriter(*pos_matcher));
  AddRewriter(new EnglishVariantsRewriter);
  AddRewriter(new NumberRewriter(data_manager));
  AddRewriter(new CollocationRewriter(data_manager));
  AddRewriter(new SingleKanjiRewriter(*pos_matcher));
  AddRewriter(new EmojiRewriter(
      kEmojiDataList, arraysize(kEmojiDataList),
      kEmojiTokenList, arraysize(kEmojiTokenList),
      kEmojiValueList));
  AddRewriter(new EmoticonRewriter);
  AddRewriter(new CalculatorRewriter(parent_converter));
  AddRewriter(new SymbolRewriter(parent_converter, data_manager));
  AddRewriter(new UnicodeRewriter(parent_converter));
  AddRewriter(new VariantsRewriter(pos_matcher));
  AddRewriter(new ZipcodeRewriter(pos_matcher));
  AddRewriter(new DiceRewriter);

  if (FLAGS_use_history_rewriter) {
    AddRewriter(new UserBoundaryHistoryRewriter(parent_converter));
    AddRewriter(new UserSegmentHistoryRewriter(pos_matcher, pos_group));
  }

  AddRewriter(new DateRewriter);
  AddRewriter(new FortuneRewriter);
#ifndef OS_ANDROID
  // CommandRewriter is not tested well on Android.
  // So we temporarily disable it.
  // TODO(yukawa, team): Enable CommandRewriter on Android if necessary.
  AddRewriter(new CommandRewriter);
#endif  // OS_ANDROID
#ifndef NO_USAGE_REWRITER
  AddRewriter(new UsageRewriter(data_manager, dictionary));
#endif  // NO_USAGE_REWRITER

  AddRewriter(new VersionRewriter);
  AddRewriter(CorrectionRewriter::CreateCorrectionRewriter(data_manager));
  AddRewriter(new NormalizationRewriter);
  AddRewriter(new RemoveRedundantCandidateRewriter);
}

}  // namespace mozc
