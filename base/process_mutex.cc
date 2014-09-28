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

#include "base/process_mutex.h"

#ifdef OS_WIN
#include <windows.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#endif  // OS_WIN

#include <map>
#include <string>
#include <utility>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/mutex.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#ifdef OS_WIN
#include "base/win_sandbox.h"
#endif  // OS_WIN

namespace mozc {

namespace {

string CreateProcessMutexFileName(const char *name) {
  name = (name == NULL) ? "NULL" : name;

#ifdef OS_WIN
  string basename;
#else
  string basename = ".";
#endif

  basename += name;
  basename += ".lock";

  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), basename);
}
}  // namespace

bool ProcessMutex::Lock() {
  return LockAndWrite("");
}

#ifdef OS_WIN

ProcessMutex::ProcessMutex(const char *name)
    : handle_(INVALID_HANDLE_VALUE), locked_(false) {
  filename_ = CreateProcessMutexFileName(name);
}

ProcessMutex::~ProcessMutex() {
  UnLock();
}

bool ProcessMutex::LockAndWrite(const string &message) {
  if (locked_) {
    VLOG(1) << filename_ << " is already locked";
    return false;
  }

  wstring wfilename;
  Util::UTF8ToWide(filename_.c_str(), &wfilename);
  const DWORD kAttribute =
      FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
      FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED |
      FILE_FLAG_DELETE_ON_CLOSE;

  SECURITY_ATTRIBUTES serucity_attributes = {};
  if (!WinSandbox::MakeSecurityAttributes(WinSandbox::kSharableFileForRead,
                                          &serucity_attributes)) {
    return false;
  }
  handle_.reset(::CreateFileW(
      wfilename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &serucity_attributes,
      CREATE_ALWAYS, kAttribute, NULL));
  ::LocalFree(serucity_attributes.lpSecurityDescriptor);

  locked_ = (handle_.get() != NULL);

  if (!locked_) {
    VLOG(1) << "already locked";
    return locked_;
  }

  if (!message.empty()) {
    DWORD size = 0;
    if (!::WriteFile(handle_.get(), message.data(), message.size(),
                     &size, NULL)) {
      const int last_error = ::GetLastError();
      LOG(ERROR) << "Cannot write message: " << message
                 << ", last_error:" << last_error;
      UnLock();
      return false;
    }
  }

  return locked_;
}

bool ProcessMutex::UnLock() {
  handle_.reset(NULL);
  FileUtil::Unlink(filename_);
  locked_ = false;
  return true;
}

#elif defined(MOZC_USE_PEPPER_FILE_IO)  // OS_WIN
namespace {

// In NaCl we can't consider about the processes.
// So we just implement inprocess named lock service.
class NamedLockManager {
 public:
  NamedLockManager() {}
  ~NamedLockManager() {}
  bool Lock(const string &filename, const string &message) {
    scoped_lock l(&mutex_);
    return lock_map_.insert(make_pair(filename, message)).second;
  }
  void UnLock(const string &filename) {
    scoped_lock l(&mutex_);
    lock_map_.erase(filename);
    return;
  }

 private:
  Mutex mutex_;
  map<string, string> lock_map_;
  DISALLOW_COPY_AND_ASSIGN(NamedLockManager);
};

}  // namespace

ProcessMutex::ProcessMutex(const char *name) : locked_(false) {
  filename_ = CreateProcessMutexFileName(name);
}

ProcessMutex::~ProcessMutex() {
  UnLock();
}

bool ProcessMutex::LockAndWrite(const string &message) {
  return Singleton<NamedLockManager>::get()->Lock(filename_, message);
}

bool ProcessMutex::UnLock() {
  Singleton<NamedLockManager>::get()->UnLock(filename_);
  return true;
}

#else  // !OS_WIN && !MOZC_USE_PEPPER_FILE_IO

namespace {
// Special workaround for the bad treatment of fcntl.
// Basically, fcntl provides "per-process" file-locking.
// When a process has multiple fds on the same file,
// file lock is released if one fd is closed. This is not
// an expected behavior for Mozc.
//
// Linux man says
// "As well as being removed by an explicit F_UNLCK, record locks are
// automatically released when the process terminates or
// if it closes any file descriptor referring to a file on which
// locks are held.  This is bad: it means that a process can lose
// the locks on a file like /etc/passwd or /etc/mtab when
// for some reason a library function decides to open,
// read and close it."
//
// FileLockerManager a wrapper class providing per-filename
// file locking implemented upon fcntl. Multiple threads on
//  the same process share one fd for file locking.
//
// We will be able to use flock() instead of fcntl since,
// flock() provides "per-fd" file locking. However, we don't
// use it because flock() doesn't work on NFS.
class FileLockManager {
 public:
  bool Lock(const string &filename, int *fd) {
    scoped_lock l(&mutex_);

    if (fd == NULL) {
      LOG(ERROR) << "fd is NULL";
      return false;
    }

    if (filename.empty()) {
      LOG(ERROR) << "filename is empty";
      return false;
    }

    if (fdmap_.find(filename) != fdmap_.end()) {
      VLOG(1) << filename << " is already locked by the same process";
      return false;
    }

    chmod(filename.c_str(), 0600);  // write temporary
    *fd = ::open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (-1 == *fd) {
      LOG(ERROR) << "open() failed:" << ::strerror(errno);
      return false;
    }

    struct flock command;
    command.l_type = F_WRLCK;
    command.l_whence = SEEK_SET;
    command.l_start = 0;
    command.l_len = 0;

    const int result = ::fcntl(*fd, F_SETLK, &command);
    if (-1 == result) {  // failed
      ::close(*fd);
      LOG(WARNING) << "already locked";
      return false;   // another server is already running
    }

    fdmap_.insert(make_pair(filename, *fd));

    return true;
  }

  void UnLock(const string &filename) {
    scoped_lock l(&mutex_);
    map<string, int>::iterator it = fdmap_.find(filename);
    if (it == fdmap_.end()) {
      LOG(ERROR) << filename << " is not locked";
      return;
    }
    ::close(it->second);
    FileUtil::Unlink(filename);
    fdmap_.erase(it);
  }

  FileLockManager() {}

  ~FileLockManager() {
    for (map<string, int>::const_iterator it = fdmap_.begin();
         it != fdmap_.end(); ++it) {
      ::close(it->second);
    }
    fdmap_.clear();
  }

 private:
  Mutex mutex_;
  map<string, int> fdmap_;
};

}  // namespace

ProcessMutex::ProcessMutex(const char *name) : locked_(false) {
  filename_ = CreateProcessMutexFileName(name);
}

ProcessMutex::~ProcessMutex() {
  if (locked_) {
    UnLock();
  }
}

bool ProcessMutex::LockAndWrite(const string &message) {
  int fd = -1;
  if (!Singleton<FileLockManager>::get()->Lock(filename_, &fd)) {
    VLOG(1) << filename_ << " is already locked";
    return false;
  }

  if (fd == -1) {
    LOG(ERROR) << "file descriptor is -1";
    return false;
  }

  if (!message.empty()) {
    if (write(fd, message.data(), message.size()) !=
        message.size()) {
      LOG(ERROR) << "Cannot write message: " << message;
      UnLock();
      return false;
    }
  }

  // changed it to read only mode.
  chmod(filename_.c_str(), 0400);
  locked_ = true;
  return true;
}

bool ProcessMutex::UnLock() {
  if (locked_) {
    Singleton<FileLockManager>::get()->UnLock(filename_);
  }
  locked_ = false;
  return true;
}
#endif
}  // namespace mozc
