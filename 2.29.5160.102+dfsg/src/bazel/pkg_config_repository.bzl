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

"""Repository rule for Linux libraries configured by `pkg-config`.

Note, this rule supports only necessary functionaries of pkg-config for Mozc.
Generated `BUILD.bazel` is available at `bazel-src/external/<repository_name>`.

## Example of usage
```:WORKSPACE.bazel
pkg_config_repository(
  name = "ibus",
  packages = ["glib-2.0", "gobject-2.0", "ibus-1.0"],
)
```

```:BUILD.bazel
cc_library(
    name = "ibus_client",
    deps = [
        "@ibus//:ibus",
        ...
    ],
    ...
)
```
"""

BUILD_TEMPLATE = """
package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "{name}",
    hdrs = glob([
        {hdrs}
    ]),
    copts = [
        {copts}
    ],
    includes = [
        {includes}
    ],
    linkopts = [
        {linkopts}
    ],
)
"""

EXPORTS_FILES_TEMPLATE = """
exports_files(glob(["bin/*"]))
"""

def _exec_pkg_config(repo_ctx, flag):
    binary = repo_ctx.which("pkg-config")
    result = repo_ctx.execute([binary, flag] + repo_ctx.attr.packages)
    items = result.stdout.strip().split(" ")
    uniq_items = sorted({key: None for key in items}.keys())
    return uniq_items

def _make_strlist(list):
    return "\"" + "\",\n        \"".join(list) + "\""

def _symlinks(repo_ctx, paths):
    for path in paths:
        if repo_ctx.path(path).exists:
            continue
        repo_ctx.symlink("/" + path, path)

def _pkg_config_repository_impl(repo_ctx):
    includes = _exec_pkg_config(repo_ctx, "--cflags-only-I")
    includes = [item[len("-I/"):] for item in includes]
    _symlinks(repo_ctx, includes)
    data = {
        "name": repo_ctx.attr.name,
        "hdrs": _make_strlist([item + "/**" for item in includes]),
        "copts": _make_strlist(_exec_pkg_config(repo_ctx, "--cflags-only-other")),
        "includes": _make_strlist(includes),
        "linkopts": _make_strlist(_exec_pkg_config(repo_ctx, "--libs-only-l")),
    }
    build_file_data = BUILD_TEMPLATE.format(**data)

    # host_bins
    host_bins = _exec_pkg_config(repo_ctx, "--variable=host_bins")
    if len(host_bins) == 1:
        repo_ctx.symlink(host_bins[0], "bin")
        build_file_data += EXPORTS_FILES_TEMPLATE

    repo_ctx.file("BUILD.bazel", build_file_data)

pkg_config_repository = repository_rule(
    implementation = _pkg_config_repository_impl,
    attrs = {
        "packages": attr.string_list(),
    },
)
