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
      'target_name': 'system_dictionary_codec',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'codec.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
      ],
    },
    {
      'target_name': 'key_expansion_table',
      'type': 'none',
      'toolsets': ['target', 'host'],
      'sources': [
        'key_expansion_table.h',
      ],
    },
    {
      'target_name': 'system_dictionary',
      'type': 'static_library',
      'sources': [
        'system_dictionary.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
        '../../request/request.gyp:conversion_request',
        '../../storage/louds/louds.gyp:bit_vector_based_array',
        '../../storage/louds/louds.gyp:louds_trie',
        '../dictionary_base.gyp:text_dictionary_loader',
        '../file/dictionary_file.gyp:codec_factory',
        '../file/dictionary_file.gyp:dictionary_file',
        'key_expansion_table',
        'system_dictionary_codec',
      ],
    },
    {
      'target_name': 'value_dictionary',
      'type': 'static_library',
      'sources': [
        'value_dictionary.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
        '../../storage/louds/louds.gyp:louds_trie',
        '../dictionary_base.gyp:pos_matcher',
        '../file/dictionary_file.gyp:codec_factory',
        'system_dictionary_codec',
      ],
    },
    {
      'target_name': 'system_dictionary_builder',
      'type': 'static_library',
      'toolsets': ['target', 'host'],  # "target" is needed for test.
      'sources': [
        'system_dictionary_builder.cc',
      ],
      'dependencies': [
        '../../base/base.gyp:base_core',
        '../../storage/louds/louds.gyp:bit_vector_based_array_builder',
        '../../storage/louds/louds.gyp:louds_trie_builder',
        '../dictionary_base.gyp:pos_matcher',
        '../dictionary_base.gyp:text_dictionary_loader',
        '../file/dictionary_file.gyp:codec',
        '../file/dictionary_file.gyp:codec_factory',
        'system_dictionary_codec',
      ],
    },
  ],
}
