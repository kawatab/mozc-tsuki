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
    'relative_dir': 'data_manager/oss',
    'relative_mozc_dir': '',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'gen_out_mozc_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_mozc_dir)',
  },
  'targets': [
    {
      'target_name': 'oss_data_manager_test',
      'type': 'executable',
      'sources': [
        'oss_data_manager_test.cc',
      ],
      'dependencies': [
        '../../testing/testing.gyp:gtest_main',
        '../data_manager_test.gyp:data_manager_test_base',
        'oss_data_manager.gyp:oss_data_manager',
        'oss_data_manager.gyp:gen_oss_segmenter_inl_header#host',
        'install_oss_data_manager_test_data',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'install_oss_data_manager_test_data',
      'type': 'none',
      'variables': {
        'test_data': [
          '<(gen_out_dir)/connection_single_column.txt',
          '../../<(test_data_subdir)/dictionary00.txt',
          '../../<(test_data_subdir)/dictionary01.txt',
          '../../<(test_data_subdir)/dictionary02.txt',
          '../../<(test_data_subdir)/dictionary03.txt',
          '../../<(test_data_subdir)/dictionary04.txt',
          '../../<(test_data_subdir)/dictionary05.txt',
          '../../<(test_data_subdir)/dictionary06.txt',
          '../../<(test_data_subdir)/dictionary07.txt',
          '../../<(test_data_subdir)/dictionary08.txt',
          '../../<(test_data_subdir)/dictionary09.txt',
          '../../<(test_data_subdir)/suggestion_filter.txt'
        ],
        'test_data_subdir': 'data/dictionary_oss',
      },
      'includes': ['../../gyp/install_testdata.gypi'],
    },
  ],
}
