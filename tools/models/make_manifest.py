#!/usr/bin/env python3
"""モデルの一覧（manifest.json）を作って Ed25519 で署名する（DESIGN 11.7 / B16）。持ち主の手元で使う。

  python3 make_manifest.py --model-dir parts8 --id mel-band-roformer-kj-int8-1 --role separation \
      --title "Mel-Band RoFormer (Kimberley Jensen)" --license MIT \
      --license-url https://huggingface.co/KimberleyJSN/melbandroformer \
      --base-url https://voicebooth-dl.sw-ars.com/models/mel-band-roformer-kj-int8/1/ \
      --key ~/voicebooth-keys/models-ed25519.pem --serial 1 --out out/

  - 署名は OpenSSL（3.0 以上）の Ed25519。秘密鍵（.pem）はリポジトリ・CI に置かない
  - out/manifest.json と out/manifest.json.sig（64 バイトを base64 で 1 行）を書く。同じ一覧に別のモデルを足す時は --merge で前の manifest.json を渡す
  - 歌詞の認識（B17）は 1 ファイル：--files ggml-small.bin --id whisper-small-1 --role lyrics
  - 最後に公開鍵（64 桁の 16 進）を出す。アプリの app/models/ModelManifest.cpp の builtIn に入れる（初回だけ）
"""
import argparse, base64, hashlib, json, os, subprocess, sys

BLOCK = 8 * 1024 * 1024
PARTS = ['front.onnx'] + [f'layer{i}.onnx' for i in range(6)] + ['head.onnx']


def file_entry(path, url):
    size = os.path.getsize(path)
    whole, blocks = hashlib.sha256(), []
    with open(path, 'rb') as f:
        while True:
            b = f.read(BLOCK)
            if not b:
                break
            whole.update(b)
            blocks.append(hashlib.sha256(b).hexdigest())
    return {'name': os.path.basename(path), 'url': url, 'size': size, 'sha256': whole.hexdigest(),
            'block_size': BLOCK, 'blocks': blocks}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--model-dir', required=True)
    ap.add_argument('--id', required=True)
    ap.add_argument('--role', default='separation')
    ap.add_argument('--title', required=True)
    ap.add_argument('--license', required=True)
    ap.add_argument('--license-url', default='')
    ap.add_argument('--base-url', required=True, help='ファイルを置く URL（/ で終わる。版ごとに別の場所。上書きしない）')
    ap.add_argument('--key', required=True, help='Ed25519 の秘密鍵（PEM）')
    ap.add_argument('--serial', type=int, required=True, help='一覧の版（前より大きく）')
    ap.add_argument('--files', nargs='*', default=PARTS)
    ap.add_argument('--merge', help='前の manifest.json（ほかのモデルを残す）')
    ap.add_argument('--out', required=True)
    ap.add_argument('--allow-http', action='store_true', help='開発用：手元のサーバーの http:// を許す（配布には使わない）')
    a = ap.parse_args()

    if not (a.base_url.startswith('https://') or (a.allow_http and a.base_url.startswith('http://'))) or not a.base_url.endswith('/'):
        sys.exit('--base-url は https:// で始まり / で終わること')
    models = []
    if a.merge:
        models = [m for m in json.load(open(a.merge, encoding='utf-8'))['models'] if m['id'] != a.id]
    files = [file_entry(os.path.join(a.model_dir, n), a.base_url + n) for n in a.files]
    models.append({'id': a.id, 'role': a.role, 'title': a.title, 'license': a.license,
                   'license_url': a.license_url, 'files': files})
    manifest = {'format': 'voicebooth.models', 'format_version': 1, 'serial': a.serial, 'models': models}

    os.makedirs(a.out, exist_ok=True)
    body = json.dumps(manifest, ensure_ascii=False, indent=1).encode('utf-8')
    mpath = os.path.join(a.out, 'manifest.json')
    with open(mpath, 'wb') as f:
        f.write(body)   # 署名するのはこのバイト列そのもの（書き直したら署名し直す）
    sig = subprocess.run(['openssl', 'pkeyutl', '-sign', '-inkey', a.key, '-rawin', '-in', mpath],
                         check=True, capture_output=True).stdout
    if len(sig) != 64:
        sys.exit('署名が 64 バイトではありません（Ed25519 の鍵か確かめる）')
    with open(mpath + '.sig', 'w') as f:
        f.write(base64.b64encode(sig).decode() + '\n')
    der = subprocess.run(['openssl', 'pkey', '-in', a.key, '-pubout', '-outform', 'DER'], check=True, capture_output=True).stdout
    total = sum(x['size'] for x in files)
    print(f'manifest: {mpath}  ({len(files)} files, {total / 1048576:.1f} MB)')
    print('public key (hex):', der[-32:].hex())


if __name__ == '__main__':
    main()
