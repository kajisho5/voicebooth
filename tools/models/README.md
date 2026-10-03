# モデルの配布（持ち主の作業。B16 / DESIGN 11.7）

アプリはモデルを自分からは取りに行かない。使う人が「分離モデルを入れる」を押した時だけ、
署名した一覧（`models/manifest.json` と `.sig`）を読み、署名が合えば中身の SHA-256 で照合しながらダウンロードする。

## いまの配布（2026-10-03）

- 一覧 serial 3：分離 `bs-roformer-anvuew-ft1-int8-1`、リード `bs-roformer-anvuew-karaoke-int8-1`、音程 `rmvpe-int8-1` を R2（voicebooth-dist）の `models/` に配置済み。
  公開 URL から全 31 ファイルを落として SHA-256 を照合し、アプリ（モデルなしの状態）から「分離モデルを入れる」でダウンロード → 照合 → 使える、まで確認
- 公開鍵 `25881bbba9aaf8e45aaac8eb610d7c651548453fa9c5b4b54b3cf8465e0711ac` を `app/models/ModelManifest.cpp` の `builtIn` に入れた。秘密鍵は持ち主が保管（リポジトリ・CI には無い）
- GPL-3.0 の 2 つのフォルダには `LICENSE.txt`（`tools/separation/LICENSE-anvuew-models.txt`）も置いた

## 1. 鍵を作る（初回だけ）

```sh
mkdir -p ~/voicebooth-keys && chmod 700 ~/voicebooth-keys
openssl genpkey -algorithm ed25519 -out ~/voicebooth-keys/models-ed25519.pem
chmod 600 ~/voicebooth-keys/models-ed25519.pem
```

- 秘密鍵はリポジトリ・CI・チャットに置かない。バックアップは暗号化した場所に（なくすと一覧を更新できない＝アプリに新しい鍵を入れて配り直し）
- 公開鍵（64 桁の 16 進）は次の手順の最後に表示される。`app/models/ModelManifest.cpp` の `builtIn` に入れてアプリを配る

## 2. 一覧を作って署名する

アプリは「分離モデルを入れる」で、同じ一覧の 3 つ（分離 → リードボーカル → 音程）のうち入っていない物をまとめて入れる（2026-10-02）。
3 つを `--merge` で 1 つの一覧にする（`--serial` は前より大きく）。部品の作り方は `tools/separation/README.md`（BS-RoFormer）と `tools/pitch/`（RMVPE）。

```sh
P="front.onnx $(for i in $(seq 0 11); do printf 'layer%d.onnx ' $i; done)head.onnx parts.json"

# 分離（BS-RoFormer ft1、anvuew、GPL-3.0。int8 約 55 MB）
python3 tools/models/make_manifest.py --model-dir bs-roformer-anvuew-ft1-int8-1 --files $P \
  --id bs-roformer-anvuew-ft1-int8-1 --role separation \
  --title "BS-RoFormer ft1 (anvuew)" --license GPL-3.0 \
  --license-url https://huggingface.co/anvuew/BS-RoFormer \
  --base-url https://voicebooth-dl.sw-ars.com/models/bs-roformer-anvuew-ft1-int8/1/ \
  --key ~/voicebooth-keys/models-ed25519.pem --serial 1 --out out/

# リードボーカル（BS-RoFormer karaoke、anvuew、GPL-3.0。ハモリのお手本に使う）
python3 tools/models/make_manifest.py --model-dir bs-roformer-anvuew-karaoke-int8-1 --files $P \
  --id bs-roformer-anvuew-karaoke-int8-1 --role karaoke \
  --title "BS-RoFormer karaoke (anvuew)" --license GPL-3.0 \
  --license-url https://huggingface.co/anvuew/karaoke_bs_roformer \
  --base-url https://voicebooth-dl.sw-ars.com/models/bs-roformer-anvuew-karaoke-int8/1/ \
  --merge out/manifest.json --key ~/voicebooth-keys/models-ed25519.pem --serial 2 --out out/

# 音程（RMVPE、RVC、MIT）
python3 tools/pitch/quantize_rmvpe.py --src rmvpe.onnx --out pitch/rmvpe.onnx   # 元の URL・SHA-256 はスクリプトの先頭に
python3 tools/models/make_manifest.py --model-dir pitch --files rmvpe.onnx \
  --id rmvpe-int8-1 --role pitch --title "RMVPE (RVC)" --license MIT \
  --license-url https://huggingface.co/lj1995/VoiceConversionWebUI \
  --base-url https://voicebooth-dl.sw-ars.com/models/rmvpe-int8/1/ \
  --merge out/manifest.json --key ~/voicebooth-keys/models-ed25519.pem --serial 3 --out out/
```

- GPL-3.0 の 2 つは変更版（ONNX に分けて int8 にした物）。変換の手順・元の重みの URL と SHA-256 は `tools/separation/README.md`（GPL §6 の対応するソース）。
  一覧の `license_url` からたどれるようにしておく。モデルの使い方を利用規約などで縛らない（§10）
- `--license-url` は配布元のモデルページ。実際の URL は `tools/separation/README.md` の表と合わせること

## 3. R2（voicebooth-dist）に置く

バージョンごとのファイルは上書きしない（別の場所に置く）。キャッシュの決まり（DESIGN 11.7）に合わせる。

```sh
put () { npx wrangler r2 object put "$1" --file "$2" --content-type "$3" --cache-control "$4" --remote; }
IMM="public, max-age=31536000, immutable"
for f in bs-roformer-anvuew-ft1-int8-1/*; do put "voicebooth-dist/models/bs-roformer-anvuew-ft1-int8/1/$(basename $f)" "$f" application/octet-stream "$IMM"; done
for f in bs-roformer-anvuew-karaoke-int8-1/*; do put "voicebooth-dist/models/bs-roformer-anvuew-karaoke-int8/1/$(basename $f)" "$f" application/octet-stream "$IMM"; done
put voicebooth-dist/models/rmvpe-int8/1/rmvpe.onnx pitch/rmvpe.onnx application/octet-stream "$IMM"
put voicebooth-dist/models/manifest.json     out/manifest.json     application/json "public, max-age=300"
put voicebooth-dist/models/manifest.json.sig out/manifest.json.sig text/plain       "public, max-age=300"
```

## 開発用（手元で試す）

- `VB_MODEL_PUBKEY=<16 進>`：アプリが信じる公開鍵を足す
- `VB_MODEL_MANIFEST_URL=<URL>`：一覧の場所を差し替える
- `VB_MODEL_ALLOW_HTTP=1`：一覧の中の http:// を許す（手元のサーバーだけ）
- `VB_SEPARATION_MODEL=<フォルダ>`：ダウンロードせずに、手元のモデルのフォルダを使う
- `VB_KARAOKE_MODEL=<フォルダ>`：ダウンロードせずに、手元のリードボーカルのモデルのフォルダを使う
- `VB_PITCH_MODEL=<rmvpe.onnx>`：ダウンロードせずに、手元の音程のモデルを使う
