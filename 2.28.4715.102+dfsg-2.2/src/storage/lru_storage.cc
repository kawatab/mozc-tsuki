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

#include "storage/lru_storage.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "base/clock.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/port.h"
#include "base/util.h"
#include "absl/container/flat_hash_set.h"

namespace mozc {
namespace storage {
namespace {

constexpr size_t kMaxLruSize = 1000000;  // 1M
constexpr size_t kMaxValueSize = 1024;   // 1024 byte

// The byte length used to store fingerprint and timestamp for each item.
// * 8 bytes for fingerprint
// * 4 bytes for timestamp.
constexpr size_t kItemHeaderSize = 12;

// The byte length used to store LRU properties.
// * 4 bytes for user specified value size
// * 4 bytes for LRU capacity
// * 4 bytes for fingerprint seed
constexpr size_t kFileHeaderSize = 12;

const uint64_t k62DaysInSec = 62 * 24 * 60 * 60;

template <class T>
inline void ReadValue(char **ptr, T *value) {
  memcpy(value, *ptr, sizeof(*value));
  *ptr += sizeof(*value);
}

uint64_t GetFP(const char *ptr) {
  return *reinterpret_cast<const uint64_t *>(ptr);
}

uint32_t GetTimeStamp(const char *ptr) {
  return *reinterpret_cast<const uint32_t *>(ptr + 8);
}

const char *GetValue(const char *ptr) { return ptr + kItemHeaderSize; }

void Update(char *ptr) {
  const uint32_t last_access_time = static_cast<uint32_t>(Clock::GetTime());
  memcpy(ptr + 8, reinterpret_cast<const char *>(&last_access_time), 4);
}

void Update(char *ptr, uint64_t fp, const char *value, size_t value_size) {
  const uint32_t last_access_time = static_cast<uint32_t>(Clock::GetTime());
  memcpy(ptr, reinterpret_cast<const char *>(&fp), 8);
  memcpy(ptr + 8, reinterpret_cast<const char *>(&last_access_time), 4);
  memcpy(ptr + 12, value, value_size);
}

bool IsOlderThan62Days(uint64_t timestamp) {
  const uint64_t now = Clock::GetTime();
  return (timestamp + k62DaysInSec < now);
}

class CompareByTimeStamp {
 public:
  bool operator()(const char *a, const char *b) const {
    return GetTimeStamp(a) > GetTimeStamp(b);
  }
};

}  // namespace

LruStorage *LruStorage::Create(const char *filename) {
  std::unique_ptr<LruStorage> n(new LruStorage);
  if (!n->Open(filename)) {
    LOG(ERROR) << "could not open LruStorage";
    return nullptr;
  }
  return n.release();
}

LruStorage *LruStorage::Create(const char *filename, size_t value_size,
                               size_t size, uint32_t seed) {
  std::unique_ptr<LruStorage> n(new LruStorage);
  if (!n->OpenOrCreate(filename, value_size, size, seed)) {
    LOG(ERROR) << "could not open LruStorage";
    return nullptr;
  }
  return n.release();
}

bool LruStorage::CreateStorageFile(const char *filename, size_t value_size,
                                   size_t size, uint32_t seed) {
  if (value_size == 0 || value_size > kMaxValueSize) {
    LOG(ERROR) << "value_size is out of range";
    return false;
  }

  if (size == 0 || size > kMaxLruSize) {
    LOG(ERROR) << "size is out of range";
    return false;
  }

  if (value_size % 4 != 0) {
    LOG(ERROR) << "value_size_ must be 4 byte alignment";
    return false;
  }

  OutputFileStream ofs(filename, std::ios::binary | std::ios::out);
  if (!ofs) {
    LOG(ERROR) << "cannot open " << filename;
    return false;
  }

  const uint32_t value_size_uint32 = static_cast<uint32_t>(value_size);
  const uint32_t size_uint32 = static_cast<uint32_t>(size);

  ofs.write(reinterpret_cast<const char *>(&value_size_uint32),
            sizeof(value_size_uint32));
  ofs.write(reinterpret_cast<const char *>(&size_uint32), sizeof(size_uint32));
  ofs.write(reinterpret_cast<const char *>(&seed), sizeof(seed));
  std::vector<char> ary(value_size, '\0');
  const uint32_t last_access_time = 0;
  const uint64_t fp = 0;

  for (size_t i = 0; i < size; ++i) {
    ofs.write(reinterpret_cast<const char *>(&fp), sizeof(fp));
    ofs.write(reinterpret_cast<const char *>(&last_access_time),
              sizeof(last_access_time));
    ofs.write(reinterpret_cast<const char *>(&ary[0]),
              static_cast<std::streamsize>(ary.size() * sizeof(ary[0])));
  }

  return true;
}

// Reopen file after initializing mapped page.
bool LruStorage::Clear() {
  // Don't need to clear the page if the lru list is empty
  if (mmap_ == nullptr || lru_list_.empty()) {
    return true;
  }
  const size_t offset = sizeof(value_size_) + sizeof(size_) + sizeof(seed_);
  if (offset >= mmap_->size()) {  // should not happen
    return false;
  }
  memset(mmap_->begin() + offset, '\0', mmap_->size() - offset);
  lru_list_.clear();
  lru_map_.clear();
  Open(mmap_->begin(), mmap_->size());
  return true;
}

bool LruStorage::Merge(const char *filename) {
  LruStorage target_storage;
  if (!target_storage.Open(filename)) {
    return false;
  }
  return Merge(target_storage);
}

bool LruStorage::Merge(const LruStorage &storage) {
  if (storage.value_size() != value_size()) {
    return false;
  }

  if (seed_ != storage.seed_) {
    return false;
  }

  std::vector<const char *> ary;

  // this file
  {
    const char *begin = begin_;
    const char *end = end_;
    while (begin < end) {
      ary.push_back(begin);
      begin += item_size();
    }
  }

  // target file
  {
    const char *begin = storage.begin_;
    const char *end = storage.end_;
    while (begin < end) {
      ary.push_back(begin);
      begin += item_size();
    }
  }

  std::stable_sort(ary.begin(), ary.end(), CompareByTimeStamp());

  std::string buf;
  absl::flat_hash_set<uint64_t> seen;  // remove duplicated entries.
  for (size_t i = 0; i < ary.size(); ++i) {
    if (!seen.insert(GetFP(ary[i])).second) {
      continue;
    }
    buf.append(const_cast<const char *>(ary[i]), item_size());
  }

  const size_t old_size = static_cast<size_t>(end_ - begin_);
  const size_t new_size = std::min(buf.size(), old_size);

  // TODO(taku): this part is not atomic.
  // If the converter process is killed while memcpy or memset is running,
  // the storage data will be broken.
  memcpy(begin_, buf.data(), new_size);
  if (new_size < old_size) {
    memset(begin_ + new_size, '\0', old_size - new_size);
  }

  return Open(mmap_->begin(), mmap_->size());
}

LruStorage::LruStorage()
    : value_size_(0),
      size_(0),
      seed_(0),
      next_item_(nullptr),
      begin_(nullptr),
      end_(nullptr) {}

LruStorage::~LruStorage() { Close(); }

bool LruStorage::OpenOrCreate(const char *filename, size_t new_value_size,
                              size_t new_size, uint32_t new_seed) {
  if (absl::Status s = FileUtil::FileExists(filename); !s.ok()) {
    // This is also an expected scenario. Let's create a new data file.
    VLOG(1) << filename << " does not exist. Creating a new one.";
    if (!LruStorage::CreateStorageFile(filename, new_value_size, new_size,
                                       new_seed)) {
      LOG(ERROR) << "CreateStorageFile failed against " << filename;
      return false;
    }
  }

  if (!Open(filename)) {
    Close();
    LOG(ERROR) << "Failed to open the file or the data is corrupted. "
                  "So try to recreate new file. filename: "
               << filename;
    // If the file exists but is corrupted, the following operation may
    // may fix some problem. However, if the file was temporarily locked
    // by some processes and now no longer locked, the following operation
    // is likely to result in a simple permanent data loss.
    // TODO(yukawa, team): Do not clear the data whenever we can open the
    //     data file and the content is actually valid.
    if (!LruStorage::CreateStorageFile(filename, new_value_size, new_size,
                                       new_seed)) {
      LOG(ERROR) << "CreateStorageFile failed";
      return false;
    }
    if (!Open(filename)) {
      Close();
      LOG(ERROR) << "Open failed after CreateStorageFile. Give up...";
      return false;
    }
  }

  // File format has changed
  if (new_value_size != value_size() || new_size != size()) {
    Close();
    if (!LruStorage::CreateStorageFile(filename, new_value_size, new_size,
                                       new_seed)) {
      LOG(ERROR) << "CreateStorageFile failed";
      return false;
    }
    if (!Open(filename)) {
      Close();
      LOG(ERROR) << "Open failed after CreateStorageFile";
      return false;
    }
  }

  if (new_value_size != value_size() || new_size != size()) {
    Close();
    LOG(ERROR) << "file is broken";
    return false;
  }

  return true;
}

bool LruStorage::Open(const char *filename) {
  mmap_ = std::make_unique<Mmap>();

  if (!mmap_) {
    LOG(ERROR) << "cannot make Mmap object";
    return false;
  }

  if (!mmap_->Open(filename, "r+")) {
    LOG(ERROR) << "cannot open " << filename << " with read+write mode";
    return false;
  }

  if (mmap_->size() < 8) {
    LOG(ERROR) << "file size is too small";
    return false;
  }

  filename_ = filename;
  return Open(mmap_->begin(), mmap_->size());
}

bool LruStorage::Open(char *ptr, size_t ptr_size) {
  begin_ = ptr;
  end_ = ptr + ptr_size;

  uint32_t value_size_uint32 = 0;
  uint32_t size_uint32 = 0;

  ReadValue<uint32_t>(&begin_, &value_size_uint32);
  ReadValue<uint32_t>(&begin_, &size_uint32);
  ReadValue<uint32_t>(&begin_, &seed_);

  value_size_ = static_cast<size_t>(value_size_uint32);
  size_ = static_cast<size_t>(size_uint32);

  if (value_size_ % 4 != 0) {
    LOG(ERROR) << "value_size_ must be 4 byte alignment";
    return false;
  }

  if (size_ == 0 || size_ > kMaxLruSize) {
    LOG(ERROR) << "LRU size is invalid: " << size_;
    return false;
  }

  if (value_size_ == 0 || value_size_ > kMaxValueSize) {
    LOG(ERROR) << "value_size is invalid: " << value_size_;
    return false;
  }

  if (mmap_->size() != kFileHeaderSize + item_size() * size_) {
    LOG(ERROR) << "LRU file is broken";
    return false;
  }

  std::vector<char *> ary;
  for (char *begin = begin_; begin < end_; begin += item_size()) {
    ary.push_back(begin);
  }
  std::stable_sort(ary.begin(), ary.end(), CompareByTimeStamp());

  lru_list_.clear();
  lru_map_.clear();
  char *next = nullptr;
  for (size_t i = 0; i < ary.size(); ++i) {
    if (GetTimeStamp(ary[i]) != 0) {
      lru_list_.push_back(ary[i]);
      lru_map_[GetFP(ary[i])] = std::prev(lru_list_.end());
    } else if (next == nullptr) {
      next = ary[i];
    }
  }
  next_item_ = (next != nullptr) ? next : end_;
  DCHECK_LE(next_item_, end_);

  // At the time file is opened, perform clean up.
  DeleteElementsUntouchedFor62Days();

  return true;
}

void LruStorage::Close() {
  // Perform clean up before closing the file.
  DeleteElementsUntouchedFor62Days();

  filename_.clear();
  mmap_.reset();
  lru_list_.clear();
  lru_map_.clear();
}

const char *LruStorage::Lookup(const std::string &key) const {
  uint32_t last_access_time = 0;
  return Lookup(key, &last_access_time);
}

const char *LruStorage::Lookup(const std::string &key,
                               uint32_t *last_access_time) const {
  const uint64_t fp = Hash::FingerprintWithSeed(key, seed_);
  const auto it = lru_map_.find(fp);
  if (it == lru_map_.end()) {
    return nullptr;
  }
  const uint32_t timestamp = GetTimeStamp(*it->second);
  if (IsOlderThan62Days(timestamp)) {
    return nullptr;
  }
  *last_access_time = timestamp;
  return GetValue(*it->second);
}

void LruStorage::GetAllValues(std::vector<std::string> *values) const {
  DCHECK(values);
  values->clear();
  // Iterate data from the most recently used element to the least recently used
  // element.
  for (const char *ptr : lru_list_) {
    const uint32_t timestamp = GetTimeStamp(ptr);
    if (IsOlderThan62Days(timestamp)) {
      break;
    }
    // Default constructor of string is not applicable
    // because value's size() must return value_size_.
    DCHECK(ptr);
    values->emplace_back(GetValue(ptr), value_size_);
  }
}

bool LruStorage::Touch(const std::string &key) {
  const uint64_t fp = Hash::FingerprintWithSeed(key, seed_);
  auto it = lru_map_.find(fp);
  if (it == lru_map_.end()) {
    return false;
  }
  const uint32_t timestamp = GetTimeStamp(*it->second);
  if (IsOlderThan62Days(timestamp)) {
    return false;
  }
  Update(*it->second);
  // Move the node pointed to by it->second to the front.
  lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
  return true;
}

bool LruStorage::Insert(const std::string &key, const char *value) {
  if (value == nullptr) {
    return false;
  }
  const uint64_t fp = Hash::FingerprintWithSeed(key, seed_);

  // If the data corresponding to |key| already exists in LRU, update it.
  {
    auto it = lru_map_.find(fp);
    if (it != lru_map_.end()) {
      // Overwrite the data pointed to by it->second and move it to the front.
      Update(*it->second, fp, value, value_size_);
      lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
      return true;
    }
  }

  // If the LRU is full or we run out of the mmap region, drop the least
  // recently used element (actually, the least recently used element is
  // overwritten with new data).
  if (lru_map_.size() >= size_ || next_item_ == end_) {
    auto it = std::prev(lru_list_.end());  // Least recently used data.
    const uint64_t old_fp = GetFP(*it);
    lru_map_.erase(old_fp);
    lru_list_.splice(lru_list_.begin(), lru_list_, it);  // Move to front.
    Update(*it, fp, value, value_size_);
    lru_map_[fp] = it;
    return true;
  }

  // A new item can be assigned in the mmap region.
  if (next_item_ < end_) {
    Update(next_item_, fp, value, value_size_);
    lru_list_.push_front(next_item_);
    lru_map_[fp] = lru_list_.begin();
    // Advance next_item_ for next item.
    next_item_ += item_size();
    DCHECK_LE(next_item_, end_);
    return true;
  }

  LOG(ERROR) << "Insertion failed because no more mmap region is available.";
  return false;
}

bool LruStorage::TryInsert(const std::string &key, const char *value) {
  const uint64_t fp = Hash::FingerprintWithSeed(key, seed_);
  auto it = lru_map_.find(fp);
  if (it != lru_map_.end()) {
    Update(*it->second, fp, value, value_size_);
    lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
  }
  return true;
}

bool LruStorage::Delete(const std::string &key) {
  const uint64_t fp = Hash::FingerprintWithSeed(key, seed_);
  return Delete(fp);
}

bool LruStorage::Delete(uint64_t fp) {
  auto it = lru_map_.find(fp);
  return (it == lru_map_.end() || Delete(fp, it->second));
}

bool LruStorage::Delete(std::list<char *>::iterator it) {
  return (it == lru_list_.end() || Delete(GetFP(*it), it));
}

bool LruStorage::Delete(uint64_t fp, std::list<char *>::iterator it) {
  // Determine the last element in the mmap region.
  if (next_item_ < begin_ + item_size()) {
    LOG(ERROR) << "next_item_ points to invalid location (broken?)";
    return false;
  }
  next_item_ -= item_size();

  // Backup the location of mmap region to which another element will be moved.
  char *deleted_item_pos = *it;

  // Erase the LRU structure for (fp, it).
  lru_map_.erase(fp);
  lru_list_.erase(it);

  if (next_item_ != deleted_item_pos) {
    // Move the region for the last element to the deleted location.  Then,
    // update the LRU structure for the moved element (the pointer to mmap
    // region in the list node is updated.)
    std::memcpy(deleted_item_pos, next_item_, item_size());
    const uint64_t fp = GetFP(next_item_);
    *lru_map_[fp] = deleted_item_pos;
  }

  // Clear the region for the next_item_.
  std::memset(next_item_, 0, item_size());

  return true;
}

int LruStorage::DeleteElementsBefore(uint32_t timestamp) {
  if (!mmap_ || begin_ >= end_) {
    return 0;
  }
  int num_deleted = 0;
  while (!lru_list_.empty()) {
    auto it = std::prev(lru_list_.end());
    const uint32_t last_access_time = GetTimeStamp(*it);
    if (last_access_time >= timestamp) {
      break;
    }
    if (Delete(it)) {
      ++num_deleted;
      continue;
    }
    LOG(ERROR) << "Deletion failed for an item.  Abort deletion.";
    break;
  }
  return num_deleted;
}

int LruStorage::DeleteElementsUntouchedFor62Days() {
  const uint64_t now = Clock::GetTime();
  const uint32_t timestamp =
      static_cast<uint32_t>((now > k62DaysInSec) ? now - k62DaysInSec : 0);
  return DeleteElementsBefore(timestamp);
}

size_t LruStorage::item_size() const { return value_size_ + kItemHeaderSize; }

size_t LruStorage::value_size() const { return value_size_; }

size_t LruStorage::size() const { return size_; }

size_t LruStorage::used_size() const { return lru_list_.size(); }

uint32_t LruStorage::seed() const { return seed_; }

const std::string &LruStorage::filename() const { return filename_; }

void LruStorage::Write(size_t i, uint64_t fp, const std::string &value,
                       uint32_t last_access_time) {
  DCHECK_LT(i, size_);
  char *ptr = begin_ + (i * item_size());
  memcpy(ptr, reinterpret_cast<const char *>(&fp), 8);
  memcpy(ptr + 8, reinterpret_cast<const char *>(&last_access_time), 4);
  if (value.size() == value_size_) {
    memcpy(ptr + kItemHeaderSize, value.data(), value_size_);
  } else {
    LOG(ERROR) << "value size is not " << value_size_ << " byte.";
  }
}

void LruStorage::Read(size_t i, uint64_t *fp, std::string *value,
                      uint32_t *last_access_time) const {
  DCHECK_LT(i, size_);
  const char *ptr = begin_ + (i * item_size());
  *fp = GetFP(ptr);
  value->assign(GetValue(ptr), value_size_);
  *last_access_time = GetTimeStamp(ptr);
}

}  // namespace storage
}  // namespace mozc
