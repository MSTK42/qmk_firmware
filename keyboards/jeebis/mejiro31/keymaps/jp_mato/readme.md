# Mejiro31 mato direct

通常入力をmato中心にし、設定画面のキー図から直接変更するプロファイルです。
JEEBIS27/qmk_firmware `af77bf67ec664c1718ce3d61b884d63ff1cc7fb5` を土台に、
現行の共通Mejiro Engine・辞書編集・録音機能・Vial WebHIDを使います。
レビュー用PRの比較先は、この公式コミットを固定した `codex/mejiro31-upstream-20261103`
です。既存masterと旧hybridの履歴・成果物を保持したまま移植差分を確認できます。

## 操作

- 通常時はmato。日本語・英語とも同じ直接キーマップを使用します。
- 中央親指キーは通常入力とメジロ式の切り替えです。
- 左親指外側は短押しでIME切り替え、長押し／他キーとの組み合わせで数字・記号。
- 左親指内側はSpace／カーソル編集、右親指内側はEnter／Fキー。
- 右親指外側は子音切替。元の5層と8組の修飾キー同時押しを保持します。
- Mejiro31の追加7キーはEsc、Tab、Delete、Backspace、IME OFF、モード、IME ON。
  これらも編集できます。

画面の「基本」「子音切替」「数字・記号」「カーソル編集」「Fキー」はそれぞれ
レイヤー0〜4、「メジロ式」はレイヤー5です。TapとHoldを別々に編集できます。
Shiftによる自動レイヤー切替とAlt配列の再変換はありません。

記号は表示文字のまま指定します。押しながら使うShiftは作者のJIS記号対に従います。
例えば `-`→`=`、`:`→`*`、`@`→バッククォートです。JIS/USホスト設定に応じて
必要なHIDキーへ変換します。USホストでは円記号はバックスラッシュになり、
JISのShift+0には印字できる文字がありません。

## 専用設定画面との契約

- WebHID応答のprofileは2（mato）。JP=0／EN=1とは設定JSONを混用しません。
- 12面×58論理キーを公式と同じ16×4×12ストレージへ保存します。
- 初期設定はJIS、メジロ式ON、Alt配列OFF、配列IDは両方0。
- 同時押し判定は50ms未満、親指の単独長押しは175ms。
- コンボは最初のキーを離すと修飾を解除し、残りキーから文字を出しません。
- `MJI_IME` は内部の入力言語を切り替え、LANG1/LANG2でIME ON/OFFを送ります。
  OSや別キーボード側で切り替えた状態は取得できません。ずれたときは追加の
  IME ON/OFFキーを押して合わせます。

初回は設定画面でmatoの初期値を確認してから「キーボードへ書き込み」を使います。
公式JP/EN設定を読み込んでmato配置として上書きしないでください。

## 再生成とビルド

`mato-source.json` は2026-10-23に作者の7図から目視転記した既存の検証済み資料です。
[作者の記事](https://note.com/mato_kb/n/nf57efe5d6ca0)が原典です。作者が配布したソースコードではありません。

```sh
python3 keyboards/jeebis/mejiro31/keymaps/jp_mato/generate_defaults.py --check
python3 keyboards/jeebis/mejiro31/build_vial.py --keymap jp_mato
```

CI `build_mejiro31_mato_direct.yml` は通常JP/EN回帰、mato入力・記号・WebHID、
リンク済みUSB定義、UF2構造、ARM上の共通Engine初期化・録音再生・スタックを検査します。
配布UF2と同じELFから `mejiro31_jp_mato.defaults.json` を抽出し、設定画面の初期値と照合します。
この検査は実機のUSB通信、OSのIME、実際の打鍵を確認したことにはなりません。

旧matoハイブリッド、旧UF2、公式復旧用UF2は別に保持します。本変更では実機への書込みや
既存ブランチへのマージを行いません。
