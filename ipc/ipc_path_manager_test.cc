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

#include <string>
#include <vector>

#include "base/port.h"
#include "base/file_stream.h"
#include "base/file_util.h"
#include "base/process_mutex.h"
#include "base/system_util.h"
#include "base/thread.h"
#include "base/util.h"
#include "base/version.h"
#include "ipc/ipc.h"
#include "ipc/ipc.pb.h"
#include "testing/base/public/gunit.h"

DECLARE_string(test_tmpdir);

namespace mozc {
namespace {

class CreateThread : public Thread {
 public:
  virtual void Run() {
    IPCPathManager *manager =
        IPCPathManager::GetIPCPathManager("test");
    EXPECT_TRUE(manager->CreateNewPathName());
    EXPECT_TRUE(manager->SavePathName());
    EXPECT_TRUE(manager->GetPathName(&path_));
    EXPECT_GT(manager->GetServerProtocolVersion(), 0);
    EXPECT_FALSE(manager->GetServerProductVersion().empty());
    EXPECT_GT(manager->GetServerProcessId(), 0);
  }

  const string &path() const {
    return path_;
  }

 private:
  string path_;
};

class BatchGetPathNameThread : public Thread {
 public:
  virtual void Run() {
    for (int i = 0; i < 100; ++i) {
      IPCPathManager *manager =
          IPCPathManager::GetIPCPathManager("test2");
      string path;
      EXPECT_TRUE(manager->CreateNewPathName());
      EXPECT_TRUE(manager->GetPathName(&path));
    }
  }
};
}  // anonymous namespace

class IPCPathManagerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  }
};

TEST_F(IPCPathManagerTest, IPCPathManagerTest) {
  CreateThread t;
  t.Start();
  Util::Sleep(1000);
  IPCPathManager *manager =
      IPCPathManager::GetIPCPathManager("test");
  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());
  string path;
  EXPECT_TRUE(manager->GetPathName(&path));
  EXPECT_GT(manager->GetServerProtocolVersion(), 0);
  EXPECT_FALSE(manager->GetServerProductVersion().empty());
  EXPECT_GT(manager->GetServerProcessId(), 0);
  EXPECT_EQ(t.path(), path);
#ifdef OS_LINUX
  // On Linux, |path| should be abstract (see man unix(7) for details.)
  ASSERT_FALSE(path.empty());
  EXPECT_EQ('\0', path[0]);
#endif
}

// Test the thread-safeness of GetPathName() and
// GetIPCPathManager
TEST_F(IPCPathManagerTest, IPCPathManagerBatchTest) {
  // mozc::Thread is not designed as value-semantics.
  // So here we use pointers to maintain these instances.
  vector<BatchGetPathNameThread *> threads(8192);
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i] = new BatchGetPathNameThread;
    threads[i]->Start();
  }
  for (size_t i = 0; i < threads.size(); ++i) {
    threads[i]->Join();
    delete threads[i];
    threads[i] = NULL;
  }
}

TEST_F(IPCPathManagerTest, ReloadTest) {
  // We have only mock implementations for Windows, so no test should be run.
#ifndef OS_WIN
  IPCPathManager *manager =
      IPCPathManager::GetIPCPathManager("reload_test");

  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());

  // Just after the save, there are no need to reload.
  EXPECT_FALSE(manager->ShouldReload());

  // Modify the saved file explicitly.
  EXPECT_TRUE(manager->path_mutex_->UnLock());
  Util::Sleep(1000);  // msec
  string filename = FileUtil::JoinPath(
      SystemUtil::GetUserProfileDirectory(), ".reload_test.ipc");
  OutputFileStream outf(filename.c_str());
  outf << "foobar";
  outf.close();

  EXPECT_TRUE(manager->ShouldReload());
#endif  // OS_WIN
}

TEST_F(IPCPathManagerTest, PathNameTest) {
  IPCPathManager *manager =
      IPCPathManager::GetIPCPathManager("path_name_test");

  EXPECT_TRUE(manager->CreateNewPathName());
  EXPECT_TRUE(manager->SavePathName());
  const ipc::IPCPathInfo original_path = *(manager->ipc_path_info_);
  EXPECT_EQ(IPC_PROTOCOL_VERSION, original_path.protocol_version());
  // Checks that versions are same.
  EXPECT_EQ(Version::GetMozcVersion(), original_path.product_version());
  EXPECT_TRUE(original_path.has_key());
  EXPECT_TRUE(original_path.has_process_id());
  EXPECT_TRUE(original_path.has_thread_id());

  manager->ipc_path_info_->Clear();
  EXPECT_TRUE(manager->LoadPathName());

  const ipc::IPCPathInfo loaded_path = *(manager->ipc_path_info_);
  EXPECT_EQ(original_path.protocol_version(), loaded_path.protocol_version());
  EXPECT_EQ(original_path.product_version(), loaded_path.product_version());
  EXPECT_EQ(original_path.key(), loaded_path.key());
  EXPECT_EQ(original_path.process_id(), loaded_path.process_id());
  EXPECT_EQ(original_path.thread_id(), loaded_path.thread_id());
}
}  // namespace mozc
