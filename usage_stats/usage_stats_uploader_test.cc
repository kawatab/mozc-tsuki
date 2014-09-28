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

#include "usage_stats/usage_stats_uploader.h"

#ifdef OS_ANDROID
#include <jni.h>
#endif  // OS_ANDROID

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/port.h"
#include "base/singleton.h"
#include "base/system_util.h"
#include "base/util.h"
#include "base/version.h"
#include "base/win_util.h"
#include "config/config.pb.h"
#include "config/config_handler.h"
#include "net/http_client.h"
#include "storage/registry.h"
#include "storage/storage_interface.h"
#include "testing/base/public/gunit.h"
#include "usage_stats/usage_stats.h"
#include "usage_stats/usage_stats.pb.h"
#include "usage_stats/usage_stats_testing_util.h"

#ifdef OS_ANDROID
#include "base/android_util.h"
#include "base/android_jni_mock.h"
#include "base/android_jni_proxy.h"
#endif  // OS_ANDROID

DECLARE_string(test_tmpdir);

namespace mozc {
namespace usage_stats {
namespace {

class TestableUsageStatsUploader : public UsageStatsUploader {
 public:
  // Change access rights.
  using UsageStatsUploader::LoadStats;
  using UsageStatsUploader::GetClientId;
};

class TestHTTPClient : public HTTPClientInterface {
 public:
  bool Get(const string &url, const HTTPClient::Option &option,
           string *output) const { return true; }
  bool Head(const string &url, const HTTPClient::Option &option,
            string *output) const { return true; }
  bool Post(const string &url, const string &data,
            const HTTPClient::Option &option, string *output) const {
    LOG(INFO) << "url: " << url;
    LOG(INFO) << "data: " << data;
    if (result_.expected_url != url) {
      LOG(INFO) << "expected_url: " << result_.expected_url;
      return false;
    }

    vector<string> data_set;
    Util::SplitStringUsing(data, "&", &data_set);
    for (size_t i = 0; i < expected_data_.size(); ++i) {
      vector<string>::const_iterator itr =
          find(data_set.begin(), data_set.end(), expected_data_[i]);
      const bool found = (itr != data_set.end());
      // we can't compile EXPECT_NE(itr, data_set.end()), so we use EXPECT_TRUE
      EXPECT_TRUE(found) << expected_data_[i];
    }

    *output = result_.expected_result;
    return true;
  }

  struct Result {
    string expected_url;
    string expected_result;
  };

  void set_result(const Result &result) {
    result_ = result;
  }

  // TODO(toshiyuki): integrate with struct Result
  void AddExpectedData(const string &data) {
    expected_data_.push_back(data);
  }

 private:
  // usage stats key and value parameter
  // format is "<key>:<type>=<value>"
  vector<string> expected_data_;
  Result result_;
};

const uint32 kOneDaySec = 24 * 60 * 60;  // 24 hours
const uint32 kHalfDaySec = 12 * 60 * 60;  // 12 hours
const char kBaseUrl[] =
#ifdef __native_client__
    "https://clients4.google.com/tbproxy/usagestats";
#else  // __native_client__
    "http://clients4.google.com/tbproxy/usagestats";
#endif  // __native_client__
const char kTestClientId[] = "TestClientId";
const char kCountStatsKey[] = "Commit";
const uint32 kCountStatsDefaultValue = 100;
const char kIntegerStatsKey[] = "UserRegisteredWord";
const int kIntegerStatsDefaultValue = 2;

void SetUpMetaDataWithMozcVersion(uint32 last_upload_time,
                                  const string &mozc_version) {
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.last_upload",
                                        last_upload_time));
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.mozc_version",
                                        mozc_version));
}

void SetUpMetaData(uint32 last_upload_time) {
  SetUpMetaDataWithMozcVersion(last_upload_time, Version::GetMozcVersion());
}

class TestClientId : public ClientIdInterface {
 public:
  TestClientId() {}
  virtual ~TestClientId() {}
  void GetClientId(string *output) {
    *output = kTestClientId;
  }
};

class UsageStatsUploaderTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);

    TestableUsageStatsUploader::SetClientIdHandler(&client_id_);
    HTTPClient::SetHTTPClientHandler(&client_);
    EXPECT_TRUE(storage::Registry::Clear());

    mozc::config::Config config;
    mozc::config::ConfigHandler::GetDefaultConfig(&config);
    mozc::config::ConfigHandler::SetConfig(config);

    // save test stats
    UsageStats::IncrementCountBy(kCountStatsKey, kCountStatsDefaultValue);
    EXPECT_COUNT_STATS(kCountStatsKey, kCountStatsDefaultValue);
    UsageStats::SetInteger(kIntegerStatsKey, kIntegerStatsDefaultValue);
    EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  }

  virtual void TearDown() {
    mozc::config::Config config;
    mozc::config::ConfigHandler::GetDefaultConfig(&config);
    mozc::config::ConfigHandler::SetConfig(config);

    TestableUsageStatsUploader::SetClientIdHandler(NULL);
    HTTPClient::SetHTTPClientHandler(NULL);
    EXPECT_TRUE(storage::Registry::Clear());
  }

  void SetValidResult() {
    vector<pair<string, string> > params;
    params.push_back(make_pair("sourceid", "ime"));
    params.push_back(make_pair("hl", "ja"));
    params.push_back(make_pair("v", Version::GetMozcVersion()));
    params.push_back(make_pair("client_id", kTestClientId));
    params.push_back(make_pair("os_ver", SystemUtil::GetOSVersionString()));
#ifdef OS_ANDROID
    params.push_back(
        make_pair("model",
                  AndroidUtil::GetSystemProperty(
                      AndroidUtil::kSystemPropertyModel, "Unknown")));
#endif  // OS_ANDROID

    string url = string(kBaseUrl) + "?";
    Util::AppendCGIParams(params, &url);
    TestHTTPClient::Result result;
    result.expected_url = url;
    client_.set_result(result);
  }

  TestHTTPClient client_;
  TestClientId client_id_;
  scoped_usage_stats_enabler usage_stats_enabler_;
};

TEST_F(UsageStatsUploaderTest, SendTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kOneDaySec;
  SetUpMetaData(last_upload_sec);
  SetValidResult();

  EXPECT_TRUE(TestableUsageStatsUploader::Send(NULL));

  // COUNT stats are cleared
  EXPECT_STATS_NOT_EXIST(kCountStatsKey);
  // INTEGER stats are not cleared
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  uint32 recorded_sec;
  string recorded_version;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.mozc_version",
                                        &recorded_version));
  EXPECT_LE(current_sec, recorded_sec);
  EXPECT_EQ(Version::GetMozcVersion(), recorded_version);
}

TEST_F(UsageStatsUploaderTest, FirstTimeSendTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  // Don't call SetUpMetaData()..
  SetValidResult();

  uint32 recorded_sec;
  string recorded_version;
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.last_upload",
                                         &recorded_sec));
  EXPECT_FALSE(storage::Registry::Lookup("usage_stats.mozc_version",
                                         &recorded_version));

  EXPECT_TRUE(TestableUsageStatsUploader::Send(NULL));

  EXPECT_STATS_NOT_EXIST(kCountStatsKey);
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.mozc_version",
                                        &recorded_version));
  EXPECT_LE(current_sec, recorded_sec);
  EXPECT_EQ(Version::GetMozcVersion(), recorded_version);
}

TEST_F(UsageStatsUploaderTest, SendFailTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kHalfDaySec;
  SetUpMetaData(last_upload_sec);
  SetValidResult();

  EXPECT_FALSE(TestableUsageStatsUploader::Send(NULL));

  EXPECT_COUNT_STATS(kCountStatsKey, kCountStatsDefaultValue);
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_EQ(last_upload_sec, recorded_sec);
}

TEST_F(UsageStatsUploaderTest, InvalidLastUploadTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  // future time
  // for example: time zone has changed
  const uint32 invalid_sec = current_sec + kHalfDaySec;
  SetUpMetaData(invalid_sec);
  SetValidResult();

  EXPECT_TRUE(TestableUsageStatsUploader::Send(NULL));

  EXPECT_STATS_NOT_EXIST(kCountStatsKey);
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  // Save new last_upload_time
  EXPECT_LE(current_sec, recorded_sec);
}

TEST_F(UsageStatsUploaderTest, MozcVersionMismatchTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kOneDaySec;
  SetUpMetaDataWithMozcVersion(last_upload_sec, "invalid_mozc_version");
  SetValidResult();

  EXPECT_TRUE(TestableUsageStatsUploader::Send(NULL));

  EXPECT_STATS_NOT_EXIST(kCountStatsKey);
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  uint32 recorded_sec;
  string recorded_version;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.mozc_version",
                                        &recorded_version));
  // Save new last_upload_time and recorded_version.
  EXPECT_LE(current_sec, recorded_sec);
  EXPECT_EQ(Version::GetMozcVersion(), recorded_version);
}

class TestStorage: public storage::StorageInterface {
 public:
  bool Open(const string &filename) { return true; }
  bool Sync() { return true; }
  bool Lookup(const string &key, string *value) const { return false; }
  // return false
  bool Insert(const string &key, const string &value) { return false; }
  bool Erase(const string &key) { return true; }
  bool Clear() { return true; }
  size_t Size() const { return 0; }
  TestStorage() {}
  virtual ~TestStorage() {}
};

TEST_F(UsageStatsUploaderTest, SaveMetadataFailTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kOneDaySec;
  const string current_version = Version::GetMozcVersion();
  SetUpMetaData(last_upload_sec);
  SetValidResult();

  // set the TestStorage as a storage handler.
  // writing to the registry will be failed.
  storage::Registry::SetStorage(Singleton<TestStorage>::get());
  // confirm that we can not insert.
  EXPECT_FALSE(storage::Registry::Insert("usage_stats.last_upload",
                                         last_upload_sec));
  EXPECT_FALSE(storage::Registry::Insert("usage_stats.mozc_version",
                                         current_version));

  EXPECT_FALSE(TestableUsageStatsUploader::Send(NULL));
  // restore
  storage::Registry::SetStorage(NULL);

  // stats data are kept
  EXPECT_COUNT_STATS(kCountStatsKey, kCountStatsDefaultValue);
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  uint32 recorded_sec;
  string recorded_version;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.mozc_version",
                                        &recorded_version));
  EXPECT_EQ(last_upload_sec, recorded_sec);
  EXPECT_EQ(current_version, recorded_version);
}

TEST_F(UsageStatsUploaderTest, UploadFailTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kOneDaySec;
  SetUpMetaData(last_upload_sec);
  SetValidResult();

  TestHTTPClient::Result result;
  // set dummy result url so that upload will be failed.
  result.expected_url = "fail_url";
  client_.set_result(result);

  EXPECT_FALSE(TestableUsageStatsUploader::Send(NULL));

  // stats data are not cleared
  EXPECT_COUNT_STATS(kCountStatsKey, kCountStatsDefaultValue);
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  // "UsageStatsUploadFailed" is incremented
  EXPECT_COUNT_STATS("UsageStatsUploadFailed", 1);
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  // last_upload is not updated
  EXPECT_EQ(last_upload_sec, recorded_sec);
}

TEST_F(UsageStatsUploaderTest, UploadRetryTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kOneDaySec;
  SetUpMetaData(last_upload_sec);
  SetValidResult();

  TestHTTPClient::Result result;
  // set dummy result url so that upload will be failed.
  result.expected_url = "fail_url";
  client_.set_result(result);

  EXPECT_FALSE(TestableUsageStatsUploader::Send(NULL));

  // stats data are not cleared
  EXPECT_COUNT_STATS(kCountStatsKey, kCountStatsDefaultValue);
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  uint32 recorded_sec;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  // last_upload is not updated
  EXPECT_EQ(last_upload_sec, recorded_sec);

  // retry
  SetValidResult();
  // We can send stats if network is available.
  EXPECT_TRUE(TestableUsageStatsUploader::Send(NULL));

  // Stats are cleared
  EXPECT_STATS_NOT_EXIST(kCountStatsKey);
  // However, INTEGER stats are not cleared
  EXPECT_INTEGER_STATS(kIntegerStatsKey, kIntegerStatsDefaultValue);
  // last upload is updated
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.last_upload",
                                        &recorded_sec));
  EXPECT_LE(last_upload_sec, recorded_sec);
}

TEST_F(UsageStatsUploaderTest, UploadDataTest) {
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kOneDaySec;
  SetUpMetaData(last_upload_sec);
  SetValidResult();

#ifdef OS_WIN
  const string win64 = (string("WindowsX64:b=")
                        + (SystemUtil::IsWindowsX64()? "t" : "f"));
  client_.AddExpectedData(win64);
  int major, minor, build, revision;
  const wchar_t kDllName[] = L"msctf.dll";
  wstring path = SystemUtil::GetSystemDir();
  path += L"\\";
  path += kDllName;
  if (SystemUtil::GetFileVersion(path, &major, &minor, &build, &revision)) {
    client_.AddExpectedData(Util::StringPrintf("MsctfVerMajor:i=%d", major));
    client_.AddExpectedData(Util::StringPrintf("MsctfVerMinor:i=%d", minor));
    client_.AddExpectedData(Util::StringPrintf("MsctfVerBuild:i=%d", build));
    client_.AddExpectedData(Util::StringPrintf("MsctfVerRevision:i=%d",
                                               revision));
  } else {
    LOG(ERROR) << "get file version for msctf.dll failed";
  }
  client_.AddExpectedData(
      string("CuasEnabled:b=") + (WinUtil::IsCuasEnabled() ? "t" : "f"));
#endif
  client_.AddExpectedData(Util::StringPrintf("%s:c=%u", kCountStatsKey,
                                             kCountStatsDefaultValue));
  client_.AddExpectedData(Util::StringPrintf("%s:i=%d", kIntegerStatsKey,
                                             kIntegerStatsDefaultValue));
  client_.AddExpectedData("Daily");

  EXPECT_TRUE(TestableUsageStatsUploader::Send(NULL));
}

void SetDoubleValueStats(
    uint32 num, double total, double square_total,
    usage_stats::Stats::DoubleValueStats *double_stats) {
  double_stats->set_num(num);
  double_stats->set_total(total);
  double_stats->set_square_total(square_total);
}

void SetEventStats(
    uint32 source_id,
    uint32 sx_num, double sx_total, double sx_square_total,
    uint32 sy_num, double sy_total, double sy_square_total,
    uint32 dx_num, double dx_total, double dx_square_total,
    uint32 dy_num, double dy_total, double dy_square_total,
    uint32 tl_num, double tl_total, double tl_square_total,
    usage_stats::Stats::TouchEventStats *event_stats) {
  event_stats->set_source_id(source_id);
  SetDoubleValueStats(sx_num, sx_total, sx_square_total,
                      event_stats->mutable_start_x_stats());
  SetDoubleValueStats(sy_num, sy_total, sy_square_total,
                      event_stats->mutable_start_y_stats());
  SetDoubleValueStats(dx_num, dx_total, dx_square_total,
                      event_stats->mutable_direction_x_stats());
  SetDoubleValueStats(dy_num, dy_total, dy_square_total,
                      event_stats->mutable_direction_y_stats());
  SetDoubleValueStats(tl_num, tl_total, tl_square_total,
                      event_stats->mutable_time_length_stats());
}

TEST_F(UsageStatsUploaderTest, UploadTouchEventStats) {
  // save last_upload
  const uint32 current_sec = static_cast<uint32>(Util::GetTime());
  const uint32 last_upload_sec = current_sec - kOneDaySec;
  SetUpMetaData(last_upload_sec);
  SetValidResult();

  EXPECT_STATS_NOT_EXIST("VirtualKeyboardStats");
  EXPECT_STATS_NOT_EXIST("VirtualKeyboardMissStats");
  map<string, map<uint32, Stats::TouchEventStats> > touch_stats;
  map<string, map<uint32, Stats::TouchEventStats> > miss_touch_stats;

  Stats::TouchEventStats &event_stats1 = touch_stats["KEYBOARD_01"][10];
  SetEventStats(10, 2, 3, 8, 2, 4, 10, 2, 5, 16, 2, 2, 2,
                2, 3, 9, &event_stats1);

  Stats::TouchEventStats &event_stats2 = touch_stats["KEYBOARD_02"][20];
  SetEventStats(20, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113,
                114, 115, 116, &event_stats2);

  Stats::TouchEventStats &event_stats3 = touch_stats["KEYBOARD_01"][20];
  SetEventStats(20, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213,
                214, 215, 216, &event_stats3);

  Stats::TouchEventStats &event_stats4 = miss_touch_stats["KEYBOARD_01"][20];
  SetEventStats(20, 302, 303, 304, 305, 306, 307, 308, 309, 310, 311, 312, 313,
                314, 315, 316, &event_stats4);

  Stats::TouchEventStats &event_stats5 = miss_touch_stats["KEYBOARD_01"][30];
  SetEventStats(30, 404, 406, 408, 410, 412, 414, 416, 418, 420, 422, 424, 426,
                428, 430, 432, &event_stats5);

  UsageStats::StoreTouchEventStats("VirtualKeyboardStats", touch_stats);
  UsageStats::StoreTouchEventStats("VirtualKeyboardMissStats",
                                   miss_touch_stats);

  Stats stats;
  EXPECT_TRUE(UsageStats::GetVirtualKeyboardForTest("VirtualKeyboardStats",
                                                    &stats));
  EXPECT_EQ(2, stats.virtual_keyboard_stats_size());
  EXPECT_EQ("KEYBOARD_01",
            stats.virtual_keyboard_stats(0).keyboard_name());
  EXPECT_EQ("KEYBOARD_02",
            stats.virtual_keyboard_stats(1).keyboard_name());
  EXPECT_EQ(2, stats.virtual_keyboard_stats(0).touch_event_stats_size());
  EXPECT_EQ(1, stats.virtual_keyboard_stats(1).touch_event_stats_size());

  EXPECT_EQ(event_stats1.DebugString(),
            stats.virtual_keyboard_stats(0).touch_event_stats(0).DebugString());
  EXPECT_EQ(event_stats3.DebugString(),
            stats.virtual_keyboard_stats(0).touch_event_stats(1).DebugString());
  EXPECT_EQ(event_stats2.DebugString(),
            stats.virtual_keyboard_stats(1).touch_event_stats(0).DebugString());

  EXPECT_TRUE(UsageStats::GetVirtualKeyboardForTest("VirtualKeyboardMissStats",
                                                    &stats));
  EXPECT_EQ(1, stats.virtual_keyboard_stats_size());
  EXPECT_EQ("KEYBOARD_01",
            stats.virtual_keyboard_stats(0).keyboard_name());
  EXPECT_EQ(2, stats.virtual_keyboard_stats(0).touch_event_stats_size());
  EXPECT_EQ(event_stats4.DebugString(),
            stats.virtual_keyboard_stats(0).touch_event_stats(0).DebugString());
  EXPECT_EQ(event_stats5.DebugString(),
            stats.virtual_keyboard_stats(0).touch_event_stats(1).DebugString());

  client_.AddExpectedData(string("vks%5Fname%5FKEYBOARD%5F01:i=0"));
  client_.AddExpectedData(string("vks%5Fname%5FKEYBOARD%5F02:i=1"));
  client_.AddExpectedData(string("vkms%5Fname%5FKEYBOARD%5F01:i=0"));

  // Average = total / num
  // Variance = square_total / num - (total / num) ^ 2
  // Because the current log analysis system can only deal with int values,
  // we multiply these values by a scale factor and send them to server.
  //   sxa, sya, dxa, dya : scale = 10000000
  //   sxv, syv, dxv, dyv : scale = 10000000
  //   tla, tlv : scale = 10000000

  // (3 / 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fsxa:i=15000000"));
  // (8 / 2 - (3 / 2) ^ 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fsxv:i=17500000"));
  // (4 / 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fsya:i=20000000"));
  // (10 / 2 - (4 / 2) ^ 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fsyv:i=10000000"));
  // (5 / 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fdxa:i=25000000"));
  // (16 / 2 - (5 / 2) ^ 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fdxv:i=17500000"));
  // (2 / 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fdya:i=10000000"));
  // (2 / 2 - (2 / 2) ^ 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Fdyv:i=0"));
  // (3 / 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Ftla:i=15000000"));
  // (9 / 2 - (3 / 2) ^ 2) * 10000000
  client_.AddExpectedData(string("vks%5F0%5F10%5Ftlv:i=22500000"));
  EXPECT_TRUE(TestableUsageStatsUploader::Send(NULL));
}

TEST(ClientIdTest, CreateClientIdTest) {
  // test default client id handler here
  TestableUsageStatsUploader::SetClientIdHandler(NULL);
  SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id1;
  TestableUsageStatsUploader::GetClientId(&client_id1);
  string client_id_in_storage1;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.client_id",
                                        &client_id_in_storage1));
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id2;
  TestableUsageStatsUploader::GetClientId(&client_id2);
  string client_id_in_storage2;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.client_id",
                                        &client_id_in_storage2));

  EXPECT_NE(client_id1, client_id2);
  EXPECT_NE(client_id_in_storage1, client_id_in_storage2);
}

TEST(ClientIdTest, GetClientIdTest) {
  // test default client id handler here.
  TestableUsageStatsUploader::SetClientIdHandler(NULL);
  SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id1;
  TestableUsageStatsUploader::GetClientId(&client_id1);
  string client_id2;
  TestableUsageStatsUploader::GetClientId(&client_id2);
  // we can get same client id.
  EXPECT_EQ(client_id1, client_id2);

  string client_id_in_storage;
  EXPECT_TRUE(storage::Registry::Lookup("usage_stats.client_id",
                                        &client_id_in_storage));
  // encrypted value is in storage
  EXPECT_NE(client_id1, client_id_in_storage);
}

TEST(ClientIdTest, GetClientIdFailTest) {
  // test default client id handler here.
  TestableUsageStatsUploader::SetClientIdHandler(NULL);
  SystemUtil::SetUserProfileDirectory(FLAGS_test_tmpdir);
  EXPECT_TRUE(storage::Registry::Clear());
  string client_id1;
  TestableUsageStatsUploader::GetClientId(&client_id1);
  // insert invalid data
  EXPECT_TRUE(storage::Registry::Insert("usage_stats.client_id",
                                        "invalid_data"));

  string client_id2;
  // decript should be failed
  TestableUsageStatsUploader::GetClientId(&client_id2);
  // new id should be generated
  EXPECT_NE(client_id1, client_id2);
}

}  // namespace
}  // namespace usage_stats
}  // namespace mozc
