"""お手本の音程のモデル（RMVPE）を配布用に int8 にする（2026-10-02）

元：RVC の rmvpe.onnx（https://huggingface.co/lj1995/VoiceConversionWebUI 、MIT）
    コミット e6d0c1a17da07c33557852f9dfa2bd44cc75737d
    sha256 5370e71ac80af8b4b7c793d27efd51fd8bf962de3a7ede0766dac0befa3660fd（362 MB）
結果：rmvpe.onnx（int8、約 99 MB）。Conv / MatMul / Gemm の重みだけ動的量子化

  pip install onnx onnxruntime
  python3 quantize_rmvpe.py --src rmvpe.onnx --out out/rmvpe.onnx

int8 と元の差（MIR-1K test100 の 40 曲、声の有無は強さ 0.31 で判定）：
  伴奏なし：声の有無の食い違い 0.07%、50 セント超の差 0.01%
  伴奏 0 dB：0.49% / 0.01%、伴奏 −5 dB：1.01% / 0.04%
速さ：int8 は元の約 1.5 倍の時間（4 分の曲で 1 スレッド約 25 秒）。大きさを優先した
"""
import argparse
import hashlib
import os

from onnxruntime.quantization import QuantType, quantize_dynamic

EXPECTED = "5370e71ac80af8b4b7c793d27efd51fd8bf962de3a7ede0766dac0befa3660fd"


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="元の rmvpe.onnx")
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    if sha256(a.src) != EXPECTED:
        raise SystemExit("rmvpe.onnx の SHA-256 が違います（元のファイルか確かめてください）")
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    quantize_dynamic(a.src, a.out, weight_type=QuantType.QUInt8, op_types_to_quantize=["MatMul", "Gemm", "Conv"])
    print(a.out, os.path.getsize(a.out), sha256(a.out))


if __name__ == "__main__":
    main()
