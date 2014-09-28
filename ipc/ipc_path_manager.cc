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

#include "ipc/ipc_path_manager.h"

#include <errno.h>

#ifdef OS_WIN
#include <windows.h>
#include <psapi.h>   // GetModuleFileNameExW
#else
// For stat system call
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef OS_MACOSX
#include <sys/sysctl.h>
#endif  // OS_MACOSX
#endif  // OS_WIN

#include <cstdlib>
#include <map>
#ifdef OS_WIN
#include <memory>  // for std::unique_ptr
#endif

#include "base/const.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/mac_util.h"
#include "base/mutex.h"
#include "base/port.h"
#include "base/process_mutex.h"
#include "base/scoped_handle.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/version.h"
#include "base/win_util.h"
#include "ipc/ipc.h"
#include "ipc/ipc.pb.h"

#ifdef OS_WIN
using std::unique_ptr;
#endif  // OS_WIn

namespace mozc {
namespace {

// size of key
const size_t kKeySize = 32;

// Do not use ConfigFileStream, since client won't link
// to the embedded resource files
string GetIPCKeyFileName(const string &name) {
#ifdef OS_WIN
  string basename;
#else
  string basename = ".";    // hidden file
#endif
  basename += name + ".ipc";
  return FileUtil::JoinPath(SystemUtil::GetUserProfileDirectory(), basename);
}

bool IsValidKey(const string &name) {
  if (kKeySize != name.size()) {
    LOG(ERROR) << "IPCKey is invalid length";
    return false;
  }

  for (size_t i = 0; i < name.size(); ++i) {
    if ((name[i] >= '0' && name[i] <= '9') ||
        (name[i] >= 'a' && name[i] <= 'f')) {
      // do nothing
    } else {
      LOG(ERROR) << "key name is invalid: " << name[i];
      return false;
    }
  }

  return true;
}

void CreateIPCKey(char *value) {
  char buf[16];   // key is 128 bit

#ifdef OS_WIN
  // LUID guaranties uniqueness
  LUID luid = { 0 };   // LUID is 64bit value

  DCHECK_EQ(sizeof(luid), sizeof(uint64));

  // first 64 bit is random sequence and last 64 bit is LUID
  if (::AllocateLocallyUniqueId(&luid)) {
    Util::GetRandomSequence(buf, sizeof(buf) / 2);
    ::memcpy(buf + sizeof(buf) / 2, &luid, sizeof(buf) / 2);
  } else {
    // use random value for failsafe
    Util::GetRandomSequence(buf, sizeof(buf));
  }
#else
  // get 128 bit key: Note that collision will happen.
  Util::GetRandomSequence(buf, sizeof(buf));
#endif

  // escape
  for (size_t i = 0; i < sizeof(buf); ++i) {
    const int hi = ((static_cast<int>(buf[i]) & 0xF0) >> 4);
    const int lo = (static_cast<int>(buf[i]) & 0x0F);
    value[2 * i]     = static_cast<char>(hi >= 10 ? hi - 10 + 'a' : hi + '0');
    value[2 * i + 1] = static_cast<char>(lo >= 10 ? lo - 10 + 'a' : lo + '0');
  }

  value[kKeySize] = '\0';
}

class IPCPathManagerMap {
 public:
  IPCPathManager *GetIPCPathManager(const string &name) {
    scoped_lock l(&mutex_);
    map<string, IPCPathManager *>::const_iterator it = manager_map_.find(name);
    if (it != manager_map_.end()) {
      return it->second;
    }
    IPCPathManager *manager = new IPCPathManager(name);
    manager_map_.insert(make_pair(name, manager));
    return manager;
  }

  IPCPathManagerMap() {}

  ~IPCPathManagerMap() {
    scoped_lock l(&mutex_);
    for (map<string, IPCPathManager *>::iterator it = manager_map_.begin();
         it != manager_map_.end(); ++it) {
      delete it->second;
    }
    manager_map_.clear();
  }

 private:
  map<string, IPCPathManager *> manager_map_;
  Mutex mutex_;
};

}  // namespace

IPCPathManager::IPCPathManager(const string &name)
    : mutex_(new Mutex),
      ipc_path_info_(new ipc::IPCPathInfo),
      name_(name),
      server_pid_(0),
      last_modified_(-1) {}

IPCPathManager::~IPCPathManager() {}

IPCPathManager *IPCPathManager::GetIPCPathManager(const string &name) {
  IPCPathManagerMap *manager_map = Singleton<IPCPathManagerMap>::get();
  DCHECK(manager_map != NULL);
  return manager_map->GetIPCPathManager(name);
}

bool IPCPathManager::CreateNewPathName() {
  scoped_lock l(mutex_.get());
  if (ipc_path_info_->key().empty()) {
    char ipc_key[kKeySize + 1];
    CreateIPCKey(ipc_key);
    ipc_path_info_->set_key(ipc_key);
  }
  return true;
}

bool IPCPathManager::SavePathName() {
  scoped_lock l(mutex_.get());
  if (path_mutex_.get() != NULL) {
    return true;
  }

  path_mutex_.reset(new ProcessMutex("ipc"));

  path_mutex_->set_lock_filename(GetIPCKeyFileName(name_));

  // a little tricky as CreateNewPathName() tries mutex lock
  CreateNewPathName();

  // set the server version
  ipc_path_info_->set_protocol_version(IPC_PROTOCOL_VERSION);
  ipc_path_info_->set_product_version(Version::GetMozcVersion());

#ifdef OS_WIN
  ipc_path_info_->set_process_id(static_cast<uint32>(::GetCurrentProcessId()));
  ipc_path_info_->set_thread_id(static_cast<uint32>(::GetCurrentThreadId()));
#else
  ipc_path_info_->set_process_id(static_cast<uint32>(getpid()));
  ipc_path_info_->set_thread_id(0);
#endif

  string buf;
  if (!ipc_path_info_->SerializeToString(&buf)) {
    LOG(ERROR) << "SerializeToString failed";
    return false;
  }

  if (!path_mutex_->LockAndWrite(buf)) {
    LOG(ERROR) << "ipc key file is already locked";
    return false;
  }

  VLOG(1) << "ServerIPCKey: " << ipc_path_info_->key();

  last_modified_ = GetIPCFileTimeStamp();
  return true;
}

bool IPCPathManager::LoadPathName() {
  // On Windows, ShouldReload() always returns false.
  // On other platform, it returns true when timestamp of the file is different
  // from that of previous one.
  if (ShouldReload() || ipc_path_info_->key().empty()) {
    if (!LoadPathNameInternal()) {
      LOG(ERROR) << "LoadPathName failed";
      return false;
    }
  }
  return true;
}

bool IPCPathManager::GetPathName(string *ipc_name) const {
  if (ipc_name == NULL) {
    LOG(ERROR) << "ipc_name is NULL";
    return false;
  }

  if (ipc_path_info_->key().empty()) {
    LOG(ERROR) << "ipc_path_info_ is empty";
    return false;
  }

#ifdef OS_WIN
  *ipc_name = mozc::kIPCPrefix;
#elif defined(OS_MACOSX)
  ipc_name->assign(MacUtil::GetLabelForSuffix(""));
#else  // not OS_WIN nor OS_MACOSX
  // GetUserIPCName("<name>") => "/tmp/.mozc.<key>.<name>"
  const char kIPCPrefix[] = "/tmp/.mozc.";
  *ipc_name = kIPCPrefix;
#endif  // OS_WIN

#ifdef OS_LINUX
  // On Linux, use abstract namespace which is independent of the file system.
  (*ipc_name)[0] = '\0';
#endif

  ipc_name->append(ipc_path_info_->key());
  ipc_name->append(".");
  ipc_name->append(name_);

  return true;
}

uint32 IPCPathManager::GetServerProtocolVersion() const {
  return ipc_path_info_->protocol_version();
}

const string &IPCPathManager::GetServerProductVersion() const {
  return ipc_path_info_->product_version();
}

uint32 IPCPathManager::GetServerProcessId() const {
  return ipc_path_info_->process_id();
}

void IPCPathManager::Clear() {
  scoped_lock l(mutex_.get());
  ipc_path_info_->Clear();
}

bool IPCPathManager::IsValidServer(uint32 pid,
                                   const string &server_path) {
  scoped_lock l(mutex_.get());
  if (pid == 0) {
    // For backward compatibility.
    return true;
  }
  if (server_path.empty()) {
    // This means that we do not check the server path.
    return true;
  }

  if (pid == static_cast<uint32>(-1)) {
    VLOG(1) << "pid is -1. so assume that it is an invalid program";
    return false;
  }

  // compare path name
  if (pid == server_pid_) {
    return (server_path == server_path_);
  }

  server_pid_ = 0;
  server_path_.clear();

#ifdef OS_WIN
  {
    DCHECK(SystemUtil::IsVistaOrLater())
        << "This verification is functional on Vista and later.";

    wstring expected_server_ntpath;
    const map<string, wstring>::const_iterator it =
        expected_server_ntpath_cache_.find(server_path);
    if (it != expected_server_ntpath_cache_.end()) {
      expected_server_ntpath = it->second;
    } else {
      wstring wide_server_path;
      Util::UTF8ToWide(server_path, &wide_server_path);
      if (WinUtil::GetNtPath(wide_server_path, &expected_server_ntpath)) {
        // Caches the relationship from |server_path| to
        // |expected_server_ntpath| in case |server_path| is renamed later.
        // (This can happen during the updating).
        expected_server_ntpath_cache_[server_path] = expected_server_ntpath;
      }
    }

    if (expected_server_ntpath.empty()) {
      return false;
    }

    wstring actual_server_ntpath;
    if (!WinUtil::GetProcessInitialNtPath(pid, &actual_server_ntpath)) {
      return false;
    }

    if (expected_server_ntpath != actual_server_ntpath) {
      return false;
    }

    // Here we can safely assume that |server_path| (expected one) should be
    // the same to |server_path_| (actual one).
    server_path_ = server_path;
    server_pid_ = pid;
  }
#endif  // OS_WIN

#ifdef OS_MACOSX
  int name[] = { CTL_KERN, KERN_PROCARGS, pid };
  size_t data_len = 0;
  if (sysctl(name, arraysize(name), NULL,
             &data_len, NULL, 0) < 0) {
    LOG(ERROR) << "sysctl KERN_PROCARGS failed";
    return false;
  }

  server_path_.resize(data_len);
  if (sysctl(name, arraysize(name), &server_path_[0],
             &data_len, NULL, 0) < 0) {
    LOG(ERROR) << "sysctl KERN_PROCARGS failed";
    return false;
  }
  server_pid_ = pid;
#endif  // OS_MACOSX

#ifdef OS_LINUX
  // load from /proc/<pid>/exe
  char proc[128];
  char filename[512];
  snprintf(proc, sizeof(proc) - 1, "/proc/%u/exe", pid);
  const ssize_t size = readlink(proc, filename, sizeof(filename) - 1);
  if (size == -1) {
    LOG(ERROR) << "readlink failed: " << strerror(errno);
    return false;
  }
  filename[size] = '\0';

  server_path_ = filename;
  server_pid_ = pid;
#endif  // OS_LINUX

  VLOG(1) << "server path: " << server_path << " " << server_path_;
  if (server_path == server_path_) {
    return true;
  }

#ifdef OS_LINUX
  if ((server_path + " (deleted)") == server_path_) {
    LOG(WARNING) << server_path << " on disk is modified";
    // If a user updates the server binary on disk during the server is running,
    // "readlink /proc/<pid>/exe" returns a path with the " (deleted)" suffix.
    // We allow the special case.
    server_path_ = server_path;
    return true;
  }
#endif  // OS_LINUX

  return false;
}

bool IPCPathManager::ShouldReload() const {
#ifdef OS_WIN
  // In windows, no reloading mechanism is necessary because IPC files
  // are automatically removed.
  return false;
#else
  scoped_lock l(mutex_.get());

  time_t last_modified = GetIPCFileTimeStamp();
  if (last_modified == last_modified_) {
    return false;
  }

  return true;
#endif  // OS_WIN
}

time_t IPCPathManager::GetIPCFileTimeStamp() const {
#ifdef OS_WIN
  // In windows, we don't need to get the exact file timestamp, so
  // just returns -1 at this time.
  return static_cast<time_t>(-1);
#else
  const string filename = GetIPCKeyFileName(name_);
  struct stat filestat;
  if (::stat(filename.c_str(), &filestat) == -1) {
    VLOG(2) << "stat(2) failed.  Skipping reload";
    return static_cast<time_t>(-1);
  }
  return filestat.st_mtime;
#endif  // OS_WIN
}

bool IPCPathManager::LoadPathNameInternal() {
  scoped_lock l(mutex_.get());

  // try the new file name first.
  const string filename = GetIPCKeyFileName(name_);

  // Special code for Windows,
  // we want to pass FILE_SHRED_DELETE flag for CreateFile.
#ifdef OS_WIN
  wstring wfilename;
  Util::UTF8ToWide(filename.c_str(), &wfilename);

  {
    ScopedHandle handle
        (::CreateFileW(wfilename.c_str(),
                       GENERIC_READ,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, 0, NULL));

    // ScopedHandle does not receive INVALID_HANDLE_VALUE and
    // NULL check is appropriate here.
    if (NULL == handle.get()) {
      LOG(ERROR) << "cannot open: " << filename << " " << ::GetLastError();
      return false;
    }

    const DWORD size = ::GetFileSize(handle.get(), NULL);
    if (-1 == static_cast<int>(size)) {
      LOG(ERROR) << "GetFileSize failed: " << ::GetLastError();
      return false;
    }

    const DWORD kMaxFileSize = 2096;
    if (size == 0 || size >= kMaxFileSize) {
      LOG(ERROR) << "Invalid file size: " << kMaxFileSize;
      return false;
    }

    unique_ptr<char[]> buf(new char[size]);

    DWORD read_size = 0;
    if (!::ReadFile(handle.get(), buf.get(),
                    size, &read_size, NULL)) {
      LOG(ERROR) << "ReadFile failed: " << ::GetLastError();
      return false;
    }

    if (read_size != size) {
      LOG(ERROR) << "read_size != size";
      return false;
    }

    if (!ipc_path_info_->ParseFromArray(buf.get(), static_cast<int>(size))) {
      LOG(ERROR) << "ParseFromStream failed";
      return false;
    }
  }

#else

  InputFileStream is(filename.c_str(), ios::binary|ios::in);
  if (!is) {
    LOG(ERROR) << "cannot open: " << filename;
    return false;
  }

  if (!ipc_path_info_->ParseFromIstream(&is)) {
    LOG(ERROR) << "ParseFromStream failed";
    return false;
  }
#endif

  if (!IsValidKey(ipc_path_info_->key())) {
    LOG(ERROR) << "IPCServer::key is invalid";
    return false;
  }

  VLOG(1) << "ClientIPCKey: " << ipc_path_info_->key();
  VLOG(1) << "ProtocolVersion: " << ipc_path_info_->protocol_version();

  last_modified_ = GetIPCFileTimeStamp();
  return true;
}
}  // namespace mozc
