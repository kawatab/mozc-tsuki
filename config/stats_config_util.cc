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

#include "config/stats_config_util.h"

#ifdef OS_WIN
#include <windows.h>
#include <Lmcons.h>
#include <shlobj.h>
#include <time.h>
#include <sddl.h>
// Workaround against KB813540
#include <atlbase_mozc.h>
#else
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef OS_MACOSX
#include <fstream>
#include <string>

#include "base/mac_util.h"
#include "base/mutex.h"
#endif

#if defined(OS_ANDROID) || defined(__native_client__)
#include "config/config_handler.h"
#include "config/config.pb.h"
#endif  // OS_ANDROID || __native_client__

#include "base/file_util.h"
#include "base/singleton.h"
#include "base/system_util.h"

namespace mozc {
namespace config {
namespace {

#ifdef GOOGLE_JAPANESE_INPUT_BUILD
#ifdef OS_WIN

const wchar_t kOmahaGUID[] = L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kOmahaUsageKey[] =
    L"Software\\Google\\Update\\ClientState\\"
    L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";
const wchar_t kOmahaUsageKeyForEveryone[] =
    L"Software\\Google\\Update\\ClientStateMedium\\"
    L"{DDCCD2A9-025E-4142-BCEB-F467B88CF830}";

const wchar_t kSendStatsName[] = L"usagestats";

class WinStatsConfigUtilImpl : public StatsConfigUtilInterface {
 public:
  WinStatsConfigUtilImpl() {
  }
  virtual ~WinStatsConfigUtilImpl() {
  }
  virtual bool IsEnabled();
  virtual bool SetEnabled(bool val);
 private:
  DISALLOW_COPY_AND_ASSIGN(WinStatsConfigUtilImpl);
};

bool WinStatsConfigUtilImpl::IsEnabled() {
#ifdef CHANNEL_DEV
  return true;
#else
  const REGSAM sam_desired = KEY_QUERY_VALUE |
      (SystemUtil::IsWindowsX64() ? KEY_WOW64_32KEY : 0);
  // Like the crash handler, check the "ClientStateMedium" key first.
  // Then we check "ClientState" key.
  {
    CRegKey key_medium;
    LONG result = key_medium.Open(HKEY_LOCAL_MACHINE, kOmahaUsageKeyForEveryone,
                                  sam_desired);
    if (result == ERROR_SUCCESS && key_medium) {
      DWORD value = 0;
      result = key_medium.QueryDWORDValue(kSendStatsName, value);
      if (result == ERROR_SUCCESS) {
        return (value != 0);
      }
    }
  }  // Close |key_medium|

  CRegKey key;
  LONG result = key.Open(HKEY_LOCAL_MACHINE, kOmahaUsageKey, sam_desired);
  if (result != ERROR_SUCCESS || !key) {
    return false;
  }
  DWORD value = 0;
  result = key.QueryDWORDValue(kSendStatsName, value);
  if (result != ERROR_SUCCESS) {
    return false;
  }
  return (value != 0);
#endif  // CHANNEL_DEV
}

bool WinStatsConfigUtilImpl::SetEnabled(bool val) {
#ifdef CHANNEL_DEV
  // On Dev channel, usage stats and crash report should be always sent.
  val = true;
  // We always returns true in DevChannel.
  const bool kReturnCodeInError = true;
#else
  const bool kReturnCodeInError = false;
#endif  // CHANNEL_DEV

  CRegKey key;
  const REGSAM sam_desired = KEY_WRITE |
      (SystemUtil::IsWindowsX64() ? KEY_WOW64_32KEY : 0);
  LONG result = key.Create(HKEY_LOCAL_MACHINE, kOmahaUsageKey,
                           REG_NONE, REG_OPTION_NON_VOLATILE,
                           sam_desired);
  if (ERROR_SUCCESS != result) {
    return kReturnCodeInError;
  }

  result = key.SetDWORDValue(kSendStatsName, val ? 1 : 0);
  if (ERROR_SUCCESS != result) {
    return kReturnCodeInError;
  }

  return true;
}

#endif  // OS_WIN

#ifdef OS_MACOSX
class MacStatsConfigUtilImpl : public StatsConfigUtilInterface {
 public:
  MacStatsConfigUtilImpl();
  virtual ~MacStatsConfigUtilImpl() {
  }
  virtual bool IsEnabled();
  virtual bool SetEnabled(bool val);

 private:
  string config_file_;
  Mutex mutex_;

  DISALLOW_COPY_AND_ASSIGN(MacStatsConfigUtilImpl);
};

MacStatsConfigUtilImpl::MacStatsConfigUtilImpl() {
  config_file_ =
      SystemUtil::GetUserProfileDirectory() + "/.usagestats.db";  // hidden file
}

bool MacStatsConfigUtilImpl::IsEnabled() {
#ifdef CHANNEL_DEV
  return true;
#else
  scoped_lock l(&mutex_);
  const bool kDefaultValue = false;

  ifstream ifs(config_file_.c_str(), ios::binary | ios::in);

  if (!ifs.is_open()) {
    return kDefaultValue;
  }

  uint32 value = kDefaultValue;
  if (!ifs.read(reinterpret_cast<char *>(&value), sizeof(value))) {
    return kDefaultValue;
  }

  // The value of usage stats is a 32-bit int and 1 (true) means
  // "sending the usage stats to Google".  When the meaning of the
  // value changed, we should fix mac/ActivatePane.m too.
  // TODO(mukai): export MacStatsConfigUtilImpl and share the code
  // among them.
  return static_cast<bool>(value);
#endif  // CHANNEL_DEV
}

bool MacStatsConfigUtilImpl::SetEnabled(bool val) {
#ifdef CHANNEL_DEV
  return true;
#else
  scoped_lock l(&mutex_);
  const uint32 value = static_cast<uint32>(val);

  if (FileUtil::FileExists(config_file_)) {
    ::chmod(config_file_.c_str(), S_IRUSR | S_IWUSR);  // read/write
  }
  ofstream ofs(config_file_.c_str(), ios::binary | ios::out | ios::trunc);
  if (!ofs) {
    return false;
  }
  ofs.write(reinterpret_cast<const char *>(&value), sizeof(value));
  if (!ofs.good()) {
    return false;
  }
  return 0 == ::chmod(config_file_.c_str(), S_IRUSR);  // read only
#endif  // CHANNEL_DEV
}
#endif  // MACOSX

#ifdef OS_ANDROID
class AndroidStatsConfigUtilImpl : public StatsConfigUtilInterface {
 public:
  AndroidStatsConfigUtilImpl() {
  }
  virtual ~AndroidStatsConfigUtilImpl() {
  }
  virtual bool IsEnabled() {
    return ConfigHandler::GetConfig().general_config().upload_usage_stats();
  }
  virtual bool SetEnabled(bool val) {
    // TODO(horo): Implement this.
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidStatsConfigUtilImpl);
};
#endif  // OS_ANDROID

#ifdef __native_client__
class NaclStatsConfigUtilImpl : public StatsConfigUtilInterface {
 public:
  NaclStatsConfigUtilImpl() {
  }
  virtual ~NaclStatsConfigUtilImpl() {
  }
  virtual bool IsEnabled() {
    return ConfigHandler::GetConfig().general_config().upload_usage_stats();
  }
  virtual bool SetEnabled(bool val) {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(NaclStatsConfigUtilImpl);
};
#endif  // __native_client__

#endif  // GOOGLE_JAPANESE_INPUT_BUILD

class NullStatsConfigUtilImpl : public StatsConfigUtilInterface {
 public:
  NullStatsConfigUtilImpl() {}
  virtual ~NullStatsConfigUtilImpl() {}
  virtual bool IsEnabled() {
    return false;
  }
  virtual bool SetEnabled(bool val) {
    return true;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(NullStatsConfigUtilImpl);
};

StatsConfigUtilInterface *g_stats_config_util_handler = NULL;

// GetStatsConfigUtil and SetHandler are not thread safe.

#if !defined(GOOGLE_JAPANESE_INPUT_BUILD)
// For non-official build, use null implementation.
typedef NullStatsConfigUtilImpl DefaultConfigUtilImpl;
#elif defined(OS_WIN)
typedef WinStatsConfigUtilImpl DefaultConfigUtilImpl;
#elif defined(OS_MACOSX)
typedef MacStatsConfigUtilImpl DefaultConfigUtilImpl;
#elif defined(OS_ANDROID)
typedef AndroidStatsConfigUtilImpl DefaultConfigUtilImpl;
#elif __native_client__
typedef NaclStatsConfigUtilImpl DefaultConfigUtilImpl;
#else
// Fall back mode.  Use null implementation.
typedef NullStatsConfigUtilImpl DefaultConfigUtilImpl;
#endif

StatsConfigUtilInterface &GetStatsConfigUtil() {
  if (g_stats_config_util_handler == NULL) {
    return *(Singleton<DefaultConfigUtilImpl>::get());
  } else {
    return *g_stats_config_util_handler;
  }
}
}  // namespace

void StatsConfigUtil::SetHandler(StatsConfigUtilInterface *handler) {
  g_stats_config_util_handler = handler;
}

bool StatsConfigUtil::IsEnabled() {
  return GetStatsConfigUtil().IsEnabled();
}

bool StatsConfigUtil::SetEnabled(bool val) {
  return GetStatsConfigUtil().SetEnabled(val);
}

}  // namespace config
}  // namespace mozc
