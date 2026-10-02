"""anvuew の BS-RoFormer（GPL-3.0）の ckpt + yaml を、VoiceBoothSeparator が読む分割 ONNX（fp32）にする（2026-10-02）。

出力：<out>/front.onnx・layer0..11.onnx・head.onnx（fp32、計 約 200 MB）と parts.json
      （int8 にするのは quantize_parts.py。parts.json もそのまま写す）

使い方：
  python export_bs_parts.py --ckpt ckpt/bs_roformer_ft1_anvuew_sdr_12.55.ckpt --config ckpt/config.yaml \
                            --stem vocals --out bs_ft1_parts
  python export_bs_parts.py --ckpt ckpt/karaoke_bs_roformer_anvuew.ckpt --config ckpt/karaoke_bs_roformer_anvuew.yaml \
                            --stem lead --out bs_karaoke_parts
  （MSST のクローンは --msst か環境変数 VB_MSST。既定は このフォルダの msst/）

書き出しの入力（形を決めるためだけ）は決まった種の乱数。重みにも出力にも曲は入らない。
バッチ（1..16）とフレーム数（16..4096）は動的。opset 18、torch.onnx の dynamo 経路。
"""
import argparse, json, os, time
import numpy as np
import torch
from bs_common import BSCore, BSFront, BSLayer, BSHead, load_bs, CHUNK, HOP, N_FFT


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--ckpt', required=True)
    ap.add_argument('--config', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--stem', required=True, help='parts.json の stem（ft1 は vocals、karaoke は lead）')
    ap.add_argument('--msst', default=None)
    a = ap.parse_args()

    m, cfg = load_bs(a.config, a.ckpt, a.msst)
    core = BSCore(m, per_head=True).eval()
    frames = CHUNK // HOP + 1   # 690
    spec = torch.from_numpy(np.random.default_rng(0).standard_normal((1, 2, N_FFT // 2 + 1, frames, 2)).astype(np.float32))
    Bd = torch.export.Dim('batch', min=1, max=16)
    Td = torch.export.Dim('frames', min=16, max=4096)
    os.makedirs(a.out, exist_ok=True)
    t = time.time()
    with torch.no_grad():
        x = core.front(spec)
        torch.onnx.export(BSFront(core).eval(), (spec,), dynamo=True, opset_version=18, input_names=['spec'], output_names=['x'],
                          dynamic_shapes={'spec': {0: Bd, 3: Td}}, external_data=False).save(os.path.join(a.out, 'front.onnx'))
        for i in range(len(m.layers)):
            torch.onnx.export(BSLayer(core, i).eval(), (x,), dynamo=True, opset_version=18, input_names=['x'], output_names=['y'],
                              dynamic_shapes={'x': {0: Bd, 1: Td}}, external_data=False).save(os.path.join(a.out, f'layer{i}.onnx'))
            x = core.layer(i, x)
            print('layer', i, flush=True)
        torch.onnx.export(BSHead(core).eval(), (x, spec), dynamo=True, opset_version=18, input_names=['x', 'spec'], output_names=['est'],
                          dynamic_shapes={'x': {0: Bd, 1: Td}, 'spec': {0: Bd, 3: Td}}, external_data=False).save(os.path.join(a.out, 'head.onnx'))
    # アプリが読む設定（SeparatorMain.cpp の ModelConfig）
    meta = {'arch': 'bs-roformer', 'hop': HOP, 'layers': len(m.layers), 'chunk': CHUNK, 'stem': a.stem}
    with open(os.path.join(a.out, 'parts.json'), 'w', encoding='utf-8') as f:
        json.dump(meta, f)
        f.write('\n')
    print('done', round(time.time() - t, 1), 's', json.dumps(meta))


if __name__ == '__main__':
    main()
