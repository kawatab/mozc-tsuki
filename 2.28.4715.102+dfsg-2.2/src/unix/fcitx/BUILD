load(
    "//:build_defs.bzl",
    "cc_binary_mozc",
    "cc_library_mozc",
)

package(default_visibility = ["//:__subpackages__"])

cc_library_mozc(
    name = "mozc_connection",
    srcs = ["mozc_connection.cc"],
    hdrs = ["mozc_connection.h"],
    deps = [
        ":fcitx_key_util",
        ":surrounding_text_util",
        "//base:port",
        "//base:util",
        "//client:client",
        "//session:ime_switch_util",
        "//protocol:commands_cc_proto",
    ]
)

cc_library_mozc(
    name = "fcitx_mozc",
    srcs = [
        "fcitx_mozc.cc",
        "mozc_response_parser.cc",
    ],
    hdrs = [
        "fcitx_mozc.h",
        "mozc_response_parser.h",
    ],
    deps = [
        ":mozc_connection",
        ":fcitx_key_util",
        ":surrounding_text_util",
        "//base:base",
        "//base:port",
        "//base:process",
        "//base:run_level",
        "//base:util",
        "//protocol:commands_cc_proto",
        "//client:client_interface",
    ],
)

cc_library_mozc(
    name = "fcitx_key_util",
    srcs = [
        "fcitx_key_event_handler.cc",
        "fcitx_key_translator.cc",
    ],
    hdrs = [
        "fcitx_key_event_handler.h",
        "fcitx_key_translator.h",
    ],
    deps = [
        "//base:logging",
        "//base:port",
        "//protocol:config_cc_proto",
        "//protocol:commands_cc_proto",
        "@fcitx//:fcitx",
    ],
)

cc_library_mozc(
    name = "surrounding_text_util",
    srcs = [
        "surrounding_text_util.cc",
    ],
    hdrs = [
        "surrounding_text_util.h",
    ],
    deps = [
        "//base:util",
        "//base:logging",
        "//base:port",
        "@fcitx//:fcitx",
    ],
)

cc_binary_mozc(
    name = "fcitx-mozc.so",
    srcs = ["eim.cc"],
    deps = [
        ":fcitx_mozc",
        "//base:init_mozc",
        "@fcitx//:fcitx",
    ],
    local_defines = [
        'LOCALEDIR=\\"/usr/share/locale\\"',
    ],
    linkstatic = 1,
    linkshared = 1,
)

