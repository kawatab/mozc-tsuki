# Configurations

## Date format converted from "きょう"

This is an EXPERIMENTAL feature. This feature may be changed or removed in
future.

The conversion from "きょう" returns the current date in some formats like
"2021年10月1日" and "2021-10-01".

You can add an extra format by adding an entry of "DATA_FORMAT" to the user
dictionary.

| Reading     | Word                  | Category |
| ----------- | --------------------- | -------- |
| DATE_FORMAT | {YEAR}.{MONTH}.{DATE} | 名詞     |

Any category is OK so far.

| key     | value                |
| ------- | -------------------- |
| {YEAR}  | 4 digits year (2021) |
| {MONTH} | 2 digits month (10)  |
| {DATE}  | 2 digits date (01)   |
| {{}     | sinle { character    |

## Configuration path

Mozc creates configuration files under `$XDG_CONFIG_HOME/mozc` (default:
`~/.config/mozc`). If `~/.mozc` already exists, `~/.mozc` is used for backward
compatibility.

## Configuration files

`config1.db` stores configurations managed by the GUI configuration tool. The
format is binary protocol buffer of
[mozc.config.Config](https://github.com/google/mozc/blob/master/src/protocol/config.proto).

The following command can decode `config1.db` to text protocol buffer format.

```
# Decode to text proto
protoc --decode=mozc.config.Config protocol/config.proto < config1.db > config.textproto
```

```
# Encode to binary proto
protoc --encode=mozc.config.Config protocol/config.proto < config.textproto > config1.db
```

`protoc` can be built as follows

```
bazel build @com_google_protobuf//:protoc
```

## IBus

IBus specific configurations are customizable in
`~/.config/mozc/ibus_config.textproto`.

The file path may be `~/.mozc/ibus_config.textproto` if `~/.mozc` directry
already exists.

Here is the default configuration.

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "default"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: False
```

The variables of `engines` are mapped to the same named variables of IBus.

**Note:** After modification of `ibus_config.textproto`, `ibus write-cache; ibus
restart` might be necessary to apply changes.

### Specify the keyboard layout

`layout` represents the keyboard layout (e.g. JIS keyboard, US keyboard, etc.).

Sample configuration:

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "ja"  # or "us"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: False
```

### Activate Mozc on launch

`active_on_launch` represents the default input. If it's True, Hiragana input is
the default mode. `active_on_launch` is supported by 2.26.4317 or later.

Sample configuration:

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout :"default"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: True  # default is False.
```

### Multiple engines with different keyboard layouts

`engines` can be multiple entries.

```
engines {
  name : "mozc-jp"
  longname : "Mozc"
  layout : "ja"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
engines {
  name : "mozc-us"
  longname : "Mozc US"
  layout : "us"
  layout_variant : ""
  layout_option : ""
  rank : 80
}
active_on_launch: False
```
