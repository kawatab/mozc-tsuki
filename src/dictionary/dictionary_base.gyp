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

# dictionary_base.gyp defines targets for lower layers to link to the dictionary
# modules, so modules in lower layers do not depend on ones in higher layers,
# avoiding circular dependencies.
{
  'variables': {
    'relative_dir': 'dictionary',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'text_dictionary_loader',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources': [
        'dictionary_token.h',
        'text_dictionary_loader.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:multifile',
        'pos_matcher',
      ],
    },
    {
      'target_name': 'pos_util',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '../build_tools/code_generator_util.py',
        'pos_util.py',
      ],
    },
    {
      'target_name': 'gen_pos_matcher',
      'type': 'none',
      'toolsets': ['host'],
      'dependencies': [
        'pos_util',
      ],
      'actions': [
        {
          'action_name': 'gen_pos_matcher',
          'variables': {
            'pos_matcher_rule': '../data/rules/pos_matcher_rule.def',
            'pos_matcher_header': '<(gen_out_dir)/pos_matcher.h',
          },
          'inputs': [
            'gen_pos_matcher_code.py',
            '<(pos_matcher_rule)'
          ],
          'outputs': [
            '<(pos_matcher_header)',
          ],
          'action': [
            'python', 'gen_pos_matcher_code.py',
            '--pos_matcher_rule_file=<(pos_matcher_rule)',
            '--output_pos_matcher_h=<(pos_matcher_header)',
          ],
          'message': ('Generating <(pos_matcher_header)'),
        },
      ],
    },
    {
      'target_name': 'pos_matcher',
      'type': 'none',
      'toolsets': ['target', 'host'],
      'hard_dependency': 1,
      'dependencies': [
        'gen_pos_matcher#host',
      ],
      'export_dependent_settings': [
        'gen_pos_matcher#host',
      ]
    },
    {
      'target_name': 'user_pos',
      'type': 'static_library',
      'toolsets': ['target', 'host'],
      'sources' : [
        'user_pos.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'gen_pos_map',
      'type': 'none',
      'toolsets': ['host'],
      'sources': [
        '../build_tools/code_generator_util.py',
        'gen_pos_map.py',
      ],

      'actions': [
        {
          'action_name': 'gen_pos_map',
          'variables': {
            'user_pos': '../data/rules/user_pos.def',
            'third_party_pos_map': '../data/rules/third_party_pos_map.def',
            'pos_map_header': '<(gen_out_dir)/pos_map.inc',
          },
          'inputs': [
            'gen_pos_map.py',
            '<(user_pos)',
            '<(third_party_pos_map)',
          ],
          'outputs': [
            '<(pos_map_header)',
          ],
          'action': [
            'python', 'gen_pos_map.py',
            '--user_pos_file=<(user_pos)',
            '--third_party_pos_map_file=<(third_party_pos_map)',
            '--output=<(pos_map_header)',
          ],
          'message': ('Generating <(pos_map_header)'),
        },
      ],
    },
    {
      'target_name': 'suppression_dictionary',
      'type': 'static_library',
      'sources': [
        'suppression_dictionary.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
    {
      'target_name': 'user_dictionary',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/pos_map.inc',
        'user_dictionary.cc',
        'user_dictionary_importer.cc',
        'user_dictionary_session.cc',
        'user_dictionary_session_handler.cc',
        'user_dictionary_storage.cc',
        'user_dictionary_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../config/config.gyp:config_handler',
        '../protocol/protocol.gyp:config_proto',
        '../protocol/protocol.gyp:user_dictionary_storage_proto',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        'gen_pos_map#host',
        'pos_matcher',
        'suppression_dictionary',
      ],
    },
  ],
}
