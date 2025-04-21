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

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/japanese_util.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/thread.h"
#include "base/util.h"
#include "dictionary/dictionary_token.h"
#include "dictionary/pos_matcher.h"
#include "dictionary/suppression_dictionary.h"
#include "dictionary/user_dictionary_storage.h"
#include "dictionary/user_dictionary_util.h"
#include "dictionary/user_pos.h"
#include "protocol/config.pb.h"
#include "usage_stats/usage_stats.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"

namespace mozc {
namespace dictionary {
namespace {

struct OrderByKey {
  bool operator()(const UserPos::Token &token, absl::string_view key) const {
    return token.key < key;
  }

  bool operator()(absl::string_view key, const UserPos::Token &token) const {
    return key < token.key;
  }
};

struct OrderByKeyPrefix {
  bool operator()(const UserPos::Token &token, absl::string_view prefix) const {
    return absl::string_view(token.key).substr(0, prefix.size()) < prefix;
  }

  bool operator()(absl::string_view prefix, const UserPos::Token &token) const {
    return prefix < absl::string_view(token.key).substr(0, prefix.size());
  }
};

struct OrderByKeyThenById {
  bool operator()(const UserPos::Token &lhs, const UserPos::Token &rhs) const {
    const int comp = lhs.key.compare(rhs.key);
    return comp == 0 ? (lhs.id < rhs.id) : (comp < 0);
  }
};

class UserDictionaryFileManager {
 public:
  UserDictionaryFileManager() = default;

  UserDictionaryFileManager(const UserDictionaryFileManager &) = delete;
  UserDictionaryFileManager &operator=(const UserDictionaryFileManager &) =
      delete;

  const std::string GetFileName() {
    absl::MutexLock l(&mutex_);
    if (filename_.empty()) {
      return UserDictionaryUtil::GetUserDictionaryFileName();
    } else {
      return filename_;
    }
  }

  void SetFileName(const std::string &filename) {
    absl::MutexLock l(&mutex_);
    filename_ = filename;
  }

 private:
  std::string filename_;
  absl::Mutex mutex_;
};

}  // namespace

class UserDictionary::TokensIndex {
 public:
  TokensIndex(const UserPosInterface *user_pos,
              SuppressionDictionary *suppression_dictionary)
      : user_pos_(user_pos), suppression_dictionary_(suppression_dictionary) {}

  ~TokensIndex() = default;

  bool empty() const { return user_pos_tokens_.empty(); }
  size_t size() const { return user_pos_tokens_.size(); }

  std::vector<UserPos::Token>::const_iterator begin() const {
    return user_pos_tokens_.begin();
  }
  std::vector<UserPos::Token>::const_iterator end() const {
    return user_pos_tokens_.end();
  }

  void Load(const user_dictionary::UserDictionaryStorage &storage) {
    user_pos_tokens_.clear();
    std::set<uint64_t> seen;
    std::vector<UserPos::Token> tokens;

    const SuppressionDictionaryLock l(suppression_dictionary_);
    suppression_dictionary_->Clear();

    for (const UserDictionaryStorage::UserDictionary &dic :
         storage.dictionaries()) {
      if (!dic.enabled() || dic.entries_size() == 0) {
        continue;
      }

      const bool is_shortcuts =
          (dic.name() == "__auto_imported_android_shortcuts_dictionary");

      for (const UserDictionaryStorage::UserDictionaryEntry &entry :
           dic.entries()) {
        if (!UserDictionaryUtil::IsValidEntry(*user_pos_, entry)) {
          continue;
        }

        std::string tmp, reading;
        UserDictionaryUtil::NormalizeReading(entry.key(), &tmp);

        // We cannot call NormalizeVoiceSoundMark inside NormalizeReading,
        // because the normalization is user-visible.
        // http://b/2480844
        japanese_util::NormalizeVoicedSoundMark(tmp, &reading);

        DCHECK_LE(0, entry.pos());
        MOZC_CLANG_PUSH_WARNING();
        // clang-format off
#if MOZC_CLANG_HAS_WARNING(tautological-constant-out-of-range-compare)
        MOZC_CLANG_DISABLE_WARNING(tautological-constant-out-of-range-compare);
#endif  // MOZC_CLANG_HAS_WARNING(tautological-constant-out-of-range-compare)
        // clang-format on
        DCHECK_LE(entry.pos(), 255);
        MOZC_CLANG_POP_WARNING();
        const uint64_t fp =
            Hash::Fingerprint(reading + "\t" + entry.value() + "\t" +
                              static_cast<char>(entry.pos()));
        if (!seen.insert(fp).second) {
          VLOG(1) << "Found dup item";
          continue;
        }

        // "抑制単語"
        if (entry.pos() == user_dictionary::UserDictionary::SUPPRESSION_WORD) {
          suppression_dictionary_->AddEntry(reading, entry.value());
        } else {
          tokens.clear();
          user_pos_->GetTokens(
              reading, entry.value(),
              UserDictionaryUtil::GetStringPosType(entry.pos()), &tokens);
          for (auto &token : tokens) {
            Util::StripWhiteSpaces(entry.comment(), &token.comment);
            if (is_shortcuts &&
                token.has_attribute(UserPos::Token::SUGGESTION_ONLY)) {
              // Words fed by Android shortcut are registered as SUGGESTION_ONLY
              // POS in order to minimize the side-effect of extremely short
              // reading. However, user expect that they should appear in the
              // normal conversion. Here we replace the attribute from
              // SUGGESTION_ONLY to SHORTCUT, which has more adaptive cost based
              // on the length of the key.
              token.remove_attribute(UserPos::Token::SUGGESTION_ONLY);
              token.add_attribute(UserPos::Token::SHORTCUT);
            }
            user_pos_tokens_.push_back(std::move(token));
          }
        }
      }
    }
    user_pos_tokens_.shrink_to_fit();

    // Sort first by key and then by POS ID.
    std::sort(user_pos_tokens_.begin(), user_pos_tokens_.end(),
              OrderByKeyThenById());

    VLOG(1) << user_pos_tokens_.size() << " user dic entries loaded";

    usage_stats::UsageStats::SetInteger(
        "UserRegisteredWord", static_cast<int>(user_pos_tokens_.size()));
  }

 private:
  const UserPosInterface *user_pos_;
  SuppressionDictionary *suppression_dictionary_;
  std::vector<UserPos::Token> user_pos_tokens_;
};

class UserDictionary::UserDictionaryReloader : public Thread {
 public:
  explicit UserDictionaryReloader(UserDictionary *dic)
      : modified_at_(0), dic_(dic) {
    DCHECK(dic_);
  }

  UserDictionaryReloader(const UserDictionaryReloader &) = delete;
  UserDictionaryReloader &operator=(const UserDictionaryReloader &) = delete;

  ~UserDictionaryReloader() override { Join(); }

  // When the user dictionary exists AND the modification time has been updated,
  // reloads the dictionary.  Returns true when reloader thread is started.
  bool MaybeStartReload() {
    absl::StatusOr<FileTimeStamp> modification_time =
        FileUtil::GetModificationTime(
            Singleton<UserDictionaryFileManager>::get()->GetFileName());
    if (!modification_time.ok()) {
      // If the file doesn't exist, return doing nothing.
      // Therefore if the file is deleted after first reload,
      // second reload does nothing so the content loaded by first reload
      // is kept as is.
      LOG(WARNING) << "Cannot get modification time of the user dictionary: "
                   << modification_time.status();
      return false;
    }
    if (modified_at_ == *modification_time) {
      return false;
    }
    modified_at_ = *modification_time;
    Start("UserDictionaryReloader");
    return true;
  }

  void Run() override {
    UserDictionaryStorage storage(
        Singleton<UserDictionaryFileManager>::get()->GetFileName());

    // Load from file
    if (absl::Status s = storage.Load(); !s.ok()) {
      LOG(ERROR) << "Failed to load the user dictionary: " << s;
      return;
    }

    if (storage.ConvertSyncDictionariesToNormalDictionaries()) {
      LOG(INFO) << "Syncable dictionaries are converted to normal dictionaries";
      if (storage.Lock()) {
        if (absl::Status s = storage.Save(); !s.ok()) {
          LOG(ERROR) << "Failed to save to storage: " << s;
        }
        storage.UnLock();
      }
    }

    dic_->Load(storage.GetProto());
  }

 private:
  FileTimeStamp modified_at_;
  UserDictionary *dic_;
  std::string key_;
  std::string value_;
};

UserDictionary::UserDictionary(std::unique_ptr<const UserPosInterface> user_pos,
                               PosMatcher pos_matcher,
                               SuppressionDictionary *suppression_dictionary)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          reloader_(new UserDictionaryReloader(this))),
      user_pos_(std::move(user_pos)),
      pos_matcher_(pos_matcher),
      suppression_dictionary_(suppression_dictionary),
      tokens_(new TokensIndex(user_pos_.get(), suppression_dictionary)) {
  DCHECK(user_pos_.get());
  DCHECK(suppression_dictionary_);
  Reload();
}

UserDictionary::~UserDictionary() {
  reloader_->Join();
  delete tokens_;
}

bool UserDictionary::HasKey(absl::string_view key) const {
  // TODO(noriyukit): Currently, we don't support HasKey() for user dictionary
  // because we need to search tokens linearly, which might be slow in extreme
  // cases where 100K entries exist.
  return false;
}

bool UserDictionary::HasValue(absl::string_view value) const {
  // TODO(noriyukit): Currently, we don't support HasValue() for user dictionary
  // because we need to search tokens linearly, which might be slow in extreme
  // cases where 100K entries exist.  Note: HasValue() method is used only in
  // UserHistoryPredictor for privacy sensitivity check.
  return false;
}

void UserDictionary::LookupPredictive(
    absl::string_view key, const ConversionRequest &conversion_request,
    Callback *callback) const {
  absl::ReaderMutexLock l(&mutex_);

  if (key.empty()) {
    VLOG(2) << "string of length zero is passed.";
    return;
  }
  if (tokens_->empty()) {
    return;
  }
  if (conversion_request.config().incognito_mode()) {
    return;
  }

  // Find the starting point of iteration over dictionary contents.
  Token token;
  for (auto [begin, end] = std::equal_range(tokens_->begin(), tokens_->end(),
                                            key, OrderByKeyPrefix());
       begin != end; ++begin) {
    const UserPos::Token &user_pos_token = *begin;
    switch (callback->OnKey(user_pos_token.key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
      case Callback::TRAVERSE_CULL:
        continue;
      default:
        break;
    }
    PopulateTokenFromUserPosToken(user_pos_token, PREDICTIVE, &token);
    if (callback->OnToken(user_pos_token.key, user_pos_token.key, token) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
  }
}

// UserDictionary doesn't support kana modifier insensitive lookup.
void UserDictionary::LookupPrefix(absl::string_view key,
                                  const ConversionRequest &conversion_request,
                                  Callback *callback) const {
  absl::ReaderMutexLock l(&mutex_);

  if (key.empty()) {
    LOG(WARNING) << "string of length zero is passed.";
    return;
  }
  if (tokens_->empty()) {
    return;
  }
  if (conversion_request.config().incognito_mode()) {
    return;
  }

  // Find the starting point for iteration over dictionary contents.
  const absl::string_view first_char =
      key.substr(0, Util::OneCharLen(key.data()));
  Token token;
  for (auto it = std::lower_bound(tokens_->begin(), tokens_->end(), first_char,
                                  OrderByKey());
       it != tokens_->end(); ++it) {
    const UserPos::Token &user_pos_token = *it;
    if (user_pos_token.key > key) {
      break;
    }
    if (user_pos_token.has_attribute(UserPos::Token::SUGGESTION_ONLY)) {
      continue;
    }
    if (!absl::StartsWith(key, user_pos_token.key)) {
      continue;
    }
    switch (callback->OnKey(user_pos_token.key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
        continue;
      case Callback::TRAVERSE_CULL:
        LOG(FATAL) << "UserDictionary doesn't support culling.";
        break;
      default:
        break;
    }
    PopulateTokenFromUserPosToken(user_pos_token, PREFIX, &token);
    switch (callback->OnToken(user_pos_token.key, user_pos_token.key, token)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_CULL:
        LOG(FATAL) << "UserDictionary doesn't support culling.";
        break;
      default:
        break;
    }
  }
}

void UserDictionary::LookupExact(absl::string_view key,
                                 const ConversionRequest &conversion_request,
                                 Callback *callback) const {
  absl::ReaderMutexLock l(&mutex_);
  if (key.empty() || tokens_->empty() ||
      conversion_request.config().incognito_mode()) {
    return;
  }
  auto [begin, end] =
      std::equal_range(tokens_->begin(), tokens_->end(), key, OrderByKey());
  if (begin == end) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }

  Token token;
  for (; begin != end; ++begin) {
    const UserPos::Token &user_pos_token = *begin;
    if (user_pos_token.has_attribute(UserPos::Token::SUGGESTION_ONLY)) {
      continue;
    }
    PopulateTokenFromUserPosToken(user_pos_token, EXACT, &token);
    if (callback->OnToken(key, key, token) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void UserDictionary::LookupReverse(absl::string_view key,
                                   const ConversionRequest &conversion_request,
                                   Callback *callback) const {}

bool UserDictionary::LookupComment(absl::string_view key,
                                   absl::string_view value,
                                   const ConversionRequest &conversion_request,
                                   std::string *comment) const {
  if (key.empty() || conversion_request.config().incognito_mode()) {
    return false;
  }

  absl::ReaderMutexLock l(&mutex_);
  if (tokens_->empty()) {
    return false;
  }

  // Set the comment that was found first.
  for (auto [begin, end] = std::equal_range(tokens_->begin(), tokens_->end(),
                                            key, OrderByKey());
       begin != end; ++begin) {
    const UserPos::Token &token = *begin;
    if (token.value == value && !token.comment.empty()) {
      comment->assign(token.comment);
      return true;
    }
  }
  return false;
}

bool UserDictionary::Reload() {
  if (reloader_->IsRunning()) {
    return false;
  }
  if (!reloader_->MaybeStartReload()) {
    LOG(INFO) << "MaybeStartReload() didn't start reloading";
  }
  return true;
}

namespace {

class FindValueCallback : public DictionaryInterface::Callback {
 public:
  explicit FindValueCallback(absl::string_view value)
      : value_(value), found_(false) {}

  ResultType OnToken(absl::string_view,  // key
                     absl::string_view,  // actual_key
                     const Token &token) override {
    if (token.value == value_) {
      found_ = true;
      return TRAVERSE_DONE;
    }
    return TRAVERSE_CONTINUE;
  }

  bool found() const { return found_; }

 private:
  const absl::string_view value_;
  bool found_;
};

}  // namespace

void UserDictionary::WaitForReloader() { reloader_->Join(); }

void UserDictionary::Swap(TokensIndex *new_tokens) {
  DCHECK(new_tokens);
  TokensIndex *old_tokens = tokens_;
  {
    absl::WriterMutexLock l(&mutex_);
    tokens_ = new_tokens;
  }
  delete old_tokens;
}

bool UserDictionary::Load(
    const user_dictionary::UserDictionaryStorage &storage) {
  size_t size = 0;
  {
    absl::ReaderMutexLock l(&mutex_);
    size = tokens_->size();
  }

  // If UserDictionary is pretty big, we first remove the
  // current dictionary to save memory usage.
#ifdef OS_ANDROID
  constexpr size_t kVeryBigUserDictionarySize = 5000;
#else   // OS_ANDROID
  constexpr size_t kVeryBigUserDictionarySize = 100000;
#endif  // OS_ANDROID

  if (size >= kVeryBigUserDictionarySize) {
    TokensIndex *dummy_empty_tokens =
        new TokensIndex(user_pos_.get(), suppression_dictionary_);
    Swap(dummy_empty_tokens);
  }

  TokensIndex *tokens =
      new TokensIndex(user_pos_.get(), suppression_dictionary_);
  tokens->Load(storage);
  Swap(tokens);
  return true;
}

std::vector<std::string> UserDictionary::GetPosList() const {
  std::vector<std::string> pos_list;
  user_pos_->GetPosList(&pos_list);
  return pos_list;
}

void UserDictionary::SetUserDictionaryName(const std::string &filename) {
  Singleton<UserDictionaryFileManager>::get()->SetFileName(filename);
}

void UserDictionary::PopulateTokenFromUserPosToken(
    const UserPosInterface::Token &user_pos_token, RequestType request_type,
    Token *token) const {
  token->key = user_pos_token.key;
  token->value = user_pos_token.value;
  token->lid = token->rid = user_pos_token.id;
  token->attributes = Token::USER_DICTIONARY;

  // * Overwrites POS ids.
  // Actual pos id of suggestion-only candidates are 名詞-サ変.
  // TODO(taku): We would like to change the POS to 名詞-サ変 in user-pos.def,
  // because SUGGEST_ONLY is not POS.
  if (user_pos_token.has_attribute(UserPos::Token::SUGGESTION_ONLY) ||
      user_pos_token.has_attribute(UserPos::Token::SHORTCUT)) {
    token->lid = token->rid = pos_matcher_.GetUnknownId();
  }

  // * Overwrites costs.
  // Locale is not Japanese.
  if (user_pos_token.has_attribute(UserPos::Token::NON_JA_LOCALE)) {
    token->cost = 10000;
  } else if (user_pos_token.has_attribute(UserPos::Token::ISOLATED_WORD)) {
    // Set smaller cost for "短縮よみ" in order to make
    // the rank of the word higher than others.
    token->cost = 200;
  } else {
    // default user dictionary cost.
    token->cost = 5000;
  }

  // The words added via Android shortcut have adaptive cost based
  // on the length of the key. Shorter keys have more penalty so that
  // they are not shown in the context.
  // TODO(taku): Better to apply this cost for all user defined words?
  if (user_pos_token.has_attribute(UserPos::Token::SHORTCUT) &&
      (request_type == PREFIX || request_type == EXACT)) {
    const int key_length = Util::CharsLen(token->key);
    token->cost += std::max<int>(0, 4 - key_length) * 2000;
  }
}

}  // namespace dictionary
}  // namespace mozc
