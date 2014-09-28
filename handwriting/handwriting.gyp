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
    'zinnia_model_file%': '/usr/share/tegaki/models/zinnia/handwriting-ja.model',
  },
  'targets': [
    {
      'target_name': 'zinnia_handwriting',
      'type': 'static_library',
      'sources': [
        'zinnia_handwriting.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        'zinnia.gyp:zinnia',
      ],
      'conditions': [
        ['target_platform=="Linux" and use_libzinnia==1', {
          'defines': [
            'USE_LIBZINNIA',
          ],
        }],
        ['target_platform=="Linux" and use_libzinnia==1 and zinnia_model_file!=""', {
          'defines': [
            'MOZC_ZINNIA_MODEL_FILE="<(zinnia_model_file)"',
          ],
        }],
      ],
    },
    {
      'target_name': 'handwriting_manager',
      'type': 'static_library',
      'sources': [
        'handwriting_manager.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
      ],
    },
  ],
  'conditions': [
    ['enable_cloud_handwriting==1', {
      'targets': [
        {
          'target_name': 'cloud_handwriting',
          'type': 'static_library',
          'sources': [
            'cloud_handwriting.cc',
          ],
          'dependencies': [
            '../base/base.gyp:base',
            '../net/jsoncpp.gyp:jsoncpp',
            '../net/net.gyp:http_client',
          ],
        },
      ],
    }],
  ],
}
