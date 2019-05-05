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

#ifndef MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
#define MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/freelist.h"
#include "base/string_piece.h"
#include "base/trie.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.pb.h"
#include "storage/lru_cache.h"
// for FRIEND_TEST
#include "testing/base/public/gunit_prod.h"

namespace mozc {

namespace storage {
class StringStorageInterface;
}  // namespace storage

class ConversionRequest;
class Segment;
class Segments;
class UserHistoryPredictorSyncer;

// Added serialization method for UserHistory.
class UserHistoryStorage : public mozc::user_history_predictor::UserHistory {
 public:
  explicit UserHistoryStorage(const string &filename);
  ~UserHistoryStorage();

  // Loads from encrypted file.
  bool Load();

  // Saves history into encrypted file.
  bool Save() const;

 private:
  std::unique_ptr<storage::StringStorageInterface> storage_;
};

// UserHistoryPredictor is NOT thread safe.
// Currently, all methods of UserHistoryPredictor is called
// by single thread. Although AsyncSave() and AsyncLoad() make
// worker threads internally, these two functions won't be
// called by multiple-threads at the same time
class UserHistoryPredictor : public PredictorInterface {
 public:
  UserHistoryPredictor(
      const dictionary::DictionaryInterface *dictionary,
      const dictionary::POSMatcher *pos_matcher,
      const dictionary::SuppressionDictionary *suppression_dictionary,
      bool enable_content_word_learning);
  ~UserHistoryPredictor() override;

  void set_content_word_learning_enabled(bool value) {
    content_word_learning_enabled_ = value;
  }

  bool Predict(Segments *segments) const;
  bool PredictForRequest(const ConversionRequest &request,
                         Segments *segments) const override;

  // Hook(s) for all mutable operations.
  void Finish(const ConversionRequest &request, Segments *segments) override;

  // Revert last Finish operation.
  void Revert(Segments *segments) override;

  // Sync user history data to local file.
  // You can call either Save() or AsyncSave().
  bool Sync() override;

  // Reloads from local disk.
  // Do not call Sync() before Reload().
  bool Reload() override;

  // Clears LRU data.
  bool ClearAllHistory() override;

  // Clears unused data.
  bool ClearUnusedHistory() override;

  // Clears a specific history entry.
  bool ClearHistoryEntry(const string &key, const string &value) override;

  // Implements PredictorInterface.
  bool Wait() override;

  // Gets user history filename.
  static string GetUserHistoryFileName();

  const string &GetPredictorName() const override { return predictor_name_; }

  // From user_history_predictor.proto
  typedef user_history_predictor::UserHistory::Entry Entry;
  typedef user_history_predictor::UserHistory::NextEntry NextEntry;
  typedef user_history_predictor::UserHistory::Entry::EntryType EntryType;

  // Returns fingerprints from various object.
  static uint32 Fingerprint(const string &key, const string &value);
  static uint32 Fingerprint(const string &key, const string &value,
                            EntryType type);
  static uint32 EntryFingerprint(const Entry &entry);
  static uint32 SegmentFingerprint(const Segment &segment);

  // Returns the size of cache.
  static uint32 cache_size();

  // Returns the size of next entries.
  static uint32 max_next_entries_size();

 private:
  struct SegmentForLearning {
    string key;
    string value;
    string content_key;
    string content_value;
    string description;
  };
  static uint32 LearningSegmentFingerprint(const SegmentForLearning &segment);

  class SegmentsForLearning {
   public:
    SegmentsForLearning() {}

    virtual ~SegmentsForLearning() {}

    void push_back_history_segment(const SegmentForLearning &val) {
      history_segments_.push_back(val);
    }

    void push_back_conversion_segment(const SegmentForLearning &val) {
      conversion_segments_.push_back(val);
    }

    size_t history_segments_size() const {
      return history_segments_.size();
    }

    size_t conversion_segments_size() const {
      return conversion_segments_.size();
    }

    size_t all_segments_size() const {
      return history_segments_size() + conversion_segments_size();
    }

    const SegmentForLearning &history_segment(size_t i) const {
      return history_segments_[i];
    }

    const SegmentForLearning &conversion_segment(size_t i) const {
      return conversion_segments_[i];
    }

    const SegmentForLearning &all_segment(size_t i) const {
      if (i < history_segments_size()) {
        return history_segment(i);
      } else {
        return conversion_segments_[i - history_segments_size()];
      }
    }

   private:
    std::vector<SegmentForLearning> history_segments_;
    std::vector<SegmentForLearning> conversion_segments_;
  };

  friend class UserHistoryPredictorSyncer;
  friend class UserHistoryPredictorTest;

  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorTest_suggestion);
  FRIEND_TEST(UserHistoryPredictorTest, DescriptionTest);
  FRIEND_TEST(UserHistoryPredictorTest,
              UserHistoryPredictorUnusedHistoryTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorRevertTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryPredictorClearTest);
  FRIEND_TEST(UserHistoryPredictorTest,
              UserHistoryPredictorTrailingPunctuation);
  FRIEND_TEST(UserHistoryPredictorTest, TrailingPunctuation_Mobile);
  FRIEND_TEST(UserHistoryPredictorTest, HistoryToPunctuation);
  FRIEND_TEST(UserHistoryPredictorTest,
              UserHistoryPredictorPreceedingPunctuation);
  FRIEND_TEST(UserHistoryPredictorTest, StartsWithPunctuations);
  FRIEND_TEST(UserHistoryPredictorTest, ZeroQuerySuggestionTest);
  FRIEND_TEST(UserHistoryPredictorTest, MultiSegmentsMultiInput);
  FRIEND_TEST(UserHistoryPredictorTest, MultiSegmentsSingleInput);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843371_Case1);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843371_Case2);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843371_Case3);
  FRIEND_TEST(UserHistoryPredictorTest, Regression2843775);
  FRIEND_TEST(UserHistoryPredictorTest, DuplicateString);
  FRIEND_TEST(UserHistoryPredictorTest, SyncTest);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeTest);
  FRIEND_TEST(UserHistoryPredictorTest, FingerPrintTest);
  FRIEND_TEST(UserHistoryPredictorTest, Uint32ToStringTest);
  FRIEND_TEST(UserHistoryPredictorTest, GetScore);
  FRIEND_TEST(UserHistoryPredictorTest, IsValidEntry);
  FRIEND_TEST(UserHistoryPredictorTest, IsValidSuggestion);
  FRIEND_TEST(UserHistoryPredictorTest, EntryPriorityQueueTest);
  FRIEND_TEST(UserHistoryPredictorTest, PrivacySensitiveTest);
  FRIEND_TEST(UserHistoryPredictorTest, PrivacySensitiveMultiSegmentsTest);
  FRIEND_TEST(UserHistoryPredictorTest, UserHistoryStorage);
  FRIEND_TEST(UserHistoryPredictorTest, RomanFuzzyPrefixMatch);
  FRIEND_TEST(UserHistoryPredictorTest, MaybeRomanMisspelledKey);
  FRIEND_TEST(UserHistoryPredictorTest, GetRomanMisspelledKey);
  FRIEND_TEST(UserHistoryPredictorTest, RomanFuzzyLookupEntry);
  FRIEND_TEST(UserHistoryPredictorTest, ExpandedLookupRoman);
  FRIEND_TEST(UserHistoryPredictorTest, ExpandedLookupKana);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeFromInputRoman);
  FRIEND_TEST(UserHistoryPredictorTest, GetMatchTypeFromInputKana);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRoman);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsRomanRandom);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsShouldNotCrash);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsFlickN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegments12KeyN);
  FRIEND_TEST(UserHistoryPredictorTest, GetInputKeyFromSegmentsKana);
  FRIEND_TEST(UserHistoryPredictorTest, RealtimeConversionInnerSegment);
  FRIEND_TEST(UserHistoryPredictorTest, ZeroQueryFromRealtimeConversion);
  FRIEND_TEST(UserHistoryPredictorTest, LongCandidateForMobile);
  FRIEND_TEST(UserHistoryPredictorTest, EraseNextEntries);
  FRIEND_TEST(UserHistoryPredictorTest, RemoveNgramChain);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Unigram);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteWhole);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteFirst);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Bigram_DeleteSecond);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteWhole);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteFirst);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteSecond);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Trigram_DeleteThird);
  FRIEND_TEST(UserHistoryPredictorTest,
              ClearHistoryEntry_Trigram_DeleteFirstBigram);
  FRIEND_TEST(UserHistoryPredictorTest,
              ClearHistoryEntry_Trigram_DeleteSecondBigram);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Scenario1);
  FRIEND_TEST(UserHistoryPredictorTest, ClearHistoryEntry_Scenario2);
  FRIEND_TEST(UserHistoryPredictorTest, JoinedSegmentsTest_Mobile);
  FRIEND_TEST(UserHistoryPredictorTest, JoinedSegmentsTest_Desktop);
  FRIEND_TEST(UserHistoryPredictorTest, UsageStats);
  FRIEND_TEST(UserHistoryPredictorTest, PunctuationLink_Mobile);
  FRIEND_TEST(UserHistoryPredictorTest, PunctuationLink_Desktop);

  enum MatchType {
    NO_MATCH,            // no match
    LEFT_PREFIX_MATCH,   // left string is a prefix of right string
    RIGHT_PREFIX_MATCH,  // right string is a prefix of left string
    LEFT_EMPTY_MATCH,    // left string is empty (for zero_query_suggestion)
    EXACT_MATCH,         // right string == left string
  };

  enum RequestType {
    DEFAULT,
    ZERO_QUERY_SUGGESTION,
  };

  // Returns value of RemoveNgramChain() method. See the comments in
  // implementation.
  enum RemoveNgramChainResult {
    DONE,
    TAIL,
    NOT_FOUND,
  };

  // Loads user history data to LRU from local file
  bool Load();

  // Saves user history data in LRU to local file
  bool Save();

  // non-blocking version of Load
  // This makes a new thread and call Load()
  bool AsyncSave();

  // non-blocking version of Sync
  // This makes a new thread and call Save()
  bool AsyncLoad();

  // Waits until syncer finishes.
  void WaitForSyncer();

  // Returns id for RevertEntry
  static uint16 revert_id();

  // Gets match type from two strings
  static MatchType GetMatchType(const string &lstr, const string &rstr);

  // Gets match type with ambiguity expansion
  static MatchType GetMatchTypeFromInput(const string &input_key,
                                         const string &key_base,
                                         const Trie<string> *key_expanded,
                                         const string &target);

  // Uint32 <=> string conversion
  static string Uint32ToString(uint32 fp);
  static uint32 StringToUint32(const string &input);

  // Returns true if prev_entry has a next_fp link to entry
  static bool HasBigramEntry(const Entry &entry,
                             const Entry &prev_entry);

  // Returns true |result_entry| can be handled as
  // a valid result if the length of user input is |prefix_len|.
  static bool IsValidSuggestion(RequestType request_type,
                                uint32 prefix_len,
                                const Entry &result_entry);

  // Returns true if entry is DEFAULT_ENTRY, satisfies certain conditions, and
  // doesn't have removed flag.
  bool IsValidEntry(const Entry &entry, uint32 available_emoji_carrier) const;
  // The same as IsValidEntry except that removed field is ignored.
  bool IsValidEntryIgnoringRemovedField(const Entry &entry,
                                        uint32 available_emoji_carrier) const;

  // Returns "tweaked" score of result_entry.
  // the score is basically determined by "last_access_time", (a.k.a,
  // LRU policy), but we want to slightly change the score
  // with different signals, including the length of value and/or
  // bigram_boost flags.
  static uint32 GetScore(const Entry &result_entry);

  // Priority Queue class for entry. New item is sorted by
  // |score| internally. By calling Pop() in sequence, you
  // can obtain the list of entry sorted by |score|.
  class EntryPriorityQueue {
   public:
    EntryPriorityQueue();
    virtual ~EntryPriorityQueue();
    size_t size() const {
      return agenda_.size();
    }
    bool Push(Entry *entry);
    Entry *Pop();
    Entry *NewEntry();

   private:
    friend class UserHistoryPredictor;
    typedef std::pair<uint32, Entry *> QueueElement;
    typedef std::priority_queue<QueueElement> Agenda;
    Agenda agenda_;
    FreeList<Entry> pool_;
    std::set<uint32> seen_;
  };

  typedef mozc::storage::LRUCache<uint32, Entry> DicCache;
  typedef DicCache::Element DicElement;

  bool CheckSyncerAndDelete() const;

  // If |entry| is the target of prediction,
  // create a new result and insert it to |results|.
  // Can set |prev_entry| if there is a history segment just before |input_key|.
  // |prev_entry| is an optional field. If set NULL, this field is just ignored.
  // This method adds a new result entry with score, pair<score, entry>, to
  // |results|.
  bool LookupEntry(RequestType request_type,
                   const string &input_key,
                   const string &key_base,
                   const Trie<string> *key_expanded,
                   const Entry *entry,
                   const Entry *prev_entry,
                   EntryPriorityQueue *results) const;

  // For the EXACT and RIGHT_PREFIX match, we will generate joined
  // candidates by looking up the history link.
  // Gets key value pair and assigns them to |result_key| and |result_value|
  // for prediction result. The last entry which was joined
  // will be assigned to |result_last_entry|.
  // Returns false if we don't have the result for this match.
  // |left_last_access_time| and |left_most_last_access_time| will be updated
  // according to the entry lookup.
  bool GetKeyValueForExactAndRightPrefixMatch(
      const string &input_key,
      const Entry *entry, const Entry **result_last_entry,
      uint64 *left_last_access_time, uint64 *left_most_last_access_time,
      string *result_key, string *result_value) const;

  const Entry *LookupPrevEntry(const Segments &segments,
                               uint32 available_emoji_carrier) const;

  // Adds an entry to a priority queue.
  Entry *AddEntry(const Entry &entry, EntryPriorityQueue *results) const;

  // Adds the entry whose key and value are modified to a priority queue.
  Entry *AddEntryWithNewKeyValue(const string &key, const string &value,
                                 const Entry &entry,
                                 EntryPriorityQueue *results) const;

  void GetResultsFromHistoryDictionary(
      RequestType request_type,
      const ConversionRequest &request,
      const Segments &segments,
      const Entry *prev_entry,
      EntryPriorityQueue *results) const;

  // Gets input data from segments.
  // These input data include ambiguities.
  static void GetInputKeyFromSegments(
      const ConversionRequest &request, const Segments &segments,
      string *input_key, string *base,
      std::unique_ptr<Trie<string>>*expanded);

  bool InsertCandidates(RequestType request_type,
                        const ConversionRequest &request, Segments *segments,
                        EntryPriorityQueue *results) const;

  void MakeLearningSegments(const Segments &segments,
                            SegmentsForLearning *learning_segments) const;

  // Returns true if |prefix| is a fuzzy-prefix of |str|.
  // 'Fuzzy' means that
  // 1) Allows one character deletation in the |prefix|.
  // 2) Allows one character swap in the |prefix|.
  static bool RomanFuzzyPrefixMatch(const string &str,
                                    const string &prefix);

  // Returns romanized preedit string if the preedit looks
  // misspelled. It first tries to get the preedit string with
  // composer() if composer is available. If not, use the key
  // directory. It also use MaybeRomanMisspelledKey() defined
  // below to check the preedit looks missspelled or not.
  static string GetRomanMisspelledKey(const ConversionRequest &request,
                                      const Segments &segments);

  // Returns true if |key| may contain miss spelling.
  // Currently, this function returns true if
  // 1) key contains only one alphabet.
  // 2) other characters of key are all hiragana.
  static bool MaybeRomanMisspelledKey(const string &key);

  // If roman_input_key can be a target key of entry->key(), creat a new
  // result and insert it to |results|.
  // This method adds a new result entry with score, pair<score, entry>, to
  // |results|.
  bool RomanFuzzyLookupEntry(
      const string &roman_input_key,
      const Entry *entry,
      EntryPriorityQueue *results) const;

  void InsertHistory(RequestType request_type,
                     bool is_suggestion_selected,
                     uint64 last_access_time,
                     Segments *segments);

  // Inserts |key,value,description| to the internal dictionary database.
  // |is_suggestion_selected|: key/value is suggestion or conversion.
  // |next_fp|: fingerprint of the next segment.
  // |last_access_time|: the time when this entrty was created
  void Insert(const string &key,
              const string &value,
              const string &description,
              bool is_suggestion_selected,
              uint32 next_fp,
              uint64 last_access_time,
              Segments *segments);

  // Tries to insert entry.
  // Entry's contents and request_type will be checked before insersion.
  void TryInsert(RequestType request_type,
                 const string &key,
                 const string &value,
                 const string &description,
                 bool is_suggestion_selected,
                 uint32 next_fp,
                 uint64 last_access_time,
                 Segments *segments);

  // Inserts event entry (CLEAN_ALL_EVENT|CLEAN_UNUSED_EVENT).
  void InsertEvent(EntryType type);

  // Inserts a new |next_entry| into |entry|.
  // it makes a bigram connection from entry to next_entry.
  void InsertNextEntry(const NextEntry &next_entry, Entry *entry) const;

  static void EraseNextEntries(uint32 fp, Entry *entry);

  // Recursively removes a chain of Entries in |dic_|. See the comment in
  // implemenetation for details.
  RemoveNgramChainResult RemoveNgramChain(
      const string &target_key, const string &target_value, Entry *entry,
      std::vector<StringPiece> *key_ngrams, size_t key_ngrams_len,
      std::vector<StringPiece> *value_ngrams, size_t value_ngrams_len);

  // Returns true if the input first candidate seems to be a privacy sensitive
  // such like password.
  bool IsPrivacySensitive(const Segments *segments) const;

  void MaybeRecordUsageStats(const Segments &segments) const;

  const dictionary::DictionaryInterface *dictionary_;
  const dictionary::POSMatcher *pos_matcher_;
  const dictionary::SuppressionDictionary *suppression_dictionary_;
  const string predictor_name_;

  bool content_word_learning_enabled_;
  bool updated_;
  std::unique_ptr<DicCache> dic_;
  mutable std::unique_ptr<UserHistoryPredictorSyncer> syncer_;
};

}  // namespace mozc

#endif  // MOZC_PREDICTION_USER_HISTORY_PREDICTOR_H_
