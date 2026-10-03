# ボーカル分離モデルの書き出し（B16）

Mel-Band RoFormer（Kimberley Jensen、MIT）を、VoiceBoothSeparator が読む「層ごとに分けた 8 個の ONNX」にする手順。
重み・書き出したモデル・試しに使った曲はリポジトリに入れない。

必要なもの
- Python 3.11、PyTorch 2.x（CPU）、onnx、onnxruntime、onnxscript、rotary-embedding-torch、einops、beartype、librosa、pyyaml、numpy
- `msst/`：ZFTurbo/Music-Source-Separation-Training（MIT）のクローン（`models/bs_roformer/mel_band_roformer.py` と
  `configs/KimberleyJensen/config_vocals_mel_band_roformer_kj.yaml` を使う）
- `ckpt/MelBandRoformer.ckpt`：https://huggingface.co/KimberleyJSN/melbandroformer（913,106,900 バイト）

手順
1. 8 秒（352800 サンプル、44.1 kHz ステレオ）の抜粋から `chunk_spec.npy` を作る（`common.stft_np` を使う。曲は `VB_SONG` で指定）
2. `python export_parts.py` → `parts/front.onnx`・`layer0..5.onnx`・`head.onnx`（fp32、計 約 900 MB）と `ref_est.npy`（PyTorch の出力）
3. `python quantize_parts.py` → `parts8/`（int8 per-channel の動的量子化、計 約 230 MB。Gemm を MatMul + Add に直してから）
   （引数なしは前と同じ `parts/` → `parts8/`。`python quantize_parts.py <入力> <出力>` でほかのフォルダにも使える）

C++ 側との約束（DESIGN 11.3）
- front：`spec` float32 [batch, 2, 1025, frames, 2] → `x`
- layer*i*：`x` → `y`
- head：`x`, `spec` → `est`（ボーカルの複素スペクトログラム。DC は 0。伴奏 = 元 − ボーカル）
- ONNX Runtime はセッションごとのアリーナを切る（`DisableCpuMemArena`）。メモリ再利用は既定のまま

# BS-RoFormer（anvuew、GPL-3.0）の書き出し（2026-10-02）

ボーカル分離の既定を Mel-Band RoFormer から anvuew の BS-RoFormer に替えた（MUSDB18-HQ の 6 曲で int8 どうし 声の SDR +0.67 dB、6/6 曲で上）。
リードボーカルとハモリを分けるのは anvuew の karaoke BS-RoFormer。どちらも重みは **GPL-3.0**。
このページとスクリプトが、配る ONNX の「対応するソース」（GPL-3.0 第 6 条）。元の重み＋下の手順で、配っているファイルを作り直せる。
配る時はフォルダに `LICENSE-anvuew-models.txt`（このフォルダ）を添える。

| 配るフォルダ | 元の重み | 出すもの | parts.json |
|---|---|---|---|
| `bs-roformer-anvuew-ft1-int8-1/` | BS-RoFormer ft1（SDR 12.55） | 声（伴奏 = 元 − 声） | `{"arch":"bs-roformer","hop":512,"layers":12,"chunk":352800,"stem":"vocals"}` |
| `bs-roformer-anvuew-karaoke-int8-1/` | karaoke BS-RoFormer | リードボーカル（それ以外 = 元 − リード） | `{"arch":"bs-roformer","hop":512,"layers":12,"chunk":352800,"stem":"lead"}` |

中身はどちらも `front.onnx`・`layer0..11.onnx`・`head.onnx`（int8、計 約 53 MB）と `parts.json`。

## 元の重み

作者 anvuew（https://huggingface.co/anvuew）、ライセンス GPL-3.0。

| ファイル | URL | バイト | SHA-256 |
|---|---|---|---|
| ft1 の ckpt | https://huggingface.co/anvuew/BS-RoFormer/resolve/main/bs_roformer_ft1_anvuew_sdr_12.55.ckpt | 204,493,312 | `60347271e8493fdff28ef558c3b2297afb869a1f4462594ed548038264bec395` |
| ft1 の設定 | https://huggingface.co/anvuew/BS-RoFormer/resolve/main/config.yaml | 1,969 | `033e57abee226a480ef410e0631b9fe00e8b7bc9a3bb5fe55659d0b43a238701` |
| karaoke の ckpt | https://huggingface.co/anvuew/karaoke_bs_roformer/resolve/main/karaoke_bs_roformer_anvuew.ckpt | 204,486,925 | `206d04757cb5f75ca3b55f8a0a48f5c26aa2351d4ff3c7adbfc9affa30ea3ae4` |
| karaoke の設定 | https://huggingface.co/anvuew/karaoke_bs_roformer/resolve/main/karaoke_bs_roformer_anvuew.yaml | 1,973 | `5cb3f127ecbc6a8e37f31ea7e05f60f360a44da43e857bde805b7b68558f6338` |

確かめた時のリポジトリのバージョン：`anvuew/BS-RoFormer` 24988f47270cb3529b62c4f3bbb8234f4586de9b、
`anvuew/karaoke_bs_roformer` 0d4423d42e12cf2ba39ae09171028507b8a2a7be（`resolve/main` を `resolve/<バージョン>` にすると固定できる）。

## 必要なもの

- Python 3.11、PyTorch 2.x（CPU。2.14.1 で確かめた）、onnx（1.23）、onnxruntime（1.30）、onnxscript（0.7）、
  rotary-embedding-torch、einops、beartype、pyyaml、numpy、soundfile（確かめる時だけ）
- `msst/`：ZFTurbo/Music-Source-Separation-Training（MIT、https://github.com/ZFTurbo/Music-Source-Separation-Training）のクローン。
  `models/bs_roformer/bs_roformer.py`（モデルの定義）と `models/bs_roformer/attend.py` を使う
  （84b1eac0887756b4f1a9d7a1ff49105939749ed2 で確かめた）。別の場所なら `--msst` か `VB_MSST`
- `ckpt/`：上の 4 つ（SHA-256 を照合する）

## 手順

```sh
# ft1（声 / 伴奏）
python export_bs_parts.py --ckpt ckpt/bs_roformer_ft1_anvuew_sdr_12.55.ckpt --config ckpt/config.yaml \
    --stem vocals --out bs_ft1_parts                         # fp32、計 約 200 MB ＋ parts.json
python quantize_parts.py bs_ft1_parts bs-roformer-anvuew-ft1-int8-1

# karaoke（リードボーカル）
python export_bs_parts.py --ckpt ckpt/karaoke_bs_roformer_anvuew.ckpt --config ckpt/karaoke_bs_roformer_anvuew.yaml \
    --stem lead --out bs_karaoke_parts
python quantize_parts.py bs_karaoke_parts bs-roformer-anvuew-karaoke-int8-1

# 確かめる（手元の 44.1 kHz ステレオの曲の 8 秒。曲はリポジトリに入れない）
python verify_bs_parts.py --ckpt ckpt/bs_roformer_ft1_anvuew_sdr_12.55.ckpt --config ckpt/config.yaml \
    --parts bs-roformer-anvuew-ft1-int8-1 --fp32 bs_ft1_parts --wav song.wav --start 60
```

- `bs_common.py`：ckpt の読み込み、書き出し用に書き直した本体 `BSCore`（MSST の `BSRoformer.forward` から STFT / iSTFT を外したもの）、
  front / layer / head の分け方、numpy の STFT（hop 512）
- `export_bs_parts.py`：fp32 の分割 ONNX（opset 18、torch.onnx の dynamo 経路。バッチ 1..16・フレーム 16..4096 が動的）と `parts.json`。
  書き出しの入力は決まった種の乱数（形を決めるためだけ。曲は使わない）
- `quantize_parts.py`：Kim と同じ int8 の手順（Gemm → MatMul + Add、MatMul の重みだけ per-channel の動的量子化）。`parts.json` も写す
- `verify_bs_parts.py`：元の PyTorch と、分割を順に回した ONNX を比べる（SNR）

## 確かめた結果（2026-10-02、MUSDB18-HQ の 1 曲の 8 秒、690 フレーム）

VERIFY_TABLE

C++ 側との約束は上の Mel-Band RoFormer と同じ（front：`spec` → `x`、layer*i*：`x` → `y`、head：`x`, `spec` → `est`。DC は 0）。
違うのは hop 512・12 層で、フォルダの `parts.json` から読む（無ければ Mel-Band RoFormer の 441・6 層）。
