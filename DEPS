# Copyright 2010-2014, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

vars = {
  "breakpad_revision": "1353",
  "gtest_revision": "692",
  "gmock_revision": "485",
  "gyp_revision": "1957",
  "jsoncpp_revision": "3a0c4fcc82d25d189b8107e07462effbab9f8e1b",
  "protobuf_revision": "488",
  "wtl_revision": "587",
  "zinnia_revision": "16",
  "jsr305_version": "2.0.2",
  "zlib_revision": "198222",
  "japanese_usage_dictionary_revision": "10",
}

deps = {
  "src/third_party/jsoncpp":
    "https://github.com/open-source-parsers/jsoncpp.git@" +
    Var("jsoncpp_revision"),
  "src/third_party/gmock":
    "http://googlemock.googlecode.com/svn/trunk@" + Var("gmock_revision"),
  "src/third_party/gtest":
    "http://googletest.googlecode.com/svn/trunk@" + Var("gtest_revision"),
  "src/third_party/gyp":
    "http://gyp.googlecode.com/svn/trunk@" + Var("gyp_revision"),
  "src/third_party/protobuf":
    "http://protobuf.googlecode.com/svn/trunk@" + Var("protobuf_revision"),
  "src/third_party/japanese_usage_dictionary":
    "http://japanese-usage-dictionary.googlecode.com/svn/trunk@" +
    Var("japanese_usage_dictionary_revision"),
}

deps_os = {
  "win": {
    "src/third_party/breakpad":
      "http://google-breakpad.googlecode.com/svn/trunk@" +
      Var("breakpad_revision"),
    "src/third_party/wtl/files/include":
      "http://svn.code.sf.net/p/wtl/code/trunk/wtl/Include@" +
      Var("wtl_revision"),
    "src/third_party/zinnia/v0_04":
      "http://svn.code.sf.net/p/zinnia/code@" +
      Var("zinnia_revision"),
  },
  "mac": {
    "src/third_party/zinnia/v0_04":
      "http://svn.code.sf.net/p/zinnia/code@" +
       Var("zinnia_revision"),
  },
  "unix": {
    "src/third_party/findbug":
      # We need only a jar file so avoiding to sync entire tree.
      # Findbug project keeps release-jar files in their repository.
      File("http://findbugs.googlecode.com/"
           + "svn/repos/release-repository/com/google/code/findbugs/jsr305/"
           + Var("jsr305_version") + "/jsr305-" + Var("jsr305_version") + ".jar"),
    "src/third_party/zlib/v1_2_8":
      "https://src.chromium.org/chrome/trunk/src/third_party/zlib@" +
      Var("zlib_revision"),
  },
}
