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

{
  'type': 'executable',
  'sources': [
    'mozc_broker_main.cc',
  ],
  'dependencies': [
    '../../base/base.gyp:base',
  ],
  'conditions': [
    ['OS=="win"', {
      'sources': [
        '<(gen_out_dir)/mozc_broker_autogen.rc',
        'ime_switcher.cc',
        'prelauncher.cc',
        'register_ime.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:crash_report_handler',
        '../../client/client.gyp:client',
        '../../config/config.gyp:config_protocol',
        '../../config/config.gyp:stats_config_util',
        '../../ipc/ipc.gyp:ipc',
        '../../renderer/renderer.gyp:renderer_client',
        '../../session/session_base.gyp:session_protocol',
        '../base/win32_base.gyp:ime_base',
        '../base/win32_base.gyp:win32_file_verifier',
        'gen_mozc_broker_resource_header',
      ],
      'msvs_settings': {
        'VCManifestTool': {
          'AdditionalManifestFiles': 'mozc_broker.exe.manifest',
          'EmbedManifest': 'true',
        },
      },
    }],
  ],
}
