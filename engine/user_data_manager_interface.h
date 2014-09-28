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

// UserDataManagerInterface is responsible for the management of the
// user data in the persistent storage, i.e. syncing, reloading, or
// clear-out.

#ifndef MOZC_ENGINE_USER_DATA_MANAGER_INTERFACE_H_
#define MOZC_ENGINE_USER_DATA_MANAGER_INTERFACE_H_

#include <string>

namespace mozc {

class UserDataManagerInterface {
 public:
  virtual ~UserDataManagerInterface() {}

  // Syncs mutable user data to local file system.
  virtual bool Sync() = 0;

  // Reloads mutable user data from local file system.
  virtual bool Reload() = 0;

  // Clears user history data.
  virtual bool ClearUserHistory() = 0;

  // Clears user prediction data.
  virtual bool ClearUserPrediction() = 0;

  // Clears unused user prediction data.
  virtual bool ClearUnusedUserPrediction() = 0;

  // Clears a specific user prediction history.
  virtual bool ClearUserPredictionEntry(
      const string &key, const string &value) = 0;

  // Waits for syncer thread to complete.
  virtual bool WaitForSyncerForTest() = 0;
};

}  // namespace mozc

#endif  // MOZC_ENGINE_USER_DATA_MANAGER_INTERFACE_H_
