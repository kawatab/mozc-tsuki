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
    'relative_dir': 'win32/ime',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
    'conditions': [
      ['branding=="GoogleJapaneseInput"', {
        'imefile_product_name_win': 'GIMEJa',
      }, {  # else
        'imefile_product_name_win': 'mozc_ja',
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'ime_core_test',
      'type': 'executable',
      'sources': [
        # empty
      ],
      'dependencies': [
        '../../base/base.gyp:base',
        '../../testing/testing.gyp:gtest_main',
      ],
      'variables': {
        'test_size': 'small',
      },
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'ime_candidate_info_test.cc',
            'ime_composition_string_test.cc',
            'ime_core_test.cc',
            'ime_input_context_test.cc',
            'ime_ui_visibility_tracker_test.cc',
          ],
          'dependencies': [
            'ime_core',
          ],
        }],
      ],
    },
    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'ime_all_test',
      'type': 'none',
      'dependencies': [
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'ime_core_test',
          ],
        }],
      ],
    },
  ],
  'conditions': [
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'gen_mozc_ime_resource_header',
          'variables': {
            'gen_resource_proj_name': 'mozc_ime',
            'gen_main_resource_path': 'win32/ime/GoogleIMEJa.rc',
            'gen_output_resource_path':
                '<(gen_out_dir)/GoogleIMEJa_autogen.rc',
          },
          'includes': [
            '../../gyp/gen_win32_resource_header.gypi',
          ],
        },
        {
          'target_name': 'ime_core',
          'type': 'static_library',
          'sources': [
            'ime_candidate_info.cc',
            'ime_composition_string.cc',
            'ime_core.cc',
            'ime_input_context.cc',
            'ime_message_queue.cc',
            'ime_private_context.cc',
            'ime_types.cc',
            'ime_ui_context.cc',
            'ime_ui_visibility_tracker.cc',
            'ime_ui_window.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../client/client.gyp:client',
            '../../config/config.gyp:config_handler',
            '../../config/config.gyp:config_protocol',
            '../../ipc/ipc.gyp:ipc',
            '../../renderer/renderer.gyp:renderer_protocol',
            '../../renderer/renderer.gyp:win32_font_util',
            '../../renderer/renderer.gyp:win32_renderer_client',
            '../../session/session_base.gyp:session_protocol',
            '../base/win32_base.gyp:ime_base',
            '../base/win32_base.gyp:ime_impl_base',
          ],
        },
        {
          'target_name': 'mozc_ime',
          'product_name': '<(imefile_product_name_win)',
          'product_extension': 'ime',
          'type': 'shared_library',
          'sources': [
            '<(gen_out_dir)/GoogleIMEJa_autogen.rc',
            'GoogleIMEJa.def',
            'ime_impl_imm.cc',
            'ime_language_bar.cc',
            'ime_language_bar_menu.cc',
            'ime_module.cc',
          ],
          'dependencies': [
            '../../base/base.gyp:base',
            '../../base/base.gyp:crash_report_handler',
            '../../client/client.gyp:client',
            '../../config/config.gyp:stats_config_util',
            '../../ipc/ipc.gyp:ipc',
            '../../session/session_base.gyp:session_protocol',
            '../base/win32_base.gyp:ime_base',
            '../base/win32_base.gyp:text_icon',
            'gen_mozc_ime_resource_header',
            'ime_core',
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'BaseAddress': '0x06000000',
            },
            'VCManifestTool': {
              'EmbedManifest': 'true',
            },
          },
        },
      ]
    }],
  ],
}
