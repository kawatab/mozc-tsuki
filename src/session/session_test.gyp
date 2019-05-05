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
    'relative_dir': 'session',
    'gen_out_dir': '<(SHARED_INTERMEDIATE_DIR)/<(relative_dir)',
  },
  'targets': [
    {
      'target_name': 'session_handler_test_util',
      'type' : 'static_library',
      'sources': [
        'session_handler_test_util.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:config_handler',
        '../engine/engine.gyp:engine_factory',
        '../engine/engine.gyp:mock_data_engine_factory',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../testing/testing.gyp:testing',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session',
        'session.gyp:session_handler',
        'session.gyp:session_usage_observer',
      ],
    },
    {
      'target_name': 'session_server_test',
      'type': 'executable',
      'sources': [
        'session_server_test.cc',
      ],
      'dependencies': [
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session.gyp:session_server',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_test',
      'type': 'executable',
      'sources': [
        'session_test.cc',
      ],
      'dependencies': [
        '../converter/converter_base.gyp:converter_mock',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../engine/engine.gyp:engine',
        '../engine/engine.gyp:mock_converter_engine',
        '../engine/engine.gyp:mock_data_engine_factory',
        '../rewriter/rewriter.gyp:rewriter',
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_regression_test',
      'type': 'executable',
      'sources': [
        'session_regression_test.cc',
      ],
      'dependencies': [
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../engine/engine.gyp:engine_factory',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
        'session.gyp:session_server',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'session_handler_test',
      'type': 'executable',
      'sources': [
        'session_handler_test.cc',
      ],
      'dependencies': [
        '../base/base_test.gyp:clock_mock',
        '../converter/converter_base.gyp:converter_mock',
        '../engine/engine.gyp:mock_converter_engine',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
      'conditions': [
        ['target_platform=="NaCl" and _toolset=="target"', {
          'dependencies!': [
            'session.gyp:session_server',
          ],
        }],
      ],
    },
    {
      'target_name': 'session_converter_test',
      'type': 'executable',
      'sources': [
        'session_converter_test.cc',
      ],
      'dependencies': [
        '../converter/converter_base.gyp:converter_mock',
        '../data_manager/testing/mock_data_manager.gyp:mock_data_manager',
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:testing_util',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session',
        'session_base.gyp:request_test_util',
      ],
    },
    {
      'target_name': 'session_module_test',
      'type': 'executable',
      'sources': [
        'output_util_test.cc',
        'session_observer_handler_test.cc',
        'session_usage_observer_test.cc',
        'session_usage_stats_util_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base_test.gyp:clock_mock',
        '../base/base_test.gyp:scheduler_stub',
        '../client/client.gyp:client_mock',
        '../config/config.gyp:config_handler',
        '../config/config.gyp:stats_config_util',
        '../protocol/protocol.gyp:commands_proto',
        '../testing/testing.gyp:gtest_main',
        '../usage_stats/usage_stats_base.gyp:usage_stats',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session_handler',
        'session.gyp:session_usage_observer',
        'session_base.gyp:keymap',
        'session_base.gyp:keymap_factory',
        'session_base.gyp:output_util',
        'session_base.gyp:session_usage_stats_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      # Android is not supported.
      'target_name': 'session_watch_dog_test',
      'type': 'executable',
      'sources': [
        'session_watch_dog_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../client/client.gyp:client_mock',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session_watch_dog',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_key_handling_test',
      'type': 'executable',
      'sources': [
        'ime_switch_util_test.cc',
        'key_info_util_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../config/config.gyp:config_handler',
        '../protocol/protocol.gyp:commands_proto',
        '../testing/testing.gyp:gtest_main',
        'session_base.gyp:ime_switch_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_internal_test',
      'type': 'executable',
      'sources': [
        'internal/candidate_list_test.cc',
        'internal/ime_context_test.cc',
        'internal/keymap_test.cc',
        'internal/keymap_factory_test.cc',
        'internal/session_output_test.cc',
        'internal/key_event_transformer_test.cc',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../converter/converter_base.gyp:converter_mock',
        '../engine/engine.gyp:mock_converter_engine',
        '../protocol/protocol.gyp:commands_proto',
        '../protocol/protocol.gyp:config_proto',
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:testing_util',
        'session.gyp:session',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_handler_stress_test',
      'type': 'executable',
      'sources': [
        'session_handler_stress_test.cc'
      ],
      'dependencies': [
        '../engine/engine.gyp:engine_factory',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:random_keyevents_generator',
        'session.gyp:session',
        'session.gyp:session_server',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'random_keyevents_generator_test',
      'type': 'executable',
      'sources': [
        'random_keyevents_generator_test.cc',
      ],
      'dependencies': [
        '../protocol/protocol.gyp:commands_proto',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:random_keyevents_generator',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'session_converter_stress_test',
      'type': 'executable',
      'sources': [
        'session_converter_stress_test.cc'
      ],
      'dependencies': [
        '../engine/engine.gyp:mock_data_engine_factory',
        '../testing/testing.gyp:gtest_main',
        'session.gyp:session',
      ],
      'variables': {
        'test_size': 'large',
      },
    },
    {
      'target_name': 'generic_storage_manager_test',
      'type': 'executable',
      'sources': [
        'generic_storage_manager_test.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/testing.gyp:gtest_main',
        'session_base.gyp:generic_storage_manager',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'request_test_util_test',
      'type': 'executable',
      'sources': [
        'request_test_util_test.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../testing/testing.gyp:gtest_main',
        'session_base.gyp:request_test_util',
      ],
      'variables': {
        'test_size': 'small',
      },
    },
    {
      'target_name': 'session_handler_scenario_test',
      'type': 'executable',
      'sources': [
        'session_handler_scenario_test.cc'
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../data/test/session/scenario/scenario.gyp:install_session_handler_scenario_test_data',
        '../data/test/session/scenario/usage_stats/usage_stats.gyp:install_session_handler_usage_stats_scenario_test_data',
        '../engine/engine.gyp:mock_data_engine_factory',
        '../protocol/protocol.gyp:commands_proto',
        '../testing/testing.gyp:gtest_main',
        '../testing/testing.gyp:mozctest',
        '../usage_stats/usage_stats_test.gyp:usage_stats_testing_util',
        'session.gyp:session_handler',
        'session_base.gyp:request_test_util',
        'session_handler_test_util',
      ],
      'variables': {
        'test_size': 'large',
      },
    },

    # Test cases meta target: this target is referred from gyp/tests.gyp
    {
      'target_name': 'session_all_test',
      'type': 'none',
      'dependencies': [
        'generic_storage_manager_test',
        'random_keyevents_generator_test',
        'request_test_util_test',
        'session_converter_stress_test',
        'session_converter_test',
        'session_handler_scenario_test',
        'session_handler_stress_test',
        'session_handler_test',
        'session_key_handling_test',
        'session_internal_test',
        'session_module_test',
        'session_regression_test',
        'session_server_test',
        'session_test',
        'session_watch_dog_test',
      ],
      'conditions': [
        ['target_platform=="Android"', {
          'dependencies!': [
            'session_server_test',
            'session_watch_dog_test',
            # These tests have been disabled as it takes long execution time.
            # In addition currently they fail.
            # Here we also disable the tests temporarirly.
            # TODO(matsuzakit): Reactivate them.
            'session_handler_stress_test',
            'session_regression_test',
          ],
        }],
      ],
    },
  ],
}
