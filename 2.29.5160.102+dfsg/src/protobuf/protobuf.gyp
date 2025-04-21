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
    # We accept following warnings come from protobuf.
    # This list should be revised when protobuf is updated.
    'msvc_disabled_warnings_for_protoc': [
      # switch statement contains 'default' but no 'case' labels.
      # https://msdn.microsoft.com/en-us/library/aa748818.aspx
      '4065',
      # unary minus operator applied to unsigned type, result still unsigned.
      # http://msdn.microsoft.com/en-us/library/4kh09110.aspx
      '4146',
      # 'this' : used in base member initializer list
      # http://msdn.microsoft.com/en-us/library/3c594ae3.aspx
      '4355',
      # no definition for inline function.
      # https://msdn.microsoft.com/en-us/library/aa733865.aspx
      '4506',
      # 'type' : forcing value to bool 'true' or 'false'
      # (performance warning)
      # http://msdn.microsoft.com/en-us/library/b6801kcy.aspx
      '4800',
    ],

    # We accept following warnings come from protobuf header files.
    # This list should be revised when protobuf is updated.
    'msvc_disabled_warnings_for_proto_headers': [
      # unary minus operator applied to unsigned type, result still unsigned.
      # http://msdn.microsoft.com/en-us/library/4kh09110.aspx
      '4146',
      # 'type' : forcing value to bool 'true' or 'false'
      # (performance warning)
      # http://msdn.microsoft.com/en-us/library/b6801kcy.aspx
      '4800',
    ],

    'protobuf_cpp_root': '<(protobuf_root)/src/google/protobuf',
    'glob_protobuf': '<(glob) --notest --base <(protobuf_cpp_root) --subdir',

    # Sources for Proto3.
    'protobuf_sources': [
      '<!@(<(glob_protobuf) . "*.cc" --exclude "*.pb.cc" reflection_tester.cc)',
      '<!@(<(glob_protobuf) io "*.cc")',
      '<!@(<(glob_protobuf) stubs "*.cc")',
      '<(protobuf_cpp_root)/descriptor.pb.cc',
      '<!@(<(glob) --notest --base <(protobuf_root)/third_party/utf8_range "*.cc")',
    ],
    # Sources for protoc (common part and C++ generator only).
    'protoc_sources': [
      '<!@(<(glob_protobuf) . "*.cc" --exclude "*.pb.cc" reflection_tester.cc)',
      '<!@(<(glob_protobuf) compiler "*.cc" --exclude main.cc)',
      '<!@(<(glob_protobuf) compiler/cpp "**/*.cc")',
      '<!@(<(glob_protobuf) io "*.cc")',
      'custom_protoc_main.cc',
      '<!@(<(glob) --notest --base <(protobuf_root)/third_party/utf8_range "*.cc")',
    ],
  },
  'targets': [
    {
      'target_name': 'protobuf',
      'type': 'static_library',
      'toolsets': ['host', 'target'],
      'all_dependent_settings': {
        'include_dirs': [
          '<(proto_out_dir)',  # make generated files (*.pb.h) visible.
        ],
      },
      'conditions': [
        ['use_libprotobuf==1', {
          'link_settings': {
            'libraries': [
              '-lprotobuf -latomic',
            ],
          },
        },
        {  # else
          'sources': ['<@(protobuf_sources)'],
          'dependencies': [
              '../base/absl.gyp:absl_log',
              '../base/absl.gyp:absl_strings',
              '../base/absl.gyp:absl_synchronization',
          ],
          'include_dirs': [
            '<(protobuf_root)/src',
            '<(protobuf_root)/third_party/utf8_range',
          ],
          'all_dependent_settings': {
            'include_dirs': [
              '<(protobuf_root)/src',
            ],
            'msvs_disabled_warnings': [
              '<@(msvc_disabled_warnings_for_proto_headers)',
            ],
          },
          'msvs_disabled_warnings': [
            '<@(msvc_disabled_warnings_for_protoc)',
          ],
          'xcode_settings': {
            'USE_HEADERMAP': 'NO',
          },
          'conditions': [
            ['(_toolset=="target" and (compiler_target=="clang" or compiler_target=="gcc")) or '
             '(_toolset=="host" and (compiler_host=="clang" or compiler_host=="gcc"))', {
              'cflags': [
                '-Wno-invalid-noreturn',
                '-Wno-tautological-constant-out-of-range-compare',
                '-Wno-unused-const-variable',
                '-Wno-unused-function',
              ],
            }],
            ['OS=="win"', {
              'defines!': [
                'WIN32_LEAN_AND_MEAN',  # protobuf already defines this
              ],
            }],
            ['OS!="win"', {
              'defines': [
                'HAVE_PTHREAD',  # only needed in google/protobuf/stubs/common.cc for now.
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'protoc',
      'type': 'executable',
      'toolsets': ['host'],
      'dependencies': [
        'protobuf',
      ],
      'conditions': [
        ['use_libprotobuf==0', {
          'sources': ['<@(protoc_sources)'],
          'dependencies': [
              '../base/absl.gyp:absl_log',
              '../base/absl.gyp:absl_strings',
              '../base/absl.gyp:absl_synchronization',
              '../base/absl.gyp:absl_types',
          ],
          'include_dirs': [
            '<(protobuf_root)/src',
            '<(protobuf_root)/third_party/utf8_range',
          ],
          'msvs_disabled_warnings': [
            '<@(msvc_disabled_warnings_for_protoc)',
          ],
          'xcode_settings': {
            'USE_HEADERMAP': 'NO',
          },
          'conditions': [
            ['(_toolset=="target" and (compiler_target=="clang" or compiler_target=="gcc")) or '
             '(_toolset=="host" and (compiler_host=="clang" or compiler_host=="gcc"))', {
              'cflags': [
                '-Wno-invalid-noreturn',
                '-Wno-tautological-constant-out-of-range-compare',
                '-Wno-unused-const-variable',
                '-Wno-unused-function',
                '-Wno-unused-private-field',
              ],
            }],
            ['OS=="win"', {
              'defines!': [
                'WIN32_LEAN_AND_MEAN',  # protobuf already defines this
              ],
            }],
          ],
        }],
      ],
    },
  ],
}
