# Copyright 2010-2018, Google Inc.
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

{
  'variables': {
    # Top directory of third party libraries.
    'third_party_dir': '<(DEPTH)/third_party',

    # Top directory of additional third party libraries.
    'ext_third_party_dir%': '<(abs_depth)/third_party',

    # TODO(komatsu): This can be replaced with 'android_ndk_dir'.
    'mozc_build_tools_dir': '<(abs_depth)/<(build_short_base)/mozc_build_tools',

    'proto_out_dir': '<(SHARED_INTERMEDIATE_DIR)/proto_out',

    # server_dir represents the directory where mozc_server is
    # installed. This option is only for Linux.
    'server_dir%': '/usr/lib/mozc',

    # Represents the directory where the source code of protobuf is
    # extracted. This value is ignored when 'use_libprotobuf' is 1.
    'protobuf_root': '<(third_party_dir)/protobuf',

    'mozc_data_dir': '<(SHARED_INTERMEDIATE_DIR)/',

    # Ninja requires <(abs_depth) instead of <(DEPTH).
    'mac_breakpad_dir': '<(PRODUCT_DIR)/Breakpad',
    # This points to the same dir with mac_breakpad_dir, but this should use
    # '${BUILT_PRODUCTS_DIR}' instead of '<(PRODUCT_DIR)'.
    # See post_build_mac.gypi
    'mac_breakpad_tools_dir': '${BUILT_PRODUCTS_DIR}/Breakpad',
    'mac_breakpad_framework': '<(mac_breakpad_dir)/Breakpad.framework',

    'conditions': [
      ['target_platform=="Windows"', {
        'wtl_dir': '<(ext_third_party_dir)/wtl',
      }, 'target_platform=="NaCl"', {
        'pnacl_bin_dir%': '<(nacl_sdk_root)/toolchain/linux_pnacl/bin',
      }, 'target_platform=="Android"', {
        'ndk_bin_dir%':
        '<(mozc_build_tools_dir)/ndk-standalone-toolchain/<(android_arch)/bin',
      }],

      # zinnia_model_file.
      ['branding=="GoogleJapaneseInput"', {
        # Test:
        #     this file is copied to a directory for testing.
        # Win / Mac:
        #     this file content is copied the release packages and
        #     other file path is used internally.
        # Linux:
        #     this file path is ignored, and used a preinstalled data.
        'zinnia_model_file%':
        '<(third_party_dir)/zinnia/tomoe/handwriting-light-ja.model',
      }, {
        # Test:
        #     this file is copied to a directory for testing.
        # Win / Mac / Linux:
        #     this file path is directory used by binaries without copying.
        'zinnia_model_file%':
        '/usr/share/tegaki/models/zinnia/handwriting-ja.model',
      }],
    ],
  },
}
