"""分割 ONNX（BS-RoFormer）が PyTorch の元のモデルと同じ音を出すか確かめる（2026-10-02）。

8 秒（352800 サンプル）の実際の曲の抜粋 1 つで：
  1. MSST の元の forward（PyTorch、STFT 込み）                          … 基準の波形
  2. BSCore（STFT を外に出した書き直し、PyTorch）                      … 1 と波形で比べる / 3・4 の基準のスペクトログラム
  3. fp32 の分割 ONNX を front → layer0..N-1 → head の順に回したもの（--fp32 を付けた時）
  4. int8 の分割 ONNX を同じ順に回したもの（--parts）
  5. 1 つのグラフの ONNX（--single を付けた時。分割を順に回した 4 と比べる）
SNR（dB）は est（複素スペクトログラム）どうしと、iSTFT した波形どうしで出す。
ONNX Runtime の設定はアプリと同じ（スレッド内 4・メモリアリーナを切る・最適化 ALL）。

使い方：
  python verify_bs_parts.py --ckpt <ckpt> --config <yaml> --parts <int8 フォルダ> [--fp32 <fp32 フォルダ>] [--single <onnx>] \
                            --wav <44.1 kHz ステレオの曲> [--start 60]
曲はリポジトリに入れない。
"""
import argparse, json, os, time
import numpy as np
import soundfile as sf
import torch
from bs_common import BSCore, load_bs, stft_np, istft_np, snr_db, CHUNK, SR


def ort_session(path, threads):
    import onnxruntime as ort
    so = ort.SessionOptions()
    so.intra_op_num_threads = threads
    so.inter_op_num_threads = 1
    so.enable_cpu_mem_arena = False
    so.graph_optimization_level = ort.GraphOptimizationLevel.ORT_ENABLE_ALL
    return ort.InferenceSession(path, so, providers=['CPUExecutionProvider'])


def run_parts(folder, spec, threads):
    """アプリと同じ順に 1 つずつ読み、回し、捨てる"""
    layers = json.load(open(os.path.join(folder, 'parts.json')))['layers']
    x = ort_session(os.path.join(folder, 'front.onnx'), threads).run(None, {'spec': spec})[0]
    for i in range(layers):
        x = ort_session(os.path.join(folder, f'layer{i}.onnx'), threads).run(None, {'x': x})[0]
    return ort_session(os.path.join(folder, 'head.onnx'), threads).run(None, {'x': x, 'spec': spec})[0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ckpt', required=True)
    ap.add_argument('--config', required=True)
    ap.add_argument('--parts', required=True)
    ap.add_argument('--fp32', default=None)
    ap.add_argument('--single', default=None)
    ap.add_argument('--wav', required=True)
    ap.add_argument('--start', type=float, default=60.0)
    ap.add_argument('--threads', type=int, default=4)
    ap.add_argument('--msst', default=None)
    ap.add_argument('--json', default=None, help='結果を書く JSON')
    a = ap.parse_args()
    torch.set_num_threads(a.threads)

    info = sf.info(a.wav)
    assert info.samplerate == SR and info.channels == 2, '44.1 kHz ステレオの WAV / FLAC にする'
    s0 = int(a.start * SR)
    audio, _ = sf.read(a.wav, start=s0, stop=s0 + CHUNK, dtype='float32', always_2d=True)
    assert audio.shape[0] == CHUNK, '曲が短い（--start を前に）'
    chunk = audio.T[None].copy()                       # (1, 2, 352800)
    spec = stft_np(chunk)                              # (1, 2, 1025, 690, 2)
    res = {'wav': os.path.basename(a.wav), 'start_s': a.start, 'frames': int(spec.shape[3])}

    m, _ = load_bs(a.config, a.ckpt, a.msst)
    with torch.inference_mode():
        t = time.time(); ref_wave = m(torch.from_numpy(chunk)).numpy()[:, 0]; res['torch_orig_s'] = round(time.time() - t, 1)
        t = time.time(); ref_est = BSCore(m).eval()(torch.from_numpy(spec)).numpy(); res['torch_core_s'] = round(time.time() - t, 1)
    res['core_vs_orig_wave_snr_db'] = round(snr_db(ref_wave, istft_np(ref_est, CHUNK)), 1)

    outs = {}
    if a.fp32:
        t = time.time(); outs['fp32_parts'] = run_parts(a.fp32, spec, a.threads); res['fp32_parts_s'] = round(time.time() - t, 1)
    t = time.time(); outs['int8_parts'] = run_parts(a.parts, spec, a.threads); res['int8_parts_s'] = round(time.time() - t, 1)
    for k, est in outs.items():
        res[f'{k}_vs_core_est_snr_db'] = round(snr_db(ref_est, est), 1)
        res[f'{k}_vs_orig_wave_snr_db'] = round(snr_db(ref_wave, istft_np(est, CHUNK)), 1)
    if a.single:
        t = time.time(); single = ort_session(a.single, a.threads).run(None, {'spec': spec})[0]; res['single_s'] = round(time.time() - t, 1)
        res['int8_parts_vs_single_est_snr_db'] = round(snr_db(single, outs['int8_parts']), 1)
        res['int8_parts_vs_single_max_abs'] = float(np.abs(single - outs['int8_parts']).max())
        res['single_vs_core_est_snr_db'] = round(snr_db(ref_est, single), 1)
    res['dc_bin_max_abs'] = float(np.abs(outs['int8_parts'][:, :, 0]).max())
    print(json.dumps(res, ensure_ascii=False, indent=1))
    if a.json:
        json.dump(res, open(a.json, 'w'), ensure_ascii=False, indent=1)


if __name__ == '__main__':
    main()
