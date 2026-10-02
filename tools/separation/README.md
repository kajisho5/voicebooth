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

C++ 側との約束（DESIGN 11.3）
- front：`spec` float32 [batch, 2, 1025, frames, 2] → `x`
- layer*i*：`x` → `y`
- head：`x`, `spec` → `est`（ボーカルの複素スペクトログラム。DC は 0。伴奏 = 元 − ボーカル）
- ONNX Runtime はセッションごとのアリーナを切る（`DisableCpuMemArena`）。メモリ再利用は既定のまま
