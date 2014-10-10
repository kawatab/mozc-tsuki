#mozc-tsuki

This is an improved version of Mozc. With this version, you can input japanese by Tsuki 2-263 layout. At the moment, ibus-mozc, fcitx-mozc and emacs-mozc are available, and uim-mozc may be. I prepare package for Debian only.

月配列2-263式を使えるようにMozcをソースに手を加えました。今のところ、ibus-mozcとfcitx-mozc、emacs-mozcは使えます。uim-mozcとfcitx-mozcはテストしていませんが、使えるかもしれません。

対応しているのはDebianのみです。

オリジナルの月配列2-263式では「・」がバックスラッシュに割り当てられていますが、横長Enterキーの場合は打ち辛い位置になってしまうので、US配列では「]」に割り当てています。


==========

##Build

* dpkg-buildpackage -r -uc -b

##Install

* dpkg -i ibus-mozc_*deb mozc-server_*deb mozc-data_*deb emacs-mozc-bin_*deb emacs-mozc_*deb
