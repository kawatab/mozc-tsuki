# mozc-tsuki

This is an improved version of Mozc. With this version, you can input japanese by Tsuki 2-263 layout. At the moment, ibus-mozc, fcitx-mozc and emacs-mozc are available, and uim-mozc may be. I prepare package for Debian only.

月配列2-263式を使えるようにMozcをソースに手を加えました。今のところ、ibus-mozcとfcitx-mozc、emacs-mozcは使えます。uim-mozcはテストしていませんが、使えるかもしれません。

対応しているのはDebianのみです。

オリジナルの月配列2-263式では「・」がバックスラッシュに割り当てられていますが、横長Enterキーの場合は打ち辛い位置になってしまうので、US配列では「]」に割り当てています。

漢字直接入力が一部可能になっています。

シフト無し

```１２３４５６７８９０－＝
そこしてょつんいのりち
はか゗とたくう゘゛きれ
すけになさっる、。゜・
```

右シフト

```一二三四五火水木金土週円
ぁひほふめ早速会合出入
ぃをらあよ言行ゑ気目見
ぅへせゅゃ今本当何来感
```

左シフト

```十百千万億六七八九〇〜≒
年月日時分ぬえみやぇ「　
上間ゐ前後まおもわゆ」
下大中小人むろねーぉ／
```

==========

## Build

* sudo apt-get build-dep mozc
* dpkg-buildpackage -r -uc -b

## Install

* dpkg -i ibus-mozc_*deb mozc-server_*deb mozc-data_*deb emacs-mozc-bin_*deb emacs-mozc_*deb
