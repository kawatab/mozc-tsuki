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
    'relative_dir': 'rewriter',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'rewriter',
      'type': 'static_library',
      'sources': [
        '<(gen_out_dir)/embedded_collocation_data.h',
        '<(gen_out_dir)/embedded_collocation_suppression_data.h',
        '<(gen_out_dir)/emoji_rewriter_data.h',
        '<(gen_out_dir)/emoticon_rewriter_data.h',
        '<(gen_out_dir)/single_kanji_rewriter_data.h',
        '<(gen_out_dir)/symbol_rewriter_data.h',
        'calculator_rewriter.cc',
        'collocation_rewriter.cc',
        'collocation_util.cc',
        'correction_rewriter.cc',
        'command_rewriter.cc',
        'date_rewriter.cc',
        'dice_rewriter.cc',
        'dictionary_generator.cc',
        'emoji_rewriter.cc',
        'emoticon_rewriter.cc',
        'english_variants_rewriter.cc',
        'focus_candidate_rewriter.cc',
        'fortune_rewriter.cc',
        'katakana_promotion_rewriter.cc',
        'language_aware_rewriter.cc',
        'normalization_rewriter.cc',
        'number_compound_util.cc',
        'number_rewriter.cc',
        'remove_redundant_candidate_rewriter.cc',
        'rewriter.cc',
        'single_kanji_rewriter.cc',
        'symbol_rewriter.cc',
        'transliteration_rewriter.cc',
        'unicode_rewriter.cc',
        'usage_rewriter.cc',
        'user_boundary_history_rewriter.cc',
        'user_dictionary_rewriter.cc',
        'user_segment_history_rewriter.cc',
        'variants_rewriter.cc',
        'version_rewriter.cc',
        'zipcode_rewriter.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:config_file_stream',
        '../base/base.gyp:serialized_string_array',
        '../composer/composer.gyp:composer',
        '../config/config.gyp:character_form_manager',
        '../config/config.gyp:config_handler',
        '../converter/converter_base.gyp:immutable_converter',
        '../data_manager/data_manager_base.gyp:serialized_dictionary',
        '../dictionary/dictionary.gyp:dictionary',
        '../dictionary/dictionary_base.gyp:pos_matcher',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../request/request.gyp:conversion_request',
        '../storage/storage.gyp:storage',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        'calculator/calculator.gyp:calculator',
        'rewriter_base.gyp:gen_rewriter_files#host',
      ],
      'conditions':[
        ['target_platform=="Android"', {
          'sources!': [
            'usage_rewriter.cc',
          ],
        }],
      ]
    },
  ],
}
