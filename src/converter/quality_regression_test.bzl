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

load(
    "//:build_defs.bzl",
    "cc_test_mozc",
)

def quality_regression_test(name, src, **kwargs):
    native.genrule(
        name = name + "@data",
        srcs = [src],
        outs = [name + ".cc"],
        # TODO(b/130248329): Remove LANG when Python 3.7 or later becomes the default.
        cmd = ("LANG=en_US.UTF8 " +
               "$(location //converter:gen_quality_regression_test_data) " +
               "$< > $@"),
        tools = ["//converter:gen_quality_regression_test_data"],
    )

    cc_test_mozc(
        name = name,
        srcs = [
            "quality_regression_test.cc",
            name + "@data",
        ],
        data = [
            "//data_manager/android:mozc.data",
            "//data_manager/google:mozc.data",
            "//data_manager/oss:mozc.data",
        ],
        deps = [
            ":quality_regression_test_lib",
            ":quality_regression_util",
            "//base:port",
            "//config:config_handler",
            "//engine:eval_engine_factory",
            "//protocol:commands_cc_proto",
            "//protocol:config_cc_proto",
            "//session:request_test_util",
            "//testing:gunit_main",
            "//testing:mozctest",
        ],
        size = "large",
        **kwargs
    )

def quality_regression_tests(name, srcs, **kwargs):
    for src in srcs:
        quality_regression_test(
            "%s@%s" % (name, src.rsplit(":", 1)[1]),
            src,
            **kwargs
        )

    native.test_suite(
        name = name,
        tests = ["%s@%s" % (name, src.rsplit(":", 1)[1]) for src in srcs],
    )

def evaluation(name, outs, data_file, data_type, engine_type, test_files, base_file):
    evaluation_name = name + "_result"
    evaluation_out = evaluation_name + ".tsv"
    test_file_locations = ["$(location %s)" % file for file in test_files]

    native.genrule(
        name = evaluation_name,
        srcs = [data_file] + test_files,
        outs = [evaluation_out],
        # TODO(b/130248329): Remove LANG when Python 3.7 or later becomes the default.
        cmd = ("LANG=en_US.UTF8 " +
               "$(location //converter:quality_regression_main) " +
               "--test_files='%s' " % ",".join(test_file_locations) +
               "--data_file=$(location %s) " % data_file +
               "--data_type=%s --engine_type=%s > $@" % (data_type, engine_type)),
        exec_tools = ["//converter:quality_regression_main"],
    )

    native.genrule(
        name = name,
        srcs = [
            base_file,
            evaluation_name,
            "//base:mozc_version_txt",
        ],
        outs = outs,
        cmd = ("$(location //converter:quality_regression) " +
               "--version_file $(location //base:mozc_version_txt) " +
               "--data_type=%s " % data_type +
               "--input $(location %s) --output $@ " % evaluation_name +
               "--base $(location %s)" % base_file),
        exec_tools = ["//converter:quality_regression"],
    )
