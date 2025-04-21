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

// A class handling the converter on the session layer.

#ifndef MOZC_SESSION_SESSION_CONVERTER_H_
#define MOZC_SESSION_SESSION_CONVERTER_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/port.h"
#include "session/session_converter_interface.h"

namespace mozc {
namespace commands {
class CandidateList;
class Candidates;
class Context;
class Output;
class Preedit;
class Request;
class Result;
}  // namespace commands

namespace config {
class Config;
}  // namespace config

namespace session {
class CandidateList;

// Class handling ConverterInterface with a session state.  This class
// support stateful operations related with the converter.
class SessionConverter : public SessionConverterInterface {
 public:
  SessionConverter(const ConverterInterface *converter,
                   const commands::Request *request,
                   const config::Config *config);
  ~SessionConverter() override;

  // Checks if the current state is in the state bitmap.
  bool CheckState(States) const override;

  // Indicates if the conversion session is active or not.  In general,
  // Convert functions make it active and Cancel, Reset and Commit
  // functions make it deactive.
  bool IsActive() const override;

  // Returns the default conversion preferences to be used for custom
  // conversion.
  const ConversionPreferences &conversion_preferences() const override;

  // Gets the selected candidate. If no candidate is selected, returns nullptr.
  const Segment::Candidate *GetSelectedCandidateOfFocusedSegment()
      const override;

  // Gets the candidate specified by id.
  const Segment::Candidate *GetCandidateById(int id) const override;

  // Sends a conversion request to the converter.
  bool Convert(const composer::Composer &composer) override;
  bool ConvertWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) override;

  // Gets reading text (e.g. from "猫" to "ねこ").
  bool GetReadingText(const std::string &source_text,
                      std::string *reading) override;

  // Sends a transliteration request to the converter.
  bool ConvertToTransliteration(
      const composer::Composer &composer,
      transliteration::TransliterationType type) override;

  // Converts the current composition to half-width characters.
  // NOTE(komatsu): This function might be merged to ConvertToTransliteration.
  bool ConvertToHalfWidth(const composer::Composer &composer) override;

  // Switches the composition to Hiragana, full-width Katakana or
  // half-width Katakana by rotation.
  bool SwitchKanaType(const composer::Composer &composer) override;

  // Sends a suggestion request to the converter.
  bool Suggest(const composer::Composer &composer) override;
  bool SuggestWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) override;

  // Sends a prediction request to the converter.
  bool Predict(const composer::Composer &composer) override;
  bool PredictWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) override;

  // Sends a prediction request to the converter.
  // The result is added at the tail of existing candidate list as "suggestion"
  // candidates.
  bool ExpandSuggestion(const composer::Composer &composer) override;
  bool ExpandSuggestionWithPreferences(
      const composer::Composer &composer,
      const ConversionPreferences &preferences) override;

  // Clears conversion segments, but keep the context.
  void Cancel() override;

  // Clears conversion segments and the context.
  void Reset() override;

  // Fixes the conversion with the current status.
  void Commit(const composer::Composer &composer,
              const commands::Context &context) override;

  // Fixes the suggestion candidate. Stores the number of characters in the key
  // of the committed candidate to committed_key_size.
  // For example, assume that "日本語" was suggested as a candidate for "にほ".
  // The key of the candidate is "にほんご". Therefore, when the candidate is
  // committed by this function, consumed_key_size will be set to 4.
  // |consumed_key_size| can be very large value
  // (i.e. SessionConverter::kConsumedAllCharacters).
  // In this case all the composition characters are consumed.
  // Examples:
  // - {composition: "にほんご|", value: "日本語", consumed_key_size: 4}
  //   This is usual case of suggestion/composition.
  // - {composition: "にほんg|", value: "日本語",
  //    consumed_key_size: kConsumedAllCharacters}
  //   kConsumedAllCharacters can be very large value.
  // - {composition: "わた|しのなまえ", value: "綿",
  //    consumed_key_size: 2}
  //   (Non-Auto) Partial suggestion.
  //   New composition is "しのなまえ".
  // - {composition: "わたしのなまえ|", value: "私の",
  //    consumed_key_size: 4}
  //   Auto partial suggestion.
  //   Consumed the composition partially and "なまえ" becomes composition.
  // - {composition: "じゅえり", value: "クエリ",
  //    consumed_key_size: 4}
  //   Typing correction.
  //   The value クエリ corresponds to raw composition "じゅえり".
  // True is returned if the suggestion is successfully committed.
  bool CommitSuggestionByIndex(size_t index, const composer::Composer &composer,
                               const commands::Context &context,
                               size_t *consumed_key_size) override;

  // Selects a candidate and commit the selected candidate.  True is
  // returned if the suggestion is successfully committed.
  // c.f. CommitSuggestionInternal
  bool CommitSuggestionById(int id, const composer::Composer &composer,
                            const commands::Context &context,
                            size_t *consumed_key_size) override;

  // Fixes only the conversion of the first segment, and keep the rest.
  // The caller should delete characters from composer based on returned
  // |committed_key_size|.
  // If |consumed_key_size| is 0, this means that there is only a segment
  // so Commit() method is called instead. In this case, the caller
  // should not delete any characters.
  // c.f. CommitSuggestionInternal
  void CommitFirstSegment(const composer::Composer &composer,
                          const commands::Context &context,
                          size_t *consumed_key_size) override;

  // Does allmost the same thing as CommitFirstSegment.
  // The only difference is to fix the segments from the head to the focused.
  void CommitHeadToFocusedSegments(const composer::Composer &composer,
                                   const commands::Context &context,
                                   size_t *consumed_key_size) override;

  // Commits the preedit string represented by Composer.
  void CommitPreedit(const composer::Composer &composer,
                     const commands::Context &context) override;

  // Commits the specified number of characters at the head of the preedit
  // string represented by Composer.
  // The caller should delete characters from composer based on returned
  // |consumed_key_size|.
  // c.f. CommitSuggestionInternal
  // TODO(yamaguchi): Enhance to support the conversion mode.
  void CommitHead(size_t count, const composer::Composer &composer,
                  size_t *consumed_key_size) override;

  // Reverts the last "Commit" operation
  void Revert() override;

  // Moves the focus of segments.
  void SegmentFocusRight() override;
  void SegmentFocusLast() override;
  void SegmentFocusLeft() override;
  void SegmentFocusLeftEdge() override;

  // Resizes the focused segment.
  void SegmentWidthExpand(const composer::Composer &composer) override;
  void SegmentWidthShrink(const composer::Composer &composer) override;

  // Moves the focus of candidates.
  void CandidateNext(const composer::Composer &composer) override;
  void CandidateNextPage() override;
  void CandidatePrev() override;
  void CandidatePrevPage() override;
  // Moves the focus to the candidate represented by the id.
  void CandidateMoveToId(int id, const composer::Composer &composer) override;
  // Moves the focus to the index from the beginning of the current page.
  void CandidateMoveToPageIndex(size_t index) override;
  // Moves the focus to the candidate represented by the shortcut.  If
  // the shortcut is not bound with any candidate, false is returned.
  bool CandidateMoveToShortcut(char shortcut) override;

  // Operation for the candidate list.
  void SetCandidateListVisible(bool visible) override;

  // Fills protocol buffers and update the internal status.
  void PopOutput(const composer::Composer &composer,
                 commands::Output *output) override;

  // Fills protocol buffers
  void FillOutput(const composer::Composer &composer,
                  commands::Output *output) const override;

  // Sets setting by the request;
  void SetRequest(const commands::Request *request) override;

  // Sets setting by the config;
  void SetConfig(const config::Config *config) override;

  // Set setting by the context.
  void OnStartComposition(const commands::Context &context) override;

  // Fills conversion request and segments with the conversion preferences.
  static void SetConversionPreferences(const ConversionPreferences &preferences,
                                       Segments *segments,
                                       ConversionRequest *request);

  // Copies SessionConverter
  // TODO(hsumita): Copy all member variables.
  // Currently, converter_ is not copied.
  SessionConverter *Clone() const override;

  // Fills protocol buffers with all flatten candidate words.
  void FillAllCandidateWords(commands::CandidateList *candidates) const;

  void set_selection_shortcut(
      config::Config::SelectionShortcut selection_shortcut) override {
    selection_shortcut_ = selection_shortcut;
  }

  void set_use_cascading_window(bool use_cascading_window) override {
    use_cascading_window_ = use_cascading_window;
  }

  // Meaning that all the composition characters are consumed.
  // c.f. CommitSuggestionInternal
  static const size_t kConsumedAllCharacters;

 private:
  friend class SessionConverterTest;

  // Resets the result value stored at the previous command.
  void ResetResult();

  // Resets the session state variables.
  void ResetState();

  // Notifies the converter that the current segment is focused.
  void SegmentFocus();

  // Notifies the converter that the current segment is fixed.
  void SegmentFix();

  // Fixes the conversion of the [0, segments_to_commit -1 ] segments,
  // and keep the rest.
  // Internal implementation for CommitFirstSegment and
  // CommitHeadToFocusedSegment.
  void CommitSegmentsInternal(const composer::Composer &composer,
                              const commands::Context &context,
                              size_t segments_to_commit,
                              size_t *consumed_key_size);

  // Gets preedit from segment(index) to segment(index + size).
  void GetPreedit(size_t index, size_t size, std::string *preedit) const;
  // Gets conversion from segment(index) to segment(index + size).
  void GetConversion(size_t index, size_t size, std::string *conversion) const;
  // Gets consumed size of the preedit characters.
  // c.f. CommitSuggestionInternal
  size_t GetConsumedPreeditSize(const size_t index, size_t size) const;

  // Performs the command if the command candidate is selected.  True
  // is returned if a command is performed.
  bool MaybePerformCommandCandidate(size_t index, size_t size);

  // Updates internal states
  bool UpdateResult(size_t index, size_t size, size_t *consumed_key_size);

  // Fills the candidate list with the focused segment's candidates.
  // This method does not clear the candidate list before processing.
  // Only the candidates of which id is not existent in the candidate list
  // are appended. Other candidates are ignored.
  void AppendCandidateList();
  // Clears the candidate list and fill it with the focused segment's
  // candidates.
  void UpdateCandidateList();

  // Returns the candidate index to be used by the converter.
  int GetCandidateIndexForConverter(const size_t segment_index) const;

  // If focus_id is pointing to the last of suggestions,
  // call StartPrediction().
  void MaybeExpandPrediction(const composer::Composer &composer);

  // Returns the value of candidate to be used by the converter.
  std::string GetSelectedCandidateValue(size_t segment_index) const;

  // Returns the candidate to be used by the converter.
  const Segment::Candidate &GetSelectedCandidate(size_t segment_index) const;

  // Returns the length of committed candidate's key in characters.
  // True is returned if the selected candidate is successfully committed.
  bool CommitSuggestionInternal(const composer::Composer &composer,
                                const commands::Context &context,
                                size_t *consumed_key_size);

  void SegmentFocusInternal(size_t segment_index);
  void ResizeSegmentWidth(const composer::Composer &composer, int delta);

  void FillConversion(commands::Preedit *preedit) const;
  void FillResult(commands::Result *result) const;
  void FillCandidates(commands::Candidates *candidates) const;

  bool IsEmptySegment(const Segment &segment) const;

  // Handles selected_indices for usage stats.
  void InitializeSelectedCandidateIndices();
  void UpdateSelectedCandidateIndex();
  void UpdateCandidateStats(const std::string &base_name, int32_t index);
  void CommitUsageStats(SessionConverterInterface::State commit_state,
                        const commands::Context &context);
  void CommitUsageStatsWithSegmentsSize(
      SessionConverterInterface::State commit_state,
      const commands::Context &context, size_t commit_segments_size);

  void SetRequestType(ConversionRequest::RequestType request_type,
                      ConversionRequest *conversion_request);

  const ConverterInterface *converter_;
  std::unique_ptr<Segments> segments_;
  size_t segment_index_;

  // Previous suggestions to be merged with the current predictions.
  Segment previous_suggestions_;

  std::unique_ptr<commands::Result> result_;

  std::unique_ptr<CandidateList> candidate_list_;

  const commands::Request *request_;
  const config::Config *config_;

  SessionConverterInterface::State state_;

  // Remembers request type to manage state.
  // TODO(team): Check whether we can switch behaviors using state_
  // instead of request_type_.
  ConversionRequest::RequestType request_type_;

  // Default conversion preferences.
  ConversionPreferences conversion_preferences_;

  config::Config::SelectionShortcut selection_shortcut_;

  // Selected index data of each segments for usage stats.
  std::vector<int> selected_candidate_indices_;

  // Indicates whether config_ will be updated by the command candidate.
  Segment::Candidate::Command updated_command_;

  // Revision number of client context with which the converter determines when
  // the history segments should be invalidated. See the implementation of
  // OnStartComposition for details.
  int32_t client_revision_;

  bool candidate_list_visible_;

  // Mutable values of |config_|.  These values may be changed temporaliry per
  // session.
  bool use_cascading_window_;

  DISALLOW_COPY_AND_ASSIGN(SessionConverter);
};

}  // namespace session
}  // namespace mozc

#endif  // MOZC_SESSION_SESSION_CONVERTER_H_
