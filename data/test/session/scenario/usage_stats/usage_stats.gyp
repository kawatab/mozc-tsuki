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
    'relative_dir': 'data/test/session/scenario/usage_stats',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'install_session_handler_usage_stats_scenario_test_data',
      'type': 'none',
      'variables': {
        'test_data': [
          "conversion.txt",
          "prediction.txt",
          "suggestion.txt",
          "composition.txt",
          "select_prediction.txt",
          "select_minor_conversion.txt",
          "select_minor_prediction.txt",
          "mouse_select_from_suggestion.txt",
          "select_t13n_by_key.txt",
          "select_t13n_on_cascading_window.txt",
          "switch_kana_type.txt",
          "multiple_segments.txt",
          "select_candidates_in_multiple_segments.txt",
          "select_candidates_in_multiple_segments_and_expand_segment.txt",
          "continue_input.txt",
          "continuous_input.txt",
          "multiple_sessions.txt",
          "backspace_after_commit.txt",
          "backspace_after_commit_after_backspace.txt",
          "multiple_backspace_after_commit.txt",
          "zero_query_suggestion.txt",
          "auto_partial_suggestion.txt",
          "insert_space.txt",
          "numpad_in_direct_input_mode.txt",
          "language_aware_input.txt",
        ],
        'test_data_subdir': 'data/test/session/scenario/usage_stats',
      },
      'includes': ['../../../../../gyp/install_testdata.gypi'],
    },
  ],
}