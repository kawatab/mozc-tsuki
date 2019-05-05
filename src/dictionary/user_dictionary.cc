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

#include "dictionary/user_dictionary.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
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

namespace mozc {
namespace dictionary {
namespace {

struct OrderByKey {
  bool operator()(const UserPOS::Token *token, StringPiece key) const {
    return token->key < key;
  }

  bool operator()(StringPiece key, const UserPOS::Token *token) const {
    return key < token->key;
  }
};

struct OrderByKeyPrefix {
  bool operator()(const UserPOS::Token *token, StringPiece prefix) const {
    return StringPiece(token->key).substr(0, prefix.size()) < prefix;
  }

  bool operator()(StringPiece prefix, const UserPOS::Token *token) const {
    return prefix < StringPiece(token->key).substr(0, prefix.size());
  }
};

struct OrderByKeyThenById {
  bool operator()(const UserPOS::Token *lhs, const UserPOS::Token *rhs) const {
    const int comp = lhs->key.compare(rhs->key);
    return comp == 0 ? (lhs->id < rhs->id) : (comp < 0);
  }
};

class UserDictionaryFileManager {
 public:
  UserDictionaryFileManager() {}

  const string GetFileName() {
    scoped_lock l(&mutex_);
    if (filename_.empty()) {
      return UserDictionaryUtil::GetUserDictionaryFileName();
    } else {
      return filename_;
    }
  }

  void SetFileName(const string &filename) {
    scoped_lock l(&mutex_);
    filename_ = filename;
  }

 private:
  string filename_;
  Mutex mutex_;
  DISALLOW_COPY_AND_ASSIGN(UserDictionaryFileManager);
};

void FillTokenFromUserPOSToken(const UserPOS::Token &user_pos_token,
                               Token *token) {
  token->key = user_pos_token.key;
  token->value = user_pos_token.value;
  token->cost = user_pos_token.cost;
  token->lid = user_pos_token.id;
  token->rid = user_pos_token.id;
  token->attributes = Token::USER_DICTIONARY;
}

}  // namespace

class UserDictionary::TokensIndex : public std::vector<UserPOS::Token *> {
 public:
  TokensIndex(const UserPOSInterface *user_pos,
              SuppressionDictionary *suppression_dictionary)
      : user_pos_(user_pos),
        suppression_dictionary_(suppression_dictionary) {}

  ~TokensIndex() {
    Clear();
  }

  void Clear() {
    STLDeleteElements(this);
    clear();
  }

  void Load(const user_dictionary::UserDictionaryStorage &storage) {
    Clear();
    std::set<uint64> seen;
    std::vector<UserPOS::Token> tokens;

    if (!suppression_dictionary_->IsLocked()) {
      LOG(ERROR) << "SuppressionDictionary must be locked first";
    }
    suppression_dictionary_->Clear();

    for (size_t i = 0; i < storage.dictionaries_size(); ++i) {
      const UserDictionaryStorage::UserDictionary &dic =
          storage.dictionaries(i);
      if (!dic.enabled() || dic.entries_size() == 0) {
        continue;
      }

      for (size_t j = 0; j < dic.entries_size(); ++j) {
        const UserDictionaryStorage::UserDictionaryEntry &entry =
            dic.entries(j);

        if (!UserDictionaryUtil::IsValidEntry(*user_pos_, entry)) {
          continue;
        }

        string tmp, reading;
        UserDictionaryUtil::NormalizeReading(entry.key(), &tmp);

        // We cannot call NormalizeVoiceSoundMark inside NormalizeReading,
        // because the normalization is user-visible.
        // http://b/2480844
        Util::NormalizeVoicedSoundMark(tmp, &reading);

        DCHECK_LE(0, entry.pos());
MOZC_CLANG_PUSH_WARNING();
#if MOZC_CLANG_HAS_WARNING(tautological-constant-out-of-range-compare)
MOZC_CLANG_DISABLE_WARNING(tautological-constant-out-of-range-compare);
#endif  // MOZC_CLANG_HAS_WARNING(tautological-constant-out-of-range-compare)
        DCHECK_LE(entry.pos(), 255);
MOZC_CLANG_POP_WARNING();
        const uint64 fp = Hash::Fingerprint(reading +
                                            "\t" +
                                            entry.value() +
                                            "\t" +
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
          for (size_t k = 0; k < tokens.size(); ++k) {
            this->push_back(new UserPOS::Token(tokens[k]));
            Util::StripWhiteSpaces(entry.comment(), &this->back()->comment);
          }
        }
      }
    }

    // Sort first by key and then by POS ID.
    std::sort(this->begin(), this->end(), OrderByKeyThenById());

    suppression_dictionary_->UnLock();

    VLOG(1) << this->size() << " user dic entries loaded";

    usage_stats::UsageStats::SetInteger("UserRegisteredWord",
                                        static_cast<int>(this->size()));
  }

 private:
  const UserPOSInterface *user_pos_;
  SuppressionDictionary *suppression_dictionary_;
};

class UserDictionary::UserDictionaryReloader : public Thread {
 public:
  explicit UserDictionaryReloader(UserDictionary *dic)
      : modified_at_(0), auto_register_mode_(false), dic_(dic) {
    DCHECK(dic_);
  }

  ~UserDictionaryReloader() override {
    Join();
  }

  void StartAutoRegistration(const string &key,
                             const string &value,
                             user_dictionary::UserDictionary::PosType pos) {
    {
      scoped_lock l(&mutex_);
      auto_register_mode_ = true;
      key_ = key;
      value_ = value;
      pos_ = pos;
    }
    Start("UserDictionaryReloader");
  }

  // When the user dictionary exists AND the modification time has been updated,
  // reloads the dictionary.  Returns true when reloader thread is started.
  bool MaybeStartReload() {
    FileTimeStamp modification_time;
    if (!FileUtil::GetModificationTime(
        Singleton<UserDictionaryFileManager>::get()->GetFileName(),
        &modification_time)) {
      // If the file doesn't exist, return doing nothing.
      // Therefore if the file is deleted after first reload,
      // second reload does nothing so the content loaded by first reload
      // is kept as is.
      return false;
    }
    if (modified_at_ == modification_time) {
      return false;
    }
    modified_at_ = modification_time;
    Start("UserDictionaryReloader");
    return true;
  }

  void Run() override {
    std::unique_ptr<UserDictionaryStorage> storage(new UserDictionaryStorage(
        Singleton<UserDictionaryFileManager>::get()->GetFileName()));

    // Load from file
    if (!storage->Load()) {
      return;
    }

    if (storage->ConvertSyncDictionariesToNormalDictionaries()) {
      LOG(INFO) << "Syncable dictionaries are converted to normal dictionaries";
      if (storage->Lock()) {
        storage->Save();
        storage->UnLock();
      }
    }

    if (auto_register_mode_ &&
        !storage->AddToAutoRegisteredDictionary(key_, value_, pos_)) {
      LOG(ERROR) << "failed to execute AddToAutoRegisteredDictionary";
      auto_register_mode_ = false;
      return;
    }

    auto_register_mode_ = false;
    dic_->Load(*(storage.get()));
  }

 private:
  FileTimeStamp modified_at_;
  Mutex mutex_;
  bool auto_register_mode_;
  UserDictionary *dic_;
  string key_;
  string value_;
  user_dictionary::UserDictionary::PosType pos_;

  DISALLOW_COPY_AND_ASSIGN(UserDictionaryReloader);
};

UserDictionary::UserDictionary(const UserPOSInterface *user_pos,
                               POSMatcher pos_matcher,
                               SuppressionDictionary *suppression_dictionary)
    : ALLOW_THIS_IN_INITIALIZER_LIST(
          reloader_(new UserDictionaryReloader(this))),
      user_pos_(user_pos),
      pos_matcher_(pos_matcher),
      suppression_dictionary_(suppression_dictionary),
      tokens_(new TokensIndex(user_pos_.get(), suppression_dictionary)),
      mutex_(new ReaderWriterMutex) {
  DCHECK(user_pos_.get());
  DCHECK(suppression_dictionary_);
  Reload();
}

UserDictionary::~UserDictionary() {
  reloader_->Join();
  delete tokens_;
}

bool UserDictionary::HasKey(StringPiece key) const {
  // TODO(noriyukit): Currently, we don't support HasKey() for user dictionary
  // because we need to search tokens linearly, which might be slow in extreme
  // cases where 100K entries exist.
  return false;
}

bool UserDictionary::HasValue(StringPiece value) const {
  // TODO(noriyukit): Currently, we don't support HasValue() for user dictionary
  // because we need to search tokens linearly, which might be slow in extreme
  // cases where 100K entries exist.  Note: HasValue() method is used only in
  // UserHistoryPredictor for privacy sensitivity check.
  return false;
}

void UserDictionary::LookupPredictive(
    StringPiece key,
    const ConversionRequest &conversion_request,
    Callback *callback) const {
  scoped_reader_lock l(mutex_.get());

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
  for (auto range = std::equal_range(tokens_->begin(), tokens_->end(), key,
                                     OrderByKeyPrefix());
       range.first != range.second; ++range.first) {
    const UserPOS::Token &user_pos_token = **range.first;
    switch (callback->OnKey(user_pos_token.key)) {
      case Callback::TRAVERSE_DONE:
        return;
      case Callback::TRAVERSE_NEXT_KEY:
      case Callback::TRAVERSE_CULL:
        continue;
      default:
        break;
    }
    FillTokenFromUserPOSToken(user_pos_token, &token);
    // Override POS IDs for suggest only words.
    if (pos_matcher_.IsSuggestOnlyWord(user_pos_token.id)) {
      token.lid = token.rid = pos_matcher_.GetUnknownId();
    }
    if (callback->OnToken(user_pos_token.key, user_pos_token.key, token) ==
        Callback::TRAVERSE_DONE) {
      return;
    }
  }
}

// UserDictionary doesn't support kana modifier insensitive lookup.
void UserDictionary::LookupPrefix(
    StringPiece key,
    const ConversionRequest &conversion_request,
    Callback *callback) const {
  scoped_reader_lock l(mutex_.get());

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
  const StringPiece first_char = key.substr(0, Util::OneCharLen(key.data()));
  Token token;
  for (auto it = std::lower_bound(tokens_->begin(), tokens_->end(), first_char,
                                  OrderByKey());
       it != tokens_->end(); ++it) {
    const UserPOS::Token &user_pos_token = **it;
    if (user_pos_token.key > key) {
      break;
    }
    if (pos_matcher_.IsSuggestOnlyWord(user_pos_token.id)) {
      continue;
    }
    if (!Util::StartsWith(key, user_pos_token.key)) {
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
    FillTokenFromUserPOSToken(user_pos_token, &token);
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

void UserDictionary::LookupExact(
    StringPiece key,
    const ConversionRequest &conversion_request,
    Callback *callback) const {
  scoped_reader_lock l(mutex_.get());
  if (key.empty() || tokens_->empty() ||
      conversion_request.config().incognito_mode()) {
    return;
  }
  auto range = std::equal_range(tokens_->begin(), tokens_->end(), key,
                                OrderByKey());
  if (range.first == range.second) {
    return;
  }
  if (callback->OnKey(key) != Callback::TRAVERSE_CONTINUE) {
    return;
  }

  Token token;
  for (; range.first != range.second; ++range.first) {
    const UserPOS::Token &user_pos_token = **range.first;
    if (pos_matcher_.IsSuggestOnlyWord(user_pos_token.id)) {
      continue;
    }
    FillTokenFromUserPOSToken(user_pos_token, &token);
    if (callback->OnToken(key, key, token) != Callback::TRAVERSE_CONTINUE) {
      return;
    }
  }
}

void UserDictionary::LookupReverse(
    StringPiece key,
    const ConversionRequest &conversion_request,
    Callback *callback) const {
}

bool UserDictionary::LookupComment(StringPiece key, StringPiece value,
                                   const ConversionRequest &conversion_request,
                                   string *comment) const {
  if (key.empty() || conversion_request.config().incognito_mode()) {
    return false;
  }

  scoped_reader_lock l(mutex_.get());
  if (tokens_->empty()) {
    return false;
  }

  // Set the comment that was found first.
  for (auto range = std::equal_range(tokens_->begin(), tokens_->end(), key,
                                     OrderByKey());
       range.first != range.second; ++range.first) {
    const UserPOS::Token &token = **range.first;
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
  suppression_dictionary_->Lock();
  DCHECK(suppression_dictionary_->IsLocked());
  // When the reloader is started, |suppression_dictionary_| is unlocked by the
  // reloader.  When not started, need to unlock it here.
  if (!reloader_->MaybeStartReload()) {
    suppression_dictionary_->UnLock();
  }
  return true;
}

namespace {

class FindValueCallback : public DictionaryInterface::Callback {
 public:
  explicit FindValueCallback(StringPiece value)
      : value_(value), found_(false) {}

  virtual ResultType OnToken(StringPiece,  // key
                             StringPiece,  // actual_key
                             const Token &token) {
    if (token.value == value_) {
      found_ = true;
      return TRAVERSE_DONE;
    }
    return TRAVERSE_CONTINUE;
  }

  bool found() const { return found_; }

 private:
  const StringPiece value_;
  bool found_;
};

}  // namespace

bool UserDictionary::AddToAutoRegisteredDictionary(
    const string &key, const string &value,
    const ConversionRequest &conversion_request,
    user_dictionary::UserDictionary::PosType pos) {
  if (reloader_->IsRunning()) {
    return false;
  }

  FindValueCallback callback(value);
  LookupExact(key, conversion_request, &callback);
  if (callback.found()) {
    // Already registered.
    return false;
  }

  suppression_dictionary_->Lock();
  DCHECK(suppression_dictionary_->IsLocked());
  reloader_->StartAutoRegistration(key, value, pos);

  return true;
}

void UserDictionary::WaitForReloader() {
  reloader_->Join();
}

void UserDictionary::Swap(TokensIndex *new_tokens) {
  DCHECK(new_tokens);
  TokensIndex *old_tokens = tokens_;
  {
    scoped_writer_lock l(mutex_.get());
    tokens_ = new_tokens;
  }
  delete old_tokens;
}

bool UserDictionary::Load(
    const user_dictionary::UserDictionaryStorage &storage) {
  size_t size = 0;
  {
    scoped_reader_lock l(mutex_.get());
    size = tokens_->size();
  }

  // If UserDictionary is pretty big, we first remove the
  // current dictionary to save memory usage.
#ifdef OS_ANDROID
  const size_t kVeryBigUserDictionarySize = 5000;
#else
  const size_t kVeryBigUserDictionarySize = 100000;
#endif

  if (size >= kVeryBigUserDictionarySize) {
    TokensIndex *dummy_empty_tokens = new TokensIndex(user_pos_.get(),
                                                      suppression_dictionary_);
    Swap(dummy_empty_tokens);
  }

  suppression_dictionary_->Lock();
  TokensIndex *tokens = new TokensIndex(user_pos_.get(),
                                        suppression_dictionary_);
  tokens->Load(storage);  // |suppression_dictionary_| is unlocked in Load().
  DCHECK(!suppression_dictionary_->IsLocked());
  Swap(tokens);
  return true;
}

void UserDictionary::SetUserDictionaryName(const string &filename) {
  Singleton<UserDictionaryFileManager>::get()->SetFileName(filename);
}

}  // namespace dictionary
}  // namespace mozc
