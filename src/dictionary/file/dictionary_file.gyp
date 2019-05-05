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
  'targets': [
    {
      'target_name': 'codec',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'codec.cc',
      ],
      'dependencies': [
        'codec_util',
        '../../base/base.gyp:base_core',
      ],
    },
    {
      'target_name': 'codec_factory',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'codec_factory.cc',
      ],
      'dependencies': [
        'codec',
        'codec_util',
        '../../base/base.gyp:base_core',
      ],
    },
    {
      'target_name': 'codec_util',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'codec_util.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
      ],
    },
    {
      'target_name': 'dictionary_file',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'dictionary_file.cc',
      ],
      'dependencies': [
        'codec',
        '../../base/base.gyp:base_core',
      ],
    },
    {
      'target_name': 'dictionary_file_builder',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'dictionary_file_builder.cc',
      ],
      'dependencies': [
        'codec',
        '../../base/base.gyp:base_core',
      ],
    },
  ],
}
