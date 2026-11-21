# Back to the Future: The Game エピソード1 日本語化パッチ（プログラム）

dinput8.dll の代理 DLL です。次の 2 つをして、ゲームを日本語にします。

- ゲームが読む `*_english.landb`（会話・字幕・UI の文）を、訳を入れたものに差し替える。差し替えたファイルは一時フォルダ（`%TEMP%\BTTFJP`）に作り、ファイル API のフックでゲームの `Pack` フォルダにあるように見せる。
- フォントの字を返す関数（Font::GetGlyph）をフックし、EUC-JP の 2 バイトの字を自前の字のテクスチャから描く。

ゲームのファイルは書き換えません。

## 組み立て方
1. `python tools/extract.py` で英語の文を取り出す（`text/en.json`）
2. 訳を `text/ja.json`（英文→訳）と `text/ids.json`（`ファイル名#ID`→訳）に用意する
3. `python tools/make_font.py` で字のフォント（build/font.ttf）を、`python tools/make_data.py` で訳のデータ（build/tr.bin）を作る
4. `build.bat`（Visual Studio の開発者コマンドプロンプトで実行）で build/dinput8.dll を作る

## 用意するもの
- Windows、Visual Studio 2022（cl・rc、x86）、Python 3（fontTools）
- 原作のゲーム（Steam 版）。原作のファイルはここには入っていないので、各自用意してください。
- 訳のデータはここには入っていません。

## ライセンス
MIT（リポジトリ直下の LICENSE）。
