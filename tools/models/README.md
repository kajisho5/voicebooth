# モデルの配布（持ち主の作業。B16 / DESIGN 11.7）

アプリはモデルを自分からは取りに行かない。使う人が「分離モデルを入れる」を押した時だけ、
署名した一覧（`models/manifest.json` と `.sig`）を読み、署名が合えば中身の SHA-256 で照合しながらダウンロードする。

## 1. 鍵を作る（初回だけ）

```sh
mkdir -p ~/voicebooth-keys && chmod 700 ~/voicebooth-keys
openssl genpkey -algorithm ed25519 -out ~/voicebooth-keys/models-ed25519.pem
chmod 600 ~/voicebooth-keys/models-ed25519.pem
```

- 秘密鍵はリポジトリ・CI・チャットに置かない。バックアップは暗号化した場所に（なくすと一覧を更新できない＝アプリに新しい鍵を入れて配り直し）
- 公開鍵（64 桁の 16 進）は次の手順の最後に表示される。`app/models/ModelManifest.cpp` の `builtIn` に入れてアプリを配る

## 2. 一覧を作って署名する

```sh
python3 tools/models/make_manifest.py --model-dir parts8 \
  --id mel-band-roformer-kj-int8-1 --role separation \
  --title "Mel-Band RoFormer (Kimberley Jensen)" --license MIT \
  --license-url https://huggingface.co/KimberleyJSN/melbandroformer \
  --base-url https://voicebooth-dl.sw-ars.com/models/mel-band-roformer-kj-int8/1/ \
  --key ~/voicebooth-keys/models-ed25519.pem --serial 1 --out out/
```

`parts8/` は `tools/separation/` で作る 8 個の ONNX（約 230 MB）。

## 3. R2（voicebooth-dist）に置く

版ごとのファイルは上書きしない（別の場所に置く）。キャッシュの決まり（DESIGN 11.7）に合わせる。

```sh
for f in parts8/*.onnx; do
  npx wrangler r2 object put "voicebooth-dist/models/mel-band-roformer-kj-int8/1/$(basename $f)" --file "$f" \
    --content-type application/octet-stream --cache-control "public, max-age=31536000, immutable" --remote
done
npx wrangler r2 object put voicebooth-dist/models/manifest.json     --file out/manifest.json     --content-type application/json --cache-control "public, max-age=300" --remote
npx wrangler r2 object put voicebooth-dist/models/manifest.json.sig --file out/manifest.json.sig --content-type text/plain       --cache-control "public, max-age=300" --remote
```

## 開発用（手元で試す）

- `VB_MODEL_PUBKEY=<16 進>`：アプリが信じる公開鍵を足す
- `VB_MODEL_MANIFEST_URL=<URL>`：一覧の場所を差し替える
- `VB_MODEL_ALLOW_HTTP=1`：一覧の中の http:// を許す（手元のサーバーだけ）
- `VB_SEPARATION_MODEL=<フォルダ>`：ダウンロードせずに、手元のモデルのフォルダを使う
