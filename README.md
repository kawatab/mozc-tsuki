mozc-tsuki
===================================

Copyright 2019, Yasuhiro Yamakawa <kawatab@yahoo.co.jp>

This is an improved version of Mozc. With this version, you can input japanese by Tsuki 2-263 layout. At the moment, ibus-mozc, fcitx-mozc and emacs-mozc are available, and uim-mozc may be. I prepare package for Debian only.

月配列2-263式を使えるようにMozcをソースに手を加えました。今のところ、ibus-mozcとfcitx-mozc、emacs-mozcは使えます。uim-mozcはテストしていませんが、使えるかもしれません。

対応しているのはDebianのみです。

オリジナルの月配列2-263式では「・」がバックスラッシュに割り当てられていますが、横長Enterキーの場合は打ち辛い位置になってしまうので、US配列では「]」に割り当てています。

漢字直接入力が一部可能になっています。

シフト無し

```
１２３４５６７８９０－＝
そこしてょつんいのりち
はか゗とたくう゘゛きれ
すけになさっる、。゜・
```

右シフト

```
一二三四五火水木金土週円
ぁひほふめ早速会合出入
ぃをらあよ言行ゑ気目見
ぅへせゅゃ今本当何来感
```

左シフト

```
十百千万億六七八九〇〜≒
年月日時分ぬえみやぇ「　
上間ゐ前後まおもわゆ」
下大中小人むろねーぉ／
```


Build and Install
------------

### 1. Install required tools.
必要なツールをインストールする。
```
sudo apt-get install build-essential fakeroot devscripts
```
  
### 2. Make a directory for work.
作業用のディレクトリを作成する。

```
mkdir mozc-tsuki
```

### 3. Clone this repository in the work directory.
作業用のディレクトリにレポジトリをクローンする。

```
cd mozc-tsuki
git clone https://github.com/kawatab/mozc-tsuki.git 
```

### 4. Build
ビルドする。

```
dpkg-buildpackage -r -uc -b
```

### 5. Install
インストールする。

```
sudo dpkg -i fcitx-mozc_*deb mozc-server_*deb mozc-data_*deb emacs-mozc-bin_*deb emacs-mozc_*deb
```

If you have any problem, please see [Debian's building tutorial](https://wiki.debian.org/BuildingTutorial).

The original README is below.

---------

[Mozc - a Japanese Input Method Editor designed for multi-platform](https://github.com/google/mozc)
===================================

Copyright 2010-2018, Google Inc.

Mozc is a Japanese Input Method Editor (IME) designed for multi-platform such as
Android OS, Apple OS X, Chromium OS, GNU/Linux and Microsoft Windows.  This
OpenSource project originates from
[Google Japanese Input](http://www.google.com/intl/ja/ime/).

Build Status
------------

|Android + OS X + Linux + NaCl |Windows |
|:----------------------------:|:------:|
[![Build Status](https://travis-ci.org/google/mozc.svg?branch=master)](https://travis-ci.org/google/mozc) |[![Build status](https://ci.appveyor.com/api/projects/status/1rvmtp7f80jv7ehf/branch/master?svg=true)](https://ci.appveyor.com/project/google/mozc/branch/master) |

What's Mozc?
------------
For historical reasons, the project name *Mozc* has two different meanings:

1. Internal code name of Google Japanese Input that is still commonly used
   inside Google.
2. Project name to release a subset of Google Japanese Input in the form of
   source code under OSS license without any warranty nor user support.

In this repository, *Mozc* means the second definition unless otherwise noted.

Detailed differences between Google Japanese Input and Mozc are described in [About Branding](docs/about_branding.md).

Build Instructions
------------------

* [How to build Mozc in Docker](docs/build_mozc_in_docker.md): Android, NaCl, and Linux desktop builds.
* [How to build Mozc in OS X](docs/build_mozc_in_osx.md): OS X build.
* [How to build Mozc in Windows](docs/build_mozc_in_windows.md): Windows build.

Release Plan
------------

tl;dr. **There is no stable version.**

As described in [About Branding](docs/about_branding.md) page, Google does
not promise any official QA for OSS Mozc project.  Because of this,
Mozc does not have a concept of *Stable Release*.  Instead we change version
number every time when we introduce non-trivial change.  If you are
interested in packaging Mozc source code, or developing your own products
based on Mozc, feel free to pick up any version.  They should be equally
stable (or equally unstable) in terms of no official QA process.

[Release History](docs/release_history.md) page may have additional
information and useful links about recent changes.

License
-------

All Mozc code written by Google is released under
[The BSD 3-Clause License](http://opensource.org/licenses/BSD-3-Clause).
For thrid party code under [src/third_party](src/third_party) directory,
see each sub directory to find the copyright notice.  Note also that
outside [src/third_party](src/third_party) following directories contain
thrid party code.

### [src/data/dictionary_oss/](src/data/dictionary_oss)

Mixed.
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/dictionary/](src/data/test/dictionary)

The same to [src/data/dictionary_oss/](src/data/dictionary_oss).
See [src/data/dictionary_oss/README.txt](src/data/dictionary_oss/README.txt)

### [src/data/test/stress_test/](src/data/test/stress_test)

Public Domain.  See the comment in
[src/data/test/stress_test/sentences.txt](src/data/test/stress_test/sentences.txt)

### [src/data/unicode/](src/data/unicode)

UNICODE, INC. LICENSE AGREEMENT.
See each file header for details.
