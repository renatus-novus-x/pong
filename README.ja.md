# PONG

Human68kのランタイムAPIを使用しない、Sharp X68000向けベアメタルPONGです。C言語で実装しています。

[English](README.md)

## 特徴

- CPUと対戦する1人プレイと、ローカル2人プレイ
- キーボードと2台のゲームパッドに対応
- 3本先取と勝者表示
- ファミコン風タイトル画面

## 操作

- プレイヤー1: `W` / `S`、またはパッド1の上 / 下
- プレイヤー2: カーソル上 / 下、またはパッド2の上 / 下
- タイトル: 上 / 下で選択、Return、Space、またはパッドAで決定
- `Q` / `ESC`: 対戦中はタイトルへ戻り、タイトル画面では終了

## ビルド

WSL Ubuntu 24.04、導入済みの`elf2x68k`ツールチェーン、`python3`、`curl`が必要です。

```sh
cd src
make
```

生成物:

- `src/human.sys`
- `dist/pong.xdf`

生成したXDFを検査する場合:

```sh
cd src
make check-xdf
```

技術的な実装内容は[`docs/pong.pptx`](docs/pong.pptx)にまとめています。
