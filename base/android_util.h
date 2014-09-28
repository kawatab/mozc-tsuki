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

#ifndef MOZC_BASE_ANDROID_UTIL_H_
#define MOZC_BASE_ANDROID_UTIL_H_

#include <jni.h>
#include <map>
#include <set>
#include <string>

#include "base/port.h"

// for FRIEND_TEST()
#include "testing/base/public/gunit_prod.h"

namespace mozc {

class AndroidUtil {
 public:
  // Frequently used property names.
  static const char kSystemPropertyOsVersion[];
  static const char kSystemPropertyModel[];
  static const char kSystemPropertySdkVersion[];

  // Reads system property from file system.
  // Note that dynamic properties (e.g. ro.build.date)
  // cannot be obtained.
  // If no property is found or something goes wrong,
  // returns |default_value|.
  // Typical usage :
  //   EXPECT_EQ(
  //     "Nexus One",
  //     AndroidUtil::GetSystemProperty(kAndroidSystemPropertyModel));
  // Note : Using ::popen("getprop [property name]", "r") is better solution
  // but currently popen seems to be unstable.
  static string GetSystemProperty(const string &key,
                                  const string &default_value);

  // Gets JNIEnv* from JavaVM*.
  static JNIEnv *GetEnv(JavaVM *vm);
 private:
  FRIEND_TEST(AndroidUtilTest, ParseLine_valid);
  FRIEND_TEST(AndroidUtilTest, ParseLine_invalid);

  // Gets the property's value, which is read from file system.
  // If successfully done, the result will be stored in |output| and
  // returns true.
  // If something goes wrong (e.g. file system error, non-existent property
  // name), returns false. |output| is undefined.
  static bool GetPropertyFromFile(const string &key, string *output);

  // Parses the line.
  // If the returned value is ture, the property's key and value string are
  // returned as |lhs| and |rhs| respectively.
  // If false, |line| is malformed.
  // In this case |lhs| and |rhs| are not modified.
  static bool ParseLine(const string &line, string *lhs, string *rhs);

  static map<string, string> property_cache;
  static set<string> undefined_keys;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AndroidUtil);
};

}  // namespace mozc
#endif  // MOZC_BASE_ANDROID_UTIL_H
