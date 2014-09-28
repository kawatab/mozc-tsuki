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

#include "prediction/user_history_predictor.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <string>

#include "base/config_file_stream.h"
#include "base/flags.h"
#include "base/init.h"
#include "base/logging.h"
#include "base/thread.h"
#include "base/trie.h"
#include "base/util.h"
#include "composer/composer.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "converter/conversion_request.h"
#include "converter/segments.h"
#include "dictionary/dictionary_interface.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "prediction/predictor_interface.h"
#include "prediction/user_history_predictor.pb.h"
#include "rewriter/variants_rewriter.h"
#include "session/commands.pb.h"
#include "storage/encrypted_string_storage.h"
#include "storage/lru_cache.h"
#include "usage_stats/usage_stats.h"


// This flag is set by predictor.cc
// We can remove this after the ambiguity expansion feature get stable.
DEFINE_bool(enable_expansion_for_user_history_predictor,
            false,
            "enable ambiguity expansion for user_history_predictor.");

namespace mozc {

using commands::Request;

namespace {
// find suggestion candidates from the most recent 3000 history in LRU.
// We don't check all history, since suggestion is called every key event
const size_t kMaxSuggestionTrial = 3000;

// find suffix matches of history_segments from the most recent 500 histories
// in LRU.
const size_t kMaxPrevValueTrial = 500;

// cache size
// Typically memory/storage footprint becomes kLRUCacheSize * 70 bytes.
#ifdef OS_ANDROID
const size_t kLRUCacheSize = 2000;
#else  // OS_ANDROID
const size_t kLRUCacheSize = 10000;
#endif  // OS_ANDROID

// don't save key/value that are
// longer than kMaxCandidateSize to avoid memory explosion
const size_t kMaxStringLength = 256;

// maximum size of next_entries
const size_t kMaxNextEntriesSize = 4;

// revert id for user_history_predictor
const uint16 kRevertId = 1;

// default object pool size for EntryPriorityQueue
const size_t kEntryPoolSize = 16;

// file name for the history
#ifdef OS_WIN
const char kFileName[] = "user://history.db";
#else
const char kFileName[] = "user://.history.db";
#endif

// use '\t' as a key/value delimiter
const char kDelimiter[] = "\t";

// "絵文字"
const char kEmojiDescription[] = "\xE7\xB5\xB5\xE6\x96\x87\xE5\xAD\x97";

// TODO(peria, hidehiko): Unify this checker and IsEmojiCandidate in
//     EmojiRewriter.  If you make similar functions before the merging in
//     case, put a similar note to avoid twisted dependency.
bool IsEmojiEntry(const UserHistoryPredictor::Entry &entry) {
  return (entry.has_description() &&
          entry.description().find(kEmojiDescription) != string::npos);
}

bool IsPunctuation(const string &value) {
  //  return (value == "。" || value == "." ||
  //          value == "、" || value == "," ||
  //          value == "？" || value == "?" ||
  //          value == "！" || value == "!" ||
  //          value == "，" || value == "．");
  return (value == "\xE3\x80\x82" || value == "." ||
          value == "\xE3\x80\x81" || value == "," ||
          value == "\xEF\xBC\x9F" || value == "?" ||
          value == "\xEF\xBC\x81" || value == "!" ||
          value == "\xEF\xBC\x8C" || value == "\xEF\xBC\x8E");
}

// Return romanaized string.
string ToRoman(const string &str) {
  string result;
  Util::HiraganaToRomanji(str, &result);
  return result;
}

// return true if value looks like a content word.
// Currently, just checks the script type.
bool IsContentWord(const string &value) {
  return Util::CharsLen(value) > 1 ||
      Util::GetScriptType(value) != Util::UNKNOWN_SCRIPT;
}

// Return candidate description.
// If candidate is spelling correction, typing correction
// or auto partial suggestion,
// don't use the description, since "did you mean" like description must be
// provided at an appropriate timing and context.
string GetDescription(const Segment::Candidate &candidate) {
  if (candidate.attributes &
      (Segment::Candidate::SPELLING_CORRECTION |
       Segment::Candidate::TYPING_CORRECTION |
       Segment::Candidate::AUTO_PARTIAL_SUGGESTION)) {
    return "";
  }
  return candidate.description;
}

}  // namespace

// Returns true if the input first candidate seems to be a privacy sensitive
// such like password.
bool UserHistoryPredictor::IsPrivacySensitive(const Segments *segments) const {
  const bool kNonSensitive = false;
  const bool kSensitive = true;

  // Skip privacy sensitive check if |segments| consists of multiple conversion
  // segment. That is, segments like "パスワードは|x7LAGhaR" where '|'
  // represents segment boundary is not considered to be privacy sensitive.
  // TODO(team): Revisit this rule if necessary.
  if (segments->conversion_segments_size() != 1) {
    return kNonSensitive;
  }

  // Hereafter, we must have only one conversion segment.
  const Segment &conversion_segment = segments->conversion_segment(0);
  const string &segment_key = conversion_segment.key();

  // The top candidate, which is about to be commited.
  const Segment::Candidate &candidate = conversion_segment.candidate(0);
  const string &candidate_value = candidate.value;

  // If |candidate_value| contains any non-ASCII character, do not treat
  // it as privacy sensitive information.
  // TODO(team): Improve the following rule. For example,
  //     "0000－0000－0000－0000" is not treated as privacy sensitive
  //     because of this rule. When a user commits his password in
  //     full-width form by mistake, like "ｘ７ＬＡＧｈａＲ", it is not
  //     treated as privacy sensitive too.
  if (Util::GetCharacterSet(candidate_value) != Util::ASCII) {
    return kNonSensitive;
  }

  // Hereafter, |candidate_value| consists of ASCII characters only.

  // Note: if the key looks like hiragana, the candidate might be Katakana to
  // English transliteration. Don't suppress transliterated candidates.
  // http://b/4394325

  // If the key consists of number characters only, treat it as privacy
  // sensitive.
  if (Util::GetScriptType(segment_key) == Util::NUMBER) {
    return kSensitive;
  }

  // If the key contains any alphabetical character but it is in our dictionary,
  // it can be treated as privacy nonsensitive word; cf. b/5995529. Besides,
  // short words would be considered as privacy nonsensitive word as well.
  if (segment_key.size() <= 3) {
    return kNonSensitive;
  }

  // Dictionary-based sensitivity test. If the word user typed is in dictionary,
  // treat it as privacy insensitive. For English (ASCII) words,
  // dictionary-based test is extended to the following forms:
  //   1) All lower case (e.g., hello)
  //   2) All upper case (e.g., HELLO)
  //   3) Capitalized (e.g., Hello)
  //   4) As-is (e.g., HeLlO)
  // Since English words are stored in lower case, in case of upper case and
  // capitalized keys, we convert it to lower case in advance.
  if (Util::IsUpperOrCapitalizedAscii(candidate_value)) {
    // Look up for keys that are all in upper case or capitalized ASCII.
    string lower_case_value(candidate_value);
    Util::LowerString(&lower_case_value);
    if (dictionary_->HasValue(lower_case_value)) {
      return kNonSensitive;
    }
  } else {
    // Look up for the original key, including those that are all in lower case
    // ASCII.
    if (dictionary_->HasValue(candidate_value)) {
      return kNonSensitive;
    }
  }
  // If the key contains any alphabetical character and is not in our
  // dictionary, treat it as privacy sensitive. There also remains some cases to
  // be considered. Compare following two cases.
  //   Case A:
  //     1. Type "ywwz1sxm" in Roman-input style then get "yっwz1sxm".
  //     2. Hit F10 key to convert it to "ywwz1sxm" by
  //        ConvertToHalfAlphanumeric command.
  //     3. Commit it.
  //     In this case, |segment_key| is "yっwz1sxm" and actually contains
  //     alphabetical characters. So kSensitive will be returned.
  //     So far so good.
  //   Case B:
  //     1. type "ia1bo3xu" in Roman-input style then get "いあ1ぼ3ぅ".
  //     2. hit F10 key to convert it to "ia1bo3xu" by
  //        ConvertToHalfAlphanumeric command.
  //     3. commit it.
  //     In this case, |segment_key| is "ia1bo3xu" and contains no
  //     alphabetical character. So the following check does nothing.
  // TODO(team): Improve the following rule so that our user experience
  //     can be consistent between case A and B.
  if (Util::ContainsScriptType(segment_key, Util::ALPHABET)) {
    return kSensitive;
  }

  return kNonSensitive;
}

UserHistoryStorage::UserHistoryStorage(const string &filename)
    : storage_(new storage::EncryptedStringStorage(filename)) {
}

UserHistoryStorage::~UserHistoryStorage() {}

bool UserHistoryStorage::Load() {
  string input;
  if (!storage_->Load(&input)) {
    LOG(ERROR) << "Can't load user history data.";
    return false;
  }

  if (!ParseFromString(input)) {
    LOG(ERROR) << "ParseFromString failed. message looks broken";
    return false;
  }

  VLOG(1) << "Loaded user histroy, size=" << entries_size();
  return true;
}

bool UserHistoryStorage::Save() const {
  if (entries_size() == 0) {
    LOG(WARNING) << "etries size is 0. Not saved";
    return false;
  }

  string output;
  if (!AppendToString(&output)) {
    LOG(ERROR) << "AppendToString failed";
    return false;
  }

  if (!storage_->Save(output)) {
    LOG(ERROR) << "Can't save user history data.";
    return false;
  }

  return true;
}

UserHistoryPredictor::EntryPriorityQueue::EntryPriorityQueue()
    : pool_(kEntryPoolSize) {}

UserHistoryPredictor::EntryPriorityQueue::~EntryPriorityQueue() {}

bool UserHistoryPredictor::EntryPriorityQueue::Push(Entry *entry) {
  DCHECK(entry);
  if (!seen_.insert(Util::Fingerprint32(entry->value())).second) {
    VLOG(2) << "found dups";
    return false;
  }
  const uint32 score = UserHistoryPredictor::GetScore(*entry);
  agenda_.push(make_pair(score, entry));
  return true;
}

UserHistoryPredictor::Entry *
UserHistoryPredictor::EntryPriorityQueue::Pop() {
  if (agenda_.empty()) {
    return NULL;
  }
  const QueueElement &element = agenda_.top();
  Entry *result = element.second;
  DCHECK(result);
  agenda_.pop();
  return result;
}

UserHistoryPredictor::Entry *
UserHistoryPredictor::EntryPriorityQueue::NewEntry() {
  return pool_.Alloc();
}

class UserHistoryPredictorSyncer : public Thread {
 public:
  enum RequestType {
    LOAD,
    SAVE
  };

  UserHistoryPredictorSyncer(UserHistoryPredictor *predictor,
                             RequestType type)
      : predictor_(predictor), type_(type) {
    DCHECK(predictor_);
  }

  virtual void Run() {
    switch (type_) {
      case LOAD:
        VLOG(1) << "Executing Reload method";
        predictor_->Load();
        break;
      case SAVE:
        VLOG(1) << "Executing Sync method";
        predictor_->Save();
        break;
      default:
        LOG(ERROR) << "Unknown request: " << static_cast<int>(type_);
    }
  }

  virtual ~UserHistoryPredictorSyncer() {
    Join();
  }

  UserHistoryPredictor *predictor_;
  RequestType type_;
};

UserHistoryPredictor::UserHistoryPredictor(
    const DictionaryInterface *dictionary,
    const POSMatcher *pos_matcher,
    const SuppressionDictionary *suppression_dictionary)
    : dictionary_(dictionary),
      pos_matcher_(pos_matcher),
      suppression_dictionary_(suppression_dictionary),
      predictor_name_("UserHistoryPredictor"),
      updated_(false),
      dic_(new DicCache(UserHistoryPredictor::cache_size())) {
  AsyncLoad();  // non-blocking
  // Load()  blocking version can be used if any
}

UserHistoryPredictor::~UserHistoryPredictor() {
  // In destructor, must call blocking version
  WaitForSyncer();
  Save();   // blocking
}

string UserHistoryPredictor::GetUserHistoryFileName() {
  return ConfigFileStream::GetFileName(kFileName);
}

// return revert id
// static
uint16 UserHistoryPredictor::revert_id() {
  return kRevertId;
}

void UserHistoryPredictor::WaitForSyncer() {
  if (syncer_.get() != NULL) {
    syncer_->Join();
    syncer_.reset(NULL);
  }
}

bool UserHistoryPredictor::WaitForSyncerForTest() {
  WaitForSyncer();
  return true;
}

bool UserHistoryPredictor::CheckSyncerAndDelete() const {
  if (syncer_.get() != NULL) {
    if (syncer_->IsRunning()) {
      return false;
    } else {
      syncer_.reset(NULL);  // remove
    }
  }

  return true;
}

bool UserHistoryPredictor::Sync() {
  return AsyncSave();
  // return Save();   blocking version
}

bool UserHistoryPredictor::Reload() {
  WaitForSyncer();
  return AsyncLoad();
}

bool UserHistoryPredictor::AsyncLoad() {
  if (!CheckSyncerAndDelete()) {  // now loading/saving
    return true;
  }

  syncer_.reset(new UserHistoryPredictorSyncer(
      this,
      UserHistoryPredictorSyncer::LOAD));
  syncer_->Start();

  return true;
}

bool UserHistoryPredictor::AsyncSave() {
  if (!updated_) {
    return true;
  }

  if (!CheckSyncerAndDelete()) {  // now loading/saving
    return true;
  }

  syncer_.reset(new UserHistoryPredictorSyncer(
      this,
      UserHistoryPredictorSyncer::SAVE));
  syncer_->Start();

  return true;
}

bool UserHistoryPredictor::Load() {
  const string filename = GetUserHistoryFileName();

  UserHistoryStorage history(filename);
  if (!history.Load()) {
    LOG(ERROR) << "UserHistoryStorage::Load() failed";
    return false;
  }

  for (size_t i = 0; i < history.entries_size(); ++i) {
    dic_->Insert(EntryFingerprint(history.entries(i)),
                 history.entries(i));
  }

  VLOG(1) << "Loaded user histroy, size=" << history.entries_size();

  return true;
}

bool UserHistoryPredictor::Save() {
  if (!updated_) {
    return true;
  }

  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return true;
  }

  if (!GET_CONFIG(use_history_suggest)) {
    VLOG(2) << "no history suggest";
    return true;
  }

  const DicElement *tail = dic_->Tail();
  if (tail == NULL) {
    return true;
  }

  const string filename = GetUserHistoryFileName();

  UserHistoryStorage history(filename);
  for (const DicElement *elm = tail; elm != NULL; elm = elm->prev) {
    history.add_entries()->CopyFrom(elm->value);
  }

  // update usage stats here.
  usage_stats::UsageStats::SetInteger(
      "UserHistoryPredictorEntrySize",
      static_cast<int>(history.entries_size()));

  if (!history.Save()) {
    LOG(ERROR) << "UserHistoryStorage::Save() failed";
    return false;
  }

  updated_ = false;

  return true;
}

bool UserHistoryPredictor::ClearAllHistory() {
  // Wait until syncer finishes
  WaitForSyncer();

  VLOG(1) << "Clearing user prediction";
  // renew DicCache as LRUCache tries to reuse the internal value by
  // using FreeList
  dic_.reset(new DicCache(UserHistoryPredictor::cache_size()));

  // insert a dummy event entry.
  InsertEvent(Entry::CLEAN_ALL_EVENT);

  updated_ = true;

  Sync();

  return true;
}

bool UserHistoryPredictor::ClearUnusedHistory() {
  // Wait until syncer finishes
  WaitForSyncer();

  VLOG(1) << "Clearing unused prediction";
  const DicElement *head = dic_->Head();
  if (head == NULL) {
    VLOG(2) << "dic head is NULL";
    return false;
  }

  vector<uint32> keys;
  for (const DicElement *elm = head; elm != NULL; elm = elm->next) {
    VLOG(3) << elm->key << " " << elm->value.suggestion_freq();
    if (elm->value.suggestion_freq() == 0) {
      keys.push_back(elm->key);
    }
  }

  for (size_t i = 0; i < keys.size(); ++i) {
    VLOG(2) << "Removing: " << keys[i];
    if (!dic_->Erase(keys[i])) {
      LOG(ERROR) << "cannot erase " << keys[i];
    }
  }

  // insert a dummy event entry.
  InsertEvent(Entry::CLEAN_UNUSED_EVENT);

  updated_ = true;

  Sync();

  VLOG(1) << keys.size() << " removed";

  return true;
}

// Erases all the next_entries whose entry_fp field equals |fp|.
void UserHistoryPredictor::EraseNextEntries(uint32 fp, Entry *entry) {
  const size_t orig_size = entry->next_entries_size();
  size_t new_size = orig_size;
  for (size_t pos = 0; pos < new_size; ) {
    if (entry->next_entries(pos).entry_fp() == fp) {
      entry->mutable_next_entries()->SwapElements(pos, --new_size);
    } else {
      ++pos;
    }
  }
  for (size_t i = 0; i < orig_size - new_size; ++i) {
    entry->mutable_next_entries()->RemoveLast();
  }
}

// Recursively finds the Ngram history that produces |target_key| and
// |target_value| and removes the last link. For example, if there exists a
// chain like
//    ("aaa", "AAA") -- ("bbb", "BBB") -- ("ccc", "CCC"),
// and if target_key == "aaabbbccc" and target_value == "AAABBBCCC", the link
// from ("bbb", "BBB") to ("ccc", "CCC") is removed. If a link was removed, this
// method returns DONE. If no history entries can produce the target key
// value, then NOT_FOUND is returned. TAIL is returned only when the
// tail was found, e.g., in the above example, when the method finds the tail
// node ("ccc", "CCC").
UserHistoryPredictor::RemoveNgramChainResult
UserHistoryPredictor::RemoveNgramChain(const string &target_key,
                                       const string &target_value,
                                       Entry *entry,
                                       vector<StringPiece> *key_ngrams,
                                       size_t key_ngrams_len,
                                       vector<StringPiece> *value_ngrams,
                                       size_t value_ngrams_len) {
  DCHECK(entry);
  DCHECK(key_ngrams);
  DCHECK(value_ngrams);

  // Update the lengths with the current entry node.
  key_ngrams_len += entry->key().size();
  value_ngrams_len += entry->value().size();

  // This is the case where ngram key and value are shorter than the target key
  // and value, respectively. In this case, we need to find further entries to
  // concatenate in order to make |target_key| and |target_value|.
  if (key_ngrams_len < target_key.size() &&
      value_ngrams_len < target_value.size()) {
    key_ngrams->push_back(entry->key());
    value_ngrams->push_back(entry->value());
    for (size_t i = 0; i < entry->next_entries().size(); ++i) {
      const uint32 fp = entry->next_entries(i).entry_fp();
      Entry *e = dic_->MutableLookupWithoutInsert(fp);
      if (e == NULL) {
        continue;
      }
      const RemoveNgramChainResult r = RemoveNgramChain(target_key,
                                                        target_value,
                                                        e,
                                                        key_ngrams,
                                                        key_ngrams_len,
                                                        value_ngrams,
                                                        value_ngrams_len);
      switch (r) {
        case DONE:
          return DONE;
        case TAIL:
          // |entry| is the second-to-the-last node. So cut the link to the
          // child entry.
          EraseNextEntries(fp, entry);
          return DONE;
        default:
          break;
      }
    }
    // Recover the state.
    key_ngrams->pop_back();
    value_ngrams->pop_back();
    return NOT_FOUND;
  }

  // This is the case where the current ngram key and value have the same
  // lengths as those of |target_key| and |target_value|, respectively.
  if (key_ngrams_len == target_key.size() &&
      value_ngrams_len == target_value.size()) {
    key_ngrams->push_back(entry->key());
    value_ngrams->push_back(entry->value());
    string ngram_key, ngram_value;
    Util::JoinStringPieces(*key_ngrams, "", &ngram_key);
    Util::JoinStringPieces(*value_ngrams, "", &ngram_value);
    if (ngram_key == target_key && ngram_value == target_value) {
      // |entry| is the last node. So return TAIL to tell the caller so
      // that it can remove the link to this last node.
      return TAIL;
    }
    key_ngrams->pop_back();
    value_ngrams->pop_back();
    return NOT_FOUND;
  }

  return NOT_FOUND;
}

bool UserHistoryPredictor::ClearHistoryEntry(const string &key,
                                             const string &value) {
  bool deleted = false;
  {
    // Find the history entry that has the exactly same key and value and has
    // not been removed yet. If exists, remove it.
    Entry *entry = dic_->MutableLookupWithoutInsert(Fingerprint(key, value));
    if (entry != NULL && !entry->removed()) {
      entry->set_suggestion_freq(0);
      entry->set_conversion_freq(0);
      entry->set_removed(true);
      // We don't clear entry->next_entries() so that we can generate prediction
      // by chaining.
      deleted = true;
    }
  }
  {
    // Find a chain of history entries that produces key and value. If exists,
    // remove the link so that N-gram history prediction never generates this
    // key value pair..
    for (DicElement *elm = dic_->MutableHead(); elm != NULL; elm = elm->next) {
      Entry *entry = &elm->value;
      if (!Util::StartsWith(key, entry->key()) ||
          !Util::StartsWith(value, entry->value())) {
        continue;
      }
      vector<StringPiece> key_ngrams, value_ngrams;
      if (RemoveNgramChain(
              key, value, entry, &key_ngrams, 0, &value_ngrams, 0) == DONE) {
        deleted = true;
      }
    }
  }
  if (deleted) {
    updated_ = true;
  }
  return deleted;
}

// return true if prev_entry has a next_fp link to entry
// static
bool UserHistoryPredictor::HasBigramEntry(
    const UserHistoryPredictor::Entry &entry,
    const UserHistoryPredictor::Entry &prev_entry) {
  const uint32 fp = EntryFingerprint(entry);
  for (int i = 0; i < prev_entry.next_entries_size(); ++i) {
    if (fp == prev_entry.next_entries(i).entry_fp()) {
      return true;
    }
  }
  return false;
}

// static
string UserHistoryPredictor::GetRomanMisspelledKey(
    const Segments &segments) {
  if (GET_CONFIG(preedit_method) != config::Config::ROMAN) {
    return "";
  }

  const string &preedit = segments.conversion_segment(0).key();
  // TODO(team): Use composer if it is available.
  // segments.composer()->GetQueryForConversion(&preedit);
  // Since ConverterInterface doesn't have StartPredictionWithComposer,
  // we cannot use composer currently.
  if (!preedit.empty() && MaybeRomanMisspelledKey(preedit)) {
    return ToRoman(preedit);
  }

  return "";
}

// static
bool UserHistoryPredictor::MaybeRomanMisspelledKey(const string &key) {
  int num_alpha = 0;
  int num_hiragana = 0;
  int num_unknown = 0;
  for (ConstChar32Iterator iter(key); !iter.Done(); iter.Next()) {
    const char32 w = iter.Get();
    const Util::ScriptType type = Util::GetScriptType(w);
    if (type == Util::HIRAGANA || w == 0x30FC) {  // "ー".
      ++num_hiragana;
      continue;
    }
    if (type == Util::UNKNOWN_SCRIPT && num_unknown <= 0) {
      ++num_unknown;
      continue;
    }
    if (type == Util::ALPHABET && num_alpha <= 0) {
      ++num_alpha;
      continue;
    }
    return false;
  }

  return (num_hiragana > 0 &&
          ((num_alpha == 1 && num_unknown == 0) ||
           (num_alpha == 0 && num_unknown == 1)));
}

// static
bool UserHistoryPredictor::RomanFuzzyPrefixMatch(
    const string &str, const string &prefix) {
  if (prefix.empty() || prefix.size() > str.size()) {
    return false;
  }

  // 1. allow one character delete in Romanji sequence.
  // 2. allow one swap in Romanji sequence.
  for (size_t i = 0; i < prefix.size(); ++i) {
    if (prefix[i] == str[i]) {
      continue;
    }

    if (str[i] == '-') {
      // '-' voice sound mark can be matched to any
      // non-alphanum character.
      if (!isalnum(prefix[i])) {
        string replaced_prefix = prefix;
        replaced_prefix[i] = str[i];
        if (Util::StartsWith(str, replaced_prefix)) {
          return true;
        }
      }
    } else {
      // deletion.
      string inserted_prefix = prefix;
      inserted_prefix.insert(i, 1, str[i]);
      if (Util::StartsWith(str, inserted_prefix)) {
        return true;
      }

      // swap.
      if (i + 1 < prefix.size()) {
        string swapped_prefix = prefix;
        swap(swapped_prefix[i], swapped_prefix[i + 1]);
        if (Util::StartsWith(str, swapped_prefix)) {
          return true;
        }
      }
    }

    return false;
  }

  // |prefix| is an exact suffix of |str|.
  return false;
}

bool UserHistoryPredictor::RomanFuzzyLookupEntry(
    const string &roman_input_key,
    const UserHistoryPredictor::Entry *entry,
    EntryPriorityQueue *results) const {
  if (roman_input_key.empty()) {
    return false;
  }

  DCHECK(entry);
  DCHECK(results);

  if (!RomanFuzzyPrefixMatch(ToRoman(entry->key()),
                             roman_input_key)) {
    return false;
  }

  Entry *result = results->NewEntry();
  DCHECK(result);
  result->Clear();
  result->CopyFrom(*entry);
  result->set_spelling_correction(true);
  results->Push(result);

  return true;
}

UserHistoryPredictor::Entry *UserHistoryPredictor::AddEntry(
    const Entry &entry, EntryPriorityQueue *results) const {
  // We add an entry even if it was marked as removed so that it can be used to
  // generate prediction by entry chaining. The deleted entry itself is never
  // shown in the final prediction result as it is filtered finally.
  Entry *new_entry = results->NewEntry();
  new_entry->Clear();
  new_entry->CopyFrom(entry);
  return new_entry;
}

UserHistoryPredictor::Entry *UserHistoryPredictor::AddEntryWithNewKeyValue(
    const string &key, const string &value, const Entry &entry,
    EntryPriorityQueue *results) const {
  // We add an entry even if it was marked as removed so that it can be used to
  // generate prediction by entry chaining. The deleted entry itself is never
  // shown in the final prediction result as it is filtered finally.
  Entry *new_entry = results->NewEntry();
  new_entry->Clear();
  new_entry->CopyFrom(entry);
  new_entry->set_key(key);
  new_entry->set_value(value);

  // Set removed field true if the new key and value were removed.
  const Entry *e = dic_->LookupWithoutInsert(Fingerprint(key, value));
  new_entry->set_removed(e != NULL && e->removed());

  return new_entry;
}

bool UserHistoryPredictor::LookupEntry(
    const string &input_key,
    const string &key_base,
    const Trie<string> *key_expanded,
    const UserHistoryPredictor::Entry *entry,
    const UserHistoryPredictor::Entry *prev_entry,
    EntryPriorityQueue *results) const {
  CHECK(entry);
  CHECK(results);

  Entry *result = NULL;

  const Entry *last_entry = NULL;

  // last_access_time of the left-closest content word.
  uint32 left_last_access_time = 0;

  // last_access_time of the left-most content word.
  uint32 left_most_last_access_time = 0;

  // Example: [a|B|c|D]
  // a,c: functional word
  // B,D: content word
  // left_last_access_time:   timestamp of D
  // left_most_last_access_time:   timestamp of B

  // |input_key| is a query user is now typing.
  // |entry->key()| is a target value saved in the database.
  //  const string input_key = key_base;

  const MatchType mtype = GetMatchTypeFromInput(
      input_key, key_base, key_expanded, entry->key());
  if (mtype == NO_MATCH) {
    return false;
  } else if (mtype == LEFT_EMPTY_MATCH) {  // zero-query-suggestion
    // if |input_key| is empty, the |prev_entry| and |entry| must
    // have bigram relation.
    if (prev_entry != NULL && HasBigramEntry(*entry, *prev_entry)) {
      result = AddEntry(*entry, results);
      if (result) {
        last_entry = entry;
        left_last_access_time = entry->last_access_time();
        left_most_last_access_time = IsContentWord(entry->value()) ?
            left_last_access_time : 0;
      }
    } else {
      return false;
    }
  } else if (mtype == LEFT_PREFIX_MATCH) {
    // |input_key| is shorter than |entry->key()|
    // This scenario is a simple prefix match.
    // e.g., |input_key|="foo", |entry->key()|="foobar"
    result = AddEntry(*entry, results);
    if (result) {
      last_entry = entry;
      left_last_access_time = entry->last_access_time();
      left_most_last_access_time = IsContentWord(entry->value()) ?
          left_last_access_time : 0;
    }
  } else if (mtype == RIGHT_PREFIX_MATCH || mtype == EXACT_MATCH) {
    // |input_key| is longer than or the same as |entry->key()|.
    // In this case, recursively traverse "next_entries" until
    // target entry gets longer than input_key.
    // e.g., |input_key|="foobar", |entry->key()|="foo"
    left_last_access_time = entry->last_access_time();
    left_most_last_access_time = IsContentWord(entry->value()) ?
        left_last_access_time : 0;
    string key = entry->key();
    string value = entry->value();
    const Entry *current_entry = entry;
    set<uint64> seen;
    seen.insert(EntryFingerprint(*current_entry));
    // Until target entry gets longer than input_key.
    while (key.size() <= input_key.size()) {
      const Entry *latest_entry = NULL;
      const Entry *left_same_timestamp_entry = NULL;
      const Entry *left_most_same_timestamp_entry = NULL;
      for (size_t i = 0; i < current_entry->next_entries_size(); ++i) {
        const Entry *tmp_entry = dic_->LookupWithoutInsert(
            current_entry->next_entries(i).entry_fp());
        if (tmp_entry == NULL || tmp_entry->key().empty()) {
          continue;
        }
        const MatchType mtype2 = GetMatchType(key + tmp_entry->key(),
                                              input_key);
        if (mtype2 == NO_MATCH || mtype2 == LEFT_EMPTY_MATCH) {
          continue;
        }
        if (latest_entry == NULL ||
            latest_entry->last_access_time() < tmp_entry->last_access_time()) {
          latest_entry = tmp_entry;
        }
        if (tmp_entry->last_access_time() == left_last_access_time) {
          left_same_timestamp_entry = tmp_entry;
        }
        if (tmp_entry->last_access_time() == left_most_last_access_time) {
          left_most_same_timestamp_entry = tmp_entry;
        }
      }

      // Prefer bigrams which are generated at the same time.
      // When last_access_time are the same, these two bigrams were
      // input together.
      // The preferences:
      // (1). The current entry's time stamp is equal to that of
      //      left most content word
      // (2). The current entry's time stamp is equal to that of
      //      left closest content word
      // (3). The current entry is the latest
      const Entry *next_entry = left_most_same_timestamp_entry;
      if (next_entry == NULL) {
        next_entry = left_same_timestamp_entry;
      }
      if (next_entry == NULL) {
        next_entry = latest_entry;
      }

      if (next_entry == NULL || next_entry->key().empty()) {
        break;
      }

      // if duplicate entry is found, don't expand more.
      // This is because an entry only has one timestamp.
      // we cannot trust the timestamp if there are duplicate values
      // in one input.
      if (!seen.insert(EntryFingerprint(*next_entry)).second) {
        break;
      }

      key += next_entry->key();
      value += next_entry->value();
      current_entry = next_entry;
      last_entry = next_entry;

      // Don't update left_access_time if the current entry is
      // not a content word.
      // The time-stamp of non-content-word will be updated frequently.
      // The time-stamp of the previous candidate is more trustful.
      // It partially fixes the bug http://b/2843371.
      const bool is_content_word = IsContentWord(current_entry->value());

      if (is_content_word) {
        left_last_access_time = current_entry->last_access_time();
      }

      // if left_most entry is a functional word (symbols/punctuations),
      // we don't take it as a canonical candidate.
      if (left_most_last_access_time == 0 && is_content_word) {
        left_most_last_access_time = current_entry->last_access_time();
      }
    }

    if (key.size() < input_key.size()) {
      VLOG(3) << "Cannot find prefix match even after chain rules";
      return false;
    }

    result = AddEntryWithNewKeyValue(key, value, *entry, results);
  } else {
    LOG(ERROR) << "Unknown match mode: " << mtype;
    return false;
  }

  if (result == NULL) {
    return false;
  }

  // if prev entry is not NULL, check whether there is a bigram
  // from |prev_entry| to |entry|.
  result->set_bigram_boost(false);

  if (prev_entry != NULL && HasBigramEntry(*entry, *prev_entry)) {
    // set bigram_boost flag so that this entry is boosted
    // against LRU policy.
    result->set_bigram_boost(true);
  }

  if (!result->removed()) {
    results->Push(result);
  }

  // Expand new entry which was input just after "last_entry"
  if (last_entry != NULL &&
      Util::CharsLen(result->key()) >= 1 &&
      2 * Util::CharsLen(input_key) >= Util::CharsLen(result->key())) {
    const Entry *latest_entry = NULL;
    const Entry *left_same_timestamp_entry = NULL;
    const Entry *left_most_same_timestamp_entry = NULL;
    for (int i = 0; i < last_entry->next_entries_size(); ++i) {
      const Entry *tmp_entry = dic_->LookupWithoutInsert(
          last_entry->next_entries(i).entry_fp());
      if (tmp_entry == NULL || tmp_entry->key().empty()) {
        continue;
      }
      if (latest_entry == NULL ||
          latest_entry->last_access_time() < tmp_entry->last_access_time()) {
        latest_entry = tmp_entry;
      }
      if (tmp_entry->last_access_time() == left_last_access_time) {
        left_same_timestamp_entry = tmp_entry;
      }
      if (tmp_entry->last_access_time() == left_most_last_access_time) {
        left_most_same_timestamp_entry = tmp_entry;
      }
    }

    const Entry *next_entry = left_most_same_timestamp_entry;
    if (next_entry == NULL) {
      next_entry = left_same_timestamp_entry;
    }
    if (next_entry == NULL) {
      next_entry = latest_entry;
    }

    // the new entry was input within 10 seconds.
    // TODO(taku): This is a simple heuristics.
    if (next_entry != NULL && !next_entry->key().empty() &&
        abs(static_cast<int32>(next_entry->last_access_time() -
                               last_entry->last_access_time())) <= 10 &&
        IsContentWord(next_entry->value())) {
      Entry *result2 = AddEntryWithNewKeyValue(
          result->key() + next_entry->key(),
          result->value() + next_entry->value(),
          *result,
          results);
      if (!result2->removed()) {
        results->Push(result2);
      }
    }
  }

  return true;
}

bool UserHistoryPredictor::Predict(Segments *segments) const {
  ConversionRequest default_request;
  return PredictForRequest(default_request, segments);
}

bool UserHistoryPredictor::PredictForRequest(const ConversionRequest &request,
                                             Segments *segments) const {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return false;
  }

  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return false;
  }

  if (segments->request_type() == Segments::CONVERSION) {
    VLOG(2) << "request type is CONVERSION";
    return false;
  }

  if (!GET_CONFIG(use_history_suggest) &&
      segments->request_type() == Segments::SUGGESTION) {
    VLOG(2) << "no history suggest";
    return false;
  }

  if (segments->conversion_segments_size() < 1) {
    VLOG(2) << "segment size < 1";
    return false;
  }

  if (dic_->Head() == NULL) {
    VLOG(2) << "dic head is NULL";
    return false;
  }

  const RequestType request_type = request.request().zero_query_suggestion() ?
      ZERO_QUERY_SUGGESTION : DEFAULT;
  const string &input_key = segments->conversion_segment(0).key();
  if (IsPunctuation(Util::SubString(input_key, 0, 1))) {
    VLOG(2) << "input_key starts with punctuations";
    return false;
  }

  const size_t input_key_len = Util::CharsLen(input_key);
  if (input_key_len == 0 && request_type == DEFAULT) {
    VLOG(2) << "key length is 0";
    return false;
  }

  const Entry *prev_entry =
      LookupPrevEntry(*segments, request.request().available_emoji_carrier());
  if (input_key_len == 0 && prev_entry == NULL) {
    VLOG(1) << "If input_key_len is 0, prev_entry must be set";
    return false;
  }

  EntryPriorityQueue results;
  GetResultsFromHistoryDictionary(request, *segments, prev_entry, &results);
  if (results.size() == 0) {
    VLOG(2) << "no prefix match candiate is found.";
    return false;
  }

  return InsertCandidates(request_type, request, segments, &results);
}

const UserHistoryPredictor::Entry *UserHistoryPredictor::LookupPrevEntry(
    const Segments &segments, uint32 available_emoji_carrier) const {
  const size_t history_segments_size = segments.history_segments_size();
  const Entry *prev_entry = NULL;
  // when threre are non-zero history segments, lookup an entry
  // from the LRU dictionary, which is correspoinding to the last
  // history segment.
  if (history_segments_size == 0) {
    return NULL;
  }

  const Segment &history_segment =
      segments.history_segment(history_segments_size - 1);

  // Simply lookup the history_segment.
  prev_entry = dic_->LookupWithoutInsert(SegmentFingerprint(history_segment));

  // When |prev_entry| is NULL or |prev_entry| has no valid next_entries,
  // do linear-search over the LRU.
  if ((prev_entry == NULL && history_segment.candidates_size() > 0) ||
      (prev_entry != NULL && prev_entry->next_entries_size() == 0)) {
    const string &prev_value = prev_entry == NULL ?
        history_segment.candidate(0).value : prev_entry->value();
    int trial = 0;
    for (const DicElement *elm = dic_->Head();
         trial++ < kMaxPrevValueTrial && elm != NULL; elm = elm->next) {
      const Entry *entry = &(elm->value);
      // entry->value() equals to the prev_value or
      // entry->value() is a SUFFIX of prev_value.
      // length of entry->value() must be >= 2, as single-length
      // match would be noisy.
      if (IsValidEntry(*entry, available_emoji_carrier) &&
          entry != prev_entry &&
          entry->next_entries_size() > 0 &&
          Util::CharsLen(entry->value()) >= 2 &&
          (entry->value() == prev_value ||
           Util::EndsWith(prev_value, entry->value()))) {
        prev_entry = entry;
        break;
      }
    }
  }
  return prev_entry;
}

void UserHistoryPredictor::GetResultsFromHistoryDictionary(
    const ConversionRequest &request,
    const Segments &segments, const Entry *prev_entry,
    EntryPriorityQueue *results) const {
  DCHECK(results);
  const size_t max_results_size = 5 * segments.max_prediction_candidates_size();

  // Get romanized input key if the given preedit looks misspelled.
  const string roman_input_key = GetRomanMisspelledKey(segments);

  // TODO(team): make GetKanaMisspelledKey(segments);
  // const string kana_input_key = GetKanaMisspelledKey(segments);

  // If we have ambiguity for the input, get expanded key.
  // Example1 roman input: for "あk", we will get |base|, "あ" and |expanded|,
  // "か", "き", etc
  // Example2 kana input: for "あか", we will get |base|, "あ" and |expanded|,
  // "か", and "が".

  // |base_key| and |input_key| could be different
  // For kana-input, we will expand the ambiguity for "゛".
  // When we input "もす",
  // |base_key|: "も"
  // |expanded|: "す", "ず"
  // |input_key|: "もす"
  // In this case, we want to show candidates for "もす" as EXACT match,
  // and candidates for "もず" as LEFT_PREFIX_MATCH
  //
  // For roman-input, when we input "あｋ",
  // |input_key| is "あｋ" and |base_key| is "あ"
  string input_key;
  string base_key;
  scoped_ptr<Trie<string> > expanded;
  GetInputKeyFromSegments(request, segments, &input_key, &base_key, &expanded);

  int trial = 0;
  for (const DicElement *elm = dic_->Head(); elm != NULL; elm = elm->next) {
    if (!IsValidEntryIgnoringRemovedField(
            elm->value, request.request().available_emoji_carrier())) {
      continue;
    }
    if (segments.request_type() == Segments::SUGGESTION &&
        trial++ >= kMaxSuggestionTrial) {
      VLOG(2) << "too many trials";
      break;
    }

    // lookup key from elm_value and prev_entry.
    // If a new entry is found, the entry is pushed to the results.
    // TODO(team): make KanaFuzzyLookupEntry().
    if (!LookupEntry(input_key, base_key, expanded.get(), &(elm->value),
                     prev_entry, results) &&
        !RomanFuzzyLookupEntry(roman_input_key, &(elm->value), results)) {
      continue;
    }

    // already found enough results.
    if (results->size() >= max_results_size) {
      break;
    }
  }
}

// static
void UserHistoryPredictor::GetInputKeyFromSegments(
    const ConversionRequest &request, const Segments &segments,
    string *input_key, string *base,
    scoped_ptr<Trie<string> > *expanded) {
  DCHECK(input_key);
  DCHECK(base);

  if (!request.has_composer() ||
      !FLAGS_enable_expansion_for_user_history_predictor) {
    *input_key = segments.conversion_segment(0).key();
    *base = segments.conversion_segment(0).key();
    return;
  }

  request.composer().GetStringForPreedit(input_key);
  set<string> expanded_set;
  request.composer().GetQueriesForPrediction(base, &expanded_set);
  if (expanded_set.size() > 0) {
    expanded->reset(new Trie<string>);
    for (set<string>::const_iterator itr = expanded_set.begin();
         itr != expanded_set.end(); ++itr) {
      // For getting matched key, insert values
      (*expanded)->AddEntry(*itr, *itr);
    }
  }
}

bool UserHistoryPredictor::InsertCandidates(RequestType request_type,
                                            const ConversionRequest &request,
                                            Segments *segments,
                                            EntryPriorityQueue *results) const {
  DCHECK(results);
  Segment *segment = segments->mutable_conversion_segment(0);
  if (segment == NULL) {
    LOG(ERROR) << "segment is NULL";
    return false;
  }
  const uint32 input_key_len = Util::CharsLen(segment->key());
  while (segment->candidates_size() <
         segments->max_prediction_candidates_size()) {
    // |results| is a priority queue where the elemtnt
    // in the queue is sorted by the score defined in GetScore().
    const Entry *result_entry = results->Pop();
    if (result_entry == NULL) {
      // Pop() returns NULL when no more valid entry exists.
      break;
    }
    bool is_valid_candidate = false;
    if (segments->request_type() == Segments::PREDICTION) {
      is_valid_candidate = true;
    } else if (segments->request_type() == Segments::SUGGESTION) {
      // The top result of suggestion should be a VALID suggestion candidate.
      // i.e., SuggestionTrigerFunc should return true for the first
      // candidate.
      // If user types "デスノート" too many times, "デスノート" will be
      // suggested when user types "で". It is expected, but if user types
      // "です" after that,  showing "デスノート" is annoying.
      // In this situation, "です" is in the LRU, but SuggestionTrigerFunc
      // returns false for "です", since it is short.
      if (IsValidSuggestion(request_type,
                            input_key_len, *result_entry)) {
        is_valid_candidate = true;
      } else if (segment->candidates_size() == 0) {
        VLOG(2) << "candidates size is 0";
        return false;
      }
    } else {
      LOG(ERROR) << "Unknown mode";
      return false;
    }

    if (!is_valid_candidate) {
      VLOG(2) << "not a valid candidate: " << result_entry->key();
      continue;
    }

    if (request.request().mixed_conversion() &&
        result_entry->suggestion_freq() < 2 &&
        Util::CharsLen(result_entry->value()) > 8) {
      // Don't show long history for mixed conversion
      // TODO(toshiyuki): Better to merge this into IsValidSuggestion logic.
      VLOG(2) << "long candidate: " << result_entry->value();
      continue;
    }

    Segment::Candidate *candidate = segment->push_back_candidate();
    DCHECK(candidate);
    candidate->Init();
    candidate->key = result_entry->key();
    candidate->content_key = result_entry->key();
    candidate->value = result_entry->value();
    candidate->content_value = result_entry->value();
    candidate->attributes |=
        Segment::Candidate::USER_HISTORY_PREDICTION |
        Segment::Candidate::NO_VARIANTS_EXPANSION;
    if (result_entry->spelling_correction()) {
      candidate->attributes |= Segment::Candidate::SPELLING_CORRECTION;
    }
    const string &description = result_entry->description();
    // If we have stored description, set it exactly.
    if (!description.empty()) {
      candidate->description = description;
      candidate->attributes |= Segment::Candidate::NO_EXTRA_DESCRIPTION;
    } else {
      VariantsRewriter::SetDescriptionForPrediction(*pos_matcher_, candidate);
    }
#if DEBUG
    if (candidate->description.find("History") == string::npos) {
      candidate->description += " History";
    }
#endif  // DEBUG
  }

  return (segment->candidates_size() > 0);
}

void UserHistoryPredictor::InsertNextEntry(
    const UserHistoryPredictor::NextEntry &next_entry,
    UserHistoryPredictor::Entry *entry) const {
  if (next_entry.entry_fp() == 0 || entry == NULL) {
    return;
  }

  NextEntry *target_next_entry = NULL;

  // If next_entries_size is less than kMaxNextEntriesSize,
  // we simply allocate a new entry.
  if (entry->next_entries_size() < max_next_entries_size()) {
    target_next_entry = entry->add_next_entries();
  } else {
    // Otherwise, find the oldest next_entry.
    uint32 last_access_time = UINT_MAX;
    for (int i = 0; i < entry->next_entries_size(); ++i) {
      // already has the same id
      if (next_entry.entry_fp() == entry->next_entries(i).entry_fp()) {
        target_next_entry = entry->mutable_next_entries(i);
        break;
      }
      const Entry *found_entry = dic_->LookupWithoutInsert(
          entry->next_entries(i).entry_fp());
      // reuse the entry if it is already removed from the LRU.
      if (found_entry == NULL) {
        target_next_entry = entry->mutable_next_entries(i);
        break;
      }
      // preserve the oldest entry
      if (target_next_entry == NULL ||
          last_access_time > found_entry->last_access_time()) {
        target_next_entry = entry->mutable_next_entries(i);
        last_access_time = found_entry->last_access_time();
      }
    }
  }

  if (target_next_entry == NULL) {
    LOG(ERROR) << "cannot find a room for inserting next fp";
    return;
  }

  target_next_entry->CopyFrom(next_entry);
}

bool UserHistoryPredictor::IsValidEntry(
    const Entry &entry, uint32 available_emoji_carrier) const {
  return !entry.removed() &&
      IsValidEntryIgnoringRemovedField(entry, available_emoji_carrier);
}

bool UserHistoryPredictor::IsValidEntryIgnoringRemovedField(
    const Entry &entry, uint32 available_emoji_carrier) const {
  if (entry.entry_type() != Entry::DEFAULT_ENTRY ||
      suppression_dictionary_->SuppressEntry(entry.key(), entry.value())) {
    return false;
  }

  if (IsEmojiEntry(entry)) {
    if (Util::IsAndroidPuaEmoji(entry.value())) {
      // Android carrier dependent emoji.
      const uint32 kAndroidCarrier =
          Request::DOCOMO_EMOJI |
          Request::SOFTBANK_EMOJI |
          Request::KDDI_EMOJI;
      if (!(available_emoji_carrier & kAndroidCarrier)) {
        return false;
      }
    } else {
      // Unicode 6.0 emoji.
      if (!(available_emoji_carrier & Request::UNICODE_EMOJI)) {
        return false;
      }
    }
  }

  return true;
}

void UserHistoryPredictor::InsertEvent(EntryType type) {
  if (type == Entry::DEFAULT_ENTRY) {
    return;
  }

  const uint32 last_access_time = static_cast<uint32>(Util::GetTime());
  const uint32 dic_key = Fingerprint("", "", type);

  CHECK(dic_.get());
  DicElement *e = dic_->Insert(dic_key);
  if (e == NULL) {
    VLOG(2) << "insert failed";
    return;
  }

  Entry *entry = &(e->value);
  DCHECK(entry);
  entry->Clear();
  entry->set_entry_type(type);
  entry->set_last_access_time(last_access_time);
}

void UserHistoryPredictor::Insert(const string &key,
                                  const string &value,
                                  const string &description,
                                  bool is_suggestion_selected,
                                  uint32 next_fp,
                                  uint32 last_access_time,
                                  Segments *segments) {
  if (key.empty() || value.empty() ||
      key.size() > kMaxStringLength ||
      value.size() > kMaxStringLength ||
      description.size() > kMaxStringLength) {
    return;
  }

  const uint32 dic_key = Fingerprint(key, value);

  if (!dic_->HasKey(dic_key)) {
    // the key is a new key inserted in the last Finish method.
    // Here we push a new RevertEntry so that the new "key" can be
    // removed when Revert() method is called.
    Segments::RevertEntry *revert_entry = segments->push_back_revert_entry();
    DCHECK(revert_entry);
    revert_entry->key = Uint32ToString(dic_key);
    revert_entry->id = UserHistoryPredictor::revert_id();
    revert_entry->revert_entry_type = Segments::RevertEntry::CREATE_ENTRY;
  } else {
    // the key is a old key not inserted in the last Finish method
    // TODO(taku):
    // add a treatment for UPDATE_ENTRY mode
  }

  DicElement *e = dic_->Insert(dic_key);
  if (e == NULL) {
    VLOG(2) << "insert failed";
    return;
  }

  Entry *entry = &(e->value);
  DCHECK(entry);

  entry->set_key(key);
  entry->set_value(value);
  entry->set_removed(false);

  if (description.empty()) {
    entry->clear_description();
  } else {
    entry->set_description(description);
  }

  entry->set_last_access_time(last_access_time);
  if (is_suggestion_selected) {
    entry->set_suggestion_freq(entry->suggestion_freq() + 1);
  } else {
    entry->set_conversion_freq(entry->conversion_freq() + 1);
  }

  // Insert next_fp to the entry
  if (next_fp != 0) {
    NextEntry next_entry;
    next_entry.set_entry_fp(next_fp);
    InsertNextEntry(next_entry, entry);
  }

  VLOG(2) << key << " " << value << " has inserted: "
          << entry->Utf8DebugString();

  // new entry is inserted to the cache
  updated_ = true;
}

void UserHistoryPredictor::Finish(Segments *segments) {
  if (segments->request_type() == Segments::REVERSE_CONVERSION) {
    // Do nothing for REVERSE_CONVERSION.
    return;
  }

  if (GET_CONFIG(incognito_mode)) {
    VLOG(2) << "incognito mode";
    return;
  }

  if (!GET_CONFIG(use_history_suggest)) {
    VLOG(2) << "no history suggest";
    return;
  }

  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  const bool is_suggestion = segments->request_type() != Segments::CONVERSION;
  const uint32 last_access_time = static_cast<uint32>(Util::GetTime());

  // If user inputs a punctuation just after some long sentence,
  // we make a new candidate by concatinating the top element in LRU and
  // the punctuation user input. The top element in LRU is supposed to be
  // the long sentence user input before.
  // This is a fix for http://b/issue?id=2216838
  if (dic_->Head() != NULL &&
      segments->conversion_segments_size() == 1 &&
      segments->history_segments_size() > 0 &&
      segments->conversion_segment(0).candidates_size() > 0 &&
      segments->history_segment(
          segments->history_segments_size() - 1).candidates_size() > 0 &&
      Util::CharsLen(
          segments->conversion_segment(0).candidate(0).value) == 1 &&
      IsPunctuation(
          segments->conversion_segment(0).candidate(0).value) &&
      dic_->Head()->value.last_access_time() + 5 > last_access_time) {
    const Entry *entry = &(dic_->Head()->value);
    DCHECK(entry);
    const string &last_value =
        segments->history_segment(
            segments->history_segments_size() - 1).candidate(0).value;
    // Check that value in head element of LRU ends with the candidate value
    // in history segments.
    if (last_value.size() <= entry->value().size() &&
        entry->value().substr(entry->value().size() - last_value.size(),
                              last_value.size()) == last_value) {
      const Segment::Candidate &candidate =
          segments->conversion_segment(0).candidate(0);
      const string key = entry->key() + candidate.key;
      const string value = entry->value() + candidate.value;
      // use the same last_access_time stored in the top element
      // so that this item can be grouped together.
      Insert(key, value, entry->description(), is_suggestion, 0,
             entry->last_access_time(), segments);
    }
  }

  string all_key, all_value;
  const size_t history_segments_size = segments->history_segments_size();

  // Check every segment is valid.
  for (size_t i = history_segments_size; i < segments->segments_size(); ++i) {
    const Segment &segment = segments->segment(i);
    if (segment.candidates_size() < 1) {
      VLOG(2) << "candidates size < 1";
      return;
    }
    if (segment.segment_type() != Segment::FIXED_VALUE) {
      VLOG(2) << "segment is not FIXED_VALUE";
      return;
    }
    const Segment::Candidate &candidate = segment.candidate(0);
    if (candidate.attributes &
        Segment::Candidate::NO_SUGGEST_LEARNING) {
      VLOG(2) << "NO_SUGGEST_LEARNING";
      return;
    }
  }

  if (IsPrivacySensitive(segments)) {
    VLOG(2) << "do not remember privacy sensitive input";
    return;
  }

  InsertHistory(is_suggestion, last_access_time, segments);
  return;
}

void UserHistoryPredictor::MakeLearningSegments(
    const Segments &segments, SegmentsForLearning *learning_segments) const {
  DCHECK(learning_segments);

  for (size_t i = 0; i < segments.history_segments_size(); ++i) {
    const Segment &segment = segments.history_segment(i);
    SegmentForLearning learning_segment;
    DCHECK_LE(1, segment.candidates_size());
    learning_segment.key = segment.candidate(0).key;
    learning_segment.value = segment.candidate(0).value;
    learning_segment.description = GetDescription(segment.candidate(0));
    learning_segments->push_back_history_segment(learning_segment);
  }
  for (size_t i = 0; i < segments.conversion_segments_size(); ++i) {
    const Segment &segment = segments.conversion_segment(i);
    const Segment::Candidate &candidate = segment.candidate(0);
    if (candidate.inner_segment_boundary.empty()) {
      SegmentForLearning learning_segment;
      learning_segment.key = candidate.key;
      learning_segment.value = candidate.value;
      learning_segment.description = GetDescription(candidate);
      learning_segments->push_back_conversion_segment(learning_segment);
    } else {
      int key_start_pos = 0, value_start_pos = 0;
      for (size_t j = 0; j < candidate.inner_segment_boundary.size(); ++j) {
        const int key_len = candidate.inner_segment_boundary[j].first;
        const int value_len = candidate.inner_segment_boundary[j].second;
        SegmentForLearning learning_segment;
        Util::SubString(candidate.key, key_start_pos,
                        key_len, &learning_segment.key);
        Util::SubString(candidate.value, value_start_pos,
                        value_len, &learning_segment.value);
        learning_segments->push_back_conversion_segment(learning_segment);
        key_start_pos += key_len;
        value_start_pos += value_len;
      }
      DCHECK_EQ(key_start_pos, Util::CharsLen(candidate.key));
      DCHECK_EQ(value_start_pos, Util::CharsLen(candidate.value));
    }
  }
}

void UserHistoryPredictor::InsertHistory(bool is_suggestion_selected,
                                         uint32 last_access_time,
                                         Segments *segments) {
  SegmentsForLearning learning_segments;
  MakeLearningSegments(*segments, &learning_segments);

  string all_key, all_value;
  set<uint64> seen;
  bool this_was_seen = false;
  const size_t history_segments_size =
      learning_segments.history_segments_size();

  for (size_t i = history_segments_size;
       i < learning_segments.all_segments_size(); ++i) {
    const SegmentForLearning &segment = learning_segments.all_segment(i);
    all_key += segment.key;
    all_value += segment.value;
    uint32 next_fp = (i == learning_segments.all_segments_size() - 1) ?
        0 : LearningSegmentFingerprint(learning_segments.all_segment(i + 1));
    // remember the first segment
    if (i == history_segments_size) {
      seen.insert(LearningSegmentFingerprint(segment));
    }
    uint32 next_fp_to_set = next_fp;
    // If two duplicate segments exist, kills the link
    // TO/FROM the second one to prevent loops.
    // Only killing "TO" link caused bug #2982886:
    // after converting "らいおん（もうじゅう）とぞうりむし（びせいぶつ）"
    // and typing "ぞうりむし", "ゾウリムシ（猛獣" was suggested.
    if (this_was_seen) {
      next_fp_to_set = 0;
    }
    if (!seen.insert(next_fp).second) {
      next_fp_to_set = 0;
      this_was_seen = true;
    } else {
      this_was_seen = false;
    }
    Insert(segment.key,
           segment.value,
           segment.description,
           is_suggestion_selected, next_fp_to_set,
           last_access_time, segments);
  }

  // insert all_key/all_value
  if (learning_segments.conversion_segments_size() > 1 &&
      !all_key.empty() && !all_value.empty()) {
    Insert(all_key, all_value, "",
           is_suggestion_selected,
           0, last_access_time, segments);
  }

  // make a link from the right most history_segment to
  // the left most segment or entire user input.
  if (learning_segments.history_segments_size() > 0 &&
      learning_segments.conversion_segments_size() > 0) {
    Entry *history_entry =
        dic_->MutableLookupWithoutInsert(
            LearningSegmentFingerprint(
                learning_segments.history_segment(
                    segments->history_segments_size() - 1)));

    NextEntry next_entry;
    if (segments->request_type() == Segments::CONVERSION) {
      next_entry.set_entry_fp(
          LearningSegmentFingerprint(learning_segments.conversion_segment(0)));
      InsertNextEntry(next_entry, history_entry);
    }

    // entire user input or SUGGESTION
    if (segments->request_type() != Segments::CONVERSION ||
        learning_segments.conversion_segments_size() > 1) {
      next_entry.set_entry_fp(Fingerprint(all_key, all_value));
      InsertNextEntry(next_entry, history_entry);
    }
  }

  return;
}

void UserHistoryPredictor::Revert(Segments *segments) {
  if (!CheckSyncerAndDelete()) {
    LOG(WARNING) << "Syncer is running";
    return;
  }

  for (size_t i = 0; i < segments->revert_entries_size(); ++i) {
    const Segments::RevertEntry &revert_entry =
        segments->revert_entry(i);
    if (revert_entry.id == UserHistoryPredictor::revert_id() &&
        revert_entry.revert_entry_type == Segments::RevertEntry::CREATE_ENTRY) {
      VLOG(2) << "Erasing the key: " << StringToUint32(revert_entry.key);
      dic_->Erase(StringToUint32(revert_entry.key));
    }
  }
}

// type
// static
UserHistoryPredictor::MatchType UserHistoryPredictor::GetMatchType(
    const string &lstr, const string &rstr) {
  if (lstr.empty() && !rstr.empty()) {
    return LEFT_EMPTY_MATCH;
  }

  const size_t size = min(lstr.size(), rstr.size());
  if (size == 0) {
    return NO_MATCH;
  }

  const int result = memcmp(lstr.data(), rstr.data(), size);
  if (result != 0) {
    return NO_MATCH;
  }

  if (lstr.size() == rstr.size()) {
    return EXACT_MATCH;
  } else if (lstr.size() < rstr.size()) {
    return LEFT_PREFIX_MATCH;
  } else {
    return RIGHT_PREFIX_MATCH;
  }

  return NO_MATCH;
}

// static
UserHistoryPredictor::MatchType UserHistoryPredictor::GetMatchTypeFromInput(
    const string &input_key,
    const string &key_base, const Trie<string> *key_expanded,
    const string &target) {
  if (key_expanded == NULL) {
    // |input_key| and |key_base| can be different by compoesr modification.
    // For example, |input_key|, "８，＋", and |base| "８、＋".
    return GetMatchType(key_base, target);
  }

  // we can assume key_expanded != NULL from here.
  if (key_base.empty()) {
      string value;
      size_t key_length = 0;
      bool has_subtrie = false;
      if (!key_expanded->LookUpPrefix(target, &value,
                                      &key_length, &has_subtrie)) {
        return NO_MATCH;
      } else if (value == target && value == input_key) {
        return EXACT_MATCH;
      } else {
        return LEFT_PREFIX_MATCH;
      }
  } else {
    const size_t size = min(key_base.size(), target.size());
    if (size == 0) {
      return NO_MATCH;
    }
    const int result = memcmp(key_base.data(), target.data(), size);
    if (result != 0) {
      return NO_MATCH;
    }
    if (target.size() <= key_base.size()) {
      return RIGHT_PREFIX_MATCH;
    }
    string value;
    size_t key_length = 0;
    bool has_subtrie = false;
    if (!key_expanded->LookUpPrefix(target.data() + key_base.size(),
                                    &value,
                                    &key_length, &has_subtrie)) {
      return NO_MATCH;
    }
    const string matched = key_base + value;
    if (matched == target && matched == input_key) {
      return EXACT_MATCH;
    } else {
      return LEFT_PREFIX_MATCH;
    }
  }

  DCHECK(false) << "Should not come here";
  return NO_MATCH;
}

// static
uint32 UserHistoryPredictor::Fingerprint(const string &key,
                                         const string &value,
                                         EntryType type) {
  if (type == Entry::DEFAULT_ENTRY) {
    // Since we have already used the fingerprint function for next entries and
    // next entries are saved in user's local machine, we are not able
    // to change the Fingerprint function for the old key/value type.
    return Util::Fingerprint32(key + kDelimiter + value);
  } else {
    uint8 id = static_cast<uint8>(type);
    return Util::Fingerprint32(reinterpret_cast<char *>(&id), sizeof(id));
  }
}

// static
uint32 UserHistoryPredictor::Fingerprint(const string &key,
                                         const string &value) {
  return Fingerprint(key, value, Entry::DEFAULT_ENTRY);
}

uint32 UserHistoryPredictor::EntryFingerprint(
    const UserHistoryPredictor::Entry &entry) {
  return Fingerprint(entry.key(), entry.value());
}

// static
uint32 UserHistoryPredictor::SegmentFingerprint(const Segment &segment) {
  if (segment.candidates_size() > 0) {
    return Fingerprint(segment.candidate(0).key, segment.candidate(0).value);
  }
  return 0;
}

// static
uint32 UserHistoryPredictor::LearningSegmentFingerprint(
    const SegmentForLearning &segment) {
  return Fingerprint(segment.key, segment.value);
}

// static
string UserHistoryPredictor::Uint32ToString(uint32 fp) {
  string buf(reinterpret_cast<const char *>(&fp), sizeof(fp));
  return buf;
}

// static
uint32 UserHistoryPredictor::StringToUint32(const string &input) {
  uint32 result = 0;
  if (input.size() == sizeof(result)) {
    memcpy(reinterpret_cast<char *>(&result), input.data(), input.size());
  }
  return result;
}

// static
bool UserHistoryPredictor::IsValidSuggestion(
    RequestType request_type,
    uint32 prefix_len,
    const UserHistoryPredictor::Entry &entry) {
  // when bigram_boost is true, that means that previous user input
  // and current input have bigram relation.
  if (entry.bigram_boost()) {
    return true;
  }
  // when zero_query_suggestion is true, that means that
  // predictor is running on mobile device. In this case,
  // make the behavior more aggressive.
  if (request_type == ZERO_QUERY_SUGGESTION) {
    return true;
  }
  // Handle suggestion_freq and conversion_freq differently.
  // conversion_freq is less aggressively affecting to the final decision.
  const uint32 freq = max(entry.suggestion_freq(),
                          entry.conversion_freq() / 4);

  // TODO(taku,komatsu): better to make it simpler and easier to be understood.
  const uint32 base_prefix_len = 3 - min(static_cast<uint32>(2), freq);
  return (prefix_len >= base_prefix_len);
}

// static
// 1) sort by last_access_time, which is basically the same as LRU policy.
// 2) boost shorter candidate, if having the same last_access_time.
// 3) add a bigram boost as a special bonus.
// TODO(taku): better to take "frequency" into consideration
uint32 UserHistoryPredictor::GetScore(
    const UserHistoryPredictor::Entry &entry) {
  const uint32 kBigramBoostAsTime = 7 * 24 * 60 * 60;   // 1 week.
  return
      entry.last_access_time() - Util::CharsLen(entry.value()) +
      (entry.bigram_boost() ? kBigramBoostAsTime : 0);
}

// return the size of cache.
// static
uint32 UserHistoryPredictor::cache_size() {
  return kLRUCacheSize;
}

// return the size of next entries.
// static
uint32 UserHistoryPredictor::max_next_entries_size() {
  return kMaxNextEntriesSize;
}

}  // namespace mozc
