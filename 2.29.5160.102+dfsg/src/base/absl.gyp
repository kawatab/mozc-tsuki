# Copyright 2010-2021, Google Inc.
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
    'gen_absl_dir': '<(SHARED_INTERMEDIATE_DIR)/third_party/abseil-cpp/absl',
    'glob_absl': '<(glob) --notest --base <(absl_srcdir) --subdir',
  },
  'targets': [
    {
      'target_name': 'absl_base',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'all_dependent_settings': {
        'include_dirs': [
          '<(proto_out_dir)',  # make generated files (*.pb.h) visible.
        ],
      },
      'conditions': [
        ['use_libabseil==1', {
          'link_settings': {
            'libraries': [
              '-latomic -labsl_base -labsl_int128 -labsl_base -labsl_hash -labsl_city -labsl_flags_reflection -labsl_raw_hash_set -labsl_str_format_internal -labsl_throw_delegate -labsl_time_zone -labsl_hashtablez_sampler -labsl_synchronization -labsl_time -labsl_strings_internal -labsl_strings -labsl_spinlock_wait -labsl_status -labsl_statusor -labsl_flags_internal -labsl_flags_usage_internal -labsl_flags_marshalling -labsl_flags_parse -labsl_string_view -labsl_raw_logging_internal -labsl_random_internal_randen -labsl_random_internal_randen_slow -labsl_random_internal_randen_hwaes -labsl_random_internal_randen_hwaes_impl -labsl_random_internal_pool_urbg -labsl_random_internal_seed_material',
            ],
          },
        },],
      ],
    },
  ],
}
