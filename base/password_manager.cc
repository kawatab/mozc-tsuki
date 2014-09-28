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

#include "base/password_manager.h"

#include <stddef.h>
#ifdef OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif  // OS_WIN

#include <cstdlib>
#include <string>

#include "base/const.h"
#include "base/encryptor.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mmap.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"

namespace mozc {
namespace {

#ifdef OS_WIN
const char  kPasswordFile[] = "encrypt_key.db";
#else
const char  kPasswordFile[] = ".encrypt_key.db";   // dot-file (hidden file)
#endif

const size_t kPasswordSize  = 32;

string CreateRandomPassword() {
  char buf[kPasswordSize];
  Util::GetRandomSequence(buf, sizeof(buf));
  return string(buf, sizeof(buf));
}

// RAII class to make a given file writable/read-only
class ScopedReadWriteFile {
 public:
  explicit ScopedReadWriteFile(const string &filename)
      : filename_(filename) {
    if (!FileUtil::FileExists(filename_)) {
      LOG(WARNING) << "file not found: " << filename;
      return;
    }
#ifdef OS_WIN
    wstring wfilename;
    Util::UTF8ToWide(filename_.c_str(), &wfilename);
    if (!::SetFileAttributesW(wfilename.c_str(), FILE_ATTRIBUTE_NORMAL)) {
      LOG(ERROR) << "Cannot make writable: " << filename_;
    }
#elif !defined(MOZC_USE_PEPPER_FILE_IO)
    chmod(filename_.c_str(), 0600);  // write temporary
#endif
  }

  ~ScopedReadWriteFile() {
    if (!FileUtil::FileExists(filename_)) {
      LOG(WARNING) << "file not found: " << filename_;
      return;
    }
#ifdef OS_WIN
    if (!FileUtil::HideFileWithExtraAttributes(filename_,
                                               FILE_ATTRIBUTE_READONLY)) {
      LOG(ERROR) << "Cannot make readonly: " << filename_;
    }
#elif !defined(MOZC_USE_PEPPER_FILE_IO)
    chmod(filename_.c_str(), 0400);  // read only
#endif
  }

 private:
  string filename_;
};

string GetFileName() {
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(),
                            kPasswordFile);
}

bool SavePassword(const string &password) {
  const string filename = GetFileName();
  ScopedReadWriteFile l(filename);

  {
    OutputFileStream ofs(filename.c_str(), ios::out | ios::binary);
    if (!ofs) {
      LOG(ERROR) << "cannot open: " << filename;
      return false;
    }
    ofs.write(password.data(), password.size());
  }

  return true;
}

bool LoadPassword(string *password) {
  const string filename = GetFileName();
  Mmap mmap;
  if (!mmap.Open(filename.c_str(), "r")) {
    LOG(ERROR) << "cannot open: " << filename;
    return false;
  }

  // Seems that the size of DPAPI-encrypted-mesage
  // becomes bigger than the original message.
  // Typical file size is 32 * 5 = 160byte.
  // We just set the maximum file size to be 4096byte just in case.
  if (mmap.size() == 0 || mmap.size() > 4096) {
    LOG(ERROR) << "Invalid password is saved";
    return false;
  }

  password->assign(mmap.begin(), mmap.size());

  return true;
}

bool RemovePasswordFile() {
  const string filename = GetFileName();
  ScopedReadWriteFile l(filename);
  return FileUtil::Unlink(filename);
}
}  // namespace

//////////////////////////////////////////////////////////////////
// PlainPasswordManager
class PlainPasswordManager : public PasswordManagerInterface {
 public:
  virtual bool SetPassword(const string &password) const;
  virtual bool GetPassword(string *password) const;
  virtual bool RemovePassword() const;
};

bool PlainPasswordManager::SetPassword(const string &password) const {
  if (password.size() != kPasswordSize) {
    LOG(ERROR) << "Invalid password given";
    return false;
  }

  if (!SavePassword(password)) {
    LOG(ERROR) << "SavePassword failed";
    return false;
  }

  return true;
}

bool PlainPasswordManager::GetPassword(string *password) const {
  if (password == NULL) {
    LOG(ERROR) << "password is NULL";
    return false;
  }

  password->clear();

  if (!LoadPassword(password)) {
    LOG(ERROR) << "LoadPassword failed";
    return false;
  }

  if (password->size() != kPasswordSize) {
    LOG(ERROR) << "Password size is invalid";
    return false;
  }

  return true;
}

bool PlainPasswordManager::RemovePassword() const {
  return RemovePasswordFile();
}

//////////////////////////////////////////////////////////////////
// WinPasswordManager
// We use this manager with both Windows and Mac
#if (defined(OS_WIN) || defined(OS_MACOSX))
class WinMacPasswordManager : public PasswordManagerInterface {
 public:
  virtual bool SetPassword(const string &password) const;
  virtual bool GetPassword(string *password) const;
  virtual bool RemovePassword() const;
};

bool WinMacPasswordManager::SetPassword(const string &password) const {
  if (password.size() != kPasswordSize) {
    LOG(ERROR) << "password size is invalid";
    return false;
  }

  string enc_password;
  if (!Encryptor::ProtectData(password, &enc_password)) {
    LOG(ERROR) << "ProtectData failed";
    return false;
  }

  return SavePassword(enc_password);
}

bool WinMacPasswordManager::GetPassword(string *password) const {
  if (password == NULL) {
    LOG(ERROR) << "password is NULL";
    return false;
  }

  string enc_password;
  if (!LoadPassword(&enc_password)) {
    LOG(ERROR) << "LoadPassword failed";
    return false;
  }

  password->clear();
  if (!Encryptor::UnprotectData(enc_password, password)) {
    LOG(ERROR) << "UnprotectData failed";
    return false;
  }

  if (password->size() != kPasswordSize) {
    LOG(ERROR) << "password size is invalid";
    return false;
  }

  return true;
}

bool WinMacPasswordManager::RemovePassword() const {
  return RemovePasswordFile();
}
#endif  // OS_WIN | OS_MACOSX

// We use plain text file for password storage on Linux. If you port this module
// to other Linux distro, you might want to implement a new password manager
// which adopts some secure mechanism such like gnome-keyring.
#if defined OS_LINUX
typedef PlainPasswordManager DefaultPasswordManager;
#endif

// Windows or Mac
#if (defined(OS_WIN) || defined(OS_MACOSX))
typedef WinMacPasswordManager DefaultPasswordManager;
#endif

namespace {
class PasswordManagerImpl {
 public:
  PasswordManagerImpl() {
    password_manager_ = Singleton<DefaultPasswordManager>::get();
    DCHECK(password_manager_ != NULL);
  }

  bool InitPassword() {
    string password;
    if (password_manager_->GetPassword(&password)) {
      return true;
    }
    password = CreateRandomPassword();
    scoped_lock l(&mutex_);
    return password_manager_->SetPassword(password);
  }

  bool GetPassword(string *password) {
    scoped_lock l(&mutex_);
    if (password_manager_->GetPassword(password)) {
      return true;
    }

    LOG(WARNING) << "Cannot get password. call InitPassword";

    if (!InitPassword()) {
      LOG(ERROR) << "InitPassword failed";
      return false;
    }

    if (!password_manager_->GetPassword(password)) {
      LOG(ERROR) << "Cannot get password.";
      return false;
    }

    return true;
  }

  bool RemovePassword() {
    scoped_lock l(&mutex_);
    return password_manager_->RemovePassword();
  }

  void SetPasswordManagerHandler(PasswordManagerInterface *handler) {
    scoped_lock l(&mutex_);
    password_manager_ = handler;
  }

 public:
  PasswordManagerInterface *password_manager_;
  Mutex mutex_;
};
}  // anonymous namespace

bool PasswordManager::InitPassword() {
  return Singleton<PasswordManagerImpl>::get()->InitPassword();
}

bool PasswordManager::GetPassword(string *password) {
  return Singleton<PasswordManagerImpl>::get()->GetPassword(password);
}

// remove current password
bool PasswordManager::RemovePassword() {
  return Singleton<PasswordManagerImpl>::get()->RemovePassword();
}

// set internal interface for unittesting
void PasswordManager::SetPasswordManagerHandler(
    PasswordManagerInterface *handler) {
  Singleton<PasswordManagerImpl>::get()->SetPasswordManagerHandler(handler);
}
}  // namespace mozc
