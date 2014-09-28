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
  'variables': {
    'relative_dir': 'client',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'client_quality_test_main',
      'type': 'executable',
      'sources': [
        'client_quality_test_main.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../session/session_base.gyp:session_protocol',
        'client.gyp:client',
      ],
      'actions': [
        {
          'action_name': 'gen_client_quality_test_data',
          'variables' : {
            'input_files': [
            ],
          },
          'inputs': [
            '<@(input_files)',
          ],
          'outputs': [
            '<(gen_out_dir)/client_quality_test_data.h',
          ],
          'action': [
            'python', '../build_tools/redirect.py',
            '<(gen_out_dir)/client_quality_test_data.h',
            'gen_client_quality_test_data.py',
            '<@(input_files)',
          ],
          'message': 'Generating <(gen_out_dir)/client_quality_test_data.h',
        },
      ],
    },
    {
      'target_name': 'client_test',
      'type': 'executable',
      'sources': [
        'client_test.cc',
      ],
      'dependencies': [
        'client.gyp:client',
        '../testing/testing.gyp:gtest_main',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'client_all_test',
      'type': 'none',
      'dependencies': [
        'client_test',
      ],
      'conditions': [
        ['target_platform=="Android"', {
          'dependencies!': [
            'client_test',
          ],
        }],
      ],
    },
  ],
}
