"""Export Mel-Band RoFormer as separate ONNX parts: front, layer0..5, head (B16: keep ORT peak memory low without mem-reuse switch)."""
import sys, os, numpy as np, torch, torch.nn as nn
from common import *

class Front(nn.Module):
    def __init__(s, c): super().__init__(); s.c = c
    def forward(s, spec):
        m = s.c.m
        B, S, Fq, T, C = spec.shape
        sr = spec.permute(0, 2, 1, 3, 4).reshape(B, Fq * S, T, 2)
        x = torch.index_select(sr, 1, m.freq_indices).permute(0, 2, 1, 3).reshape(B * T, -1)
        x = torch.stack([net(p) for p, net in zip(x.split(m.band_split.dim_inputs, dim=1), m.band_split.to_features)], dim=1)
        nb, d = x.shape[1], x.shape[2]
        return x.reshape(B, T, nb, d)

class Layer(nn.Module):
    def __init__(s, c, i): super().__init__(); s.c = c; s.i = i
    def forward(s, x):
        c = s.c; time_tr, freq_tr = c.m.layers[s.i]
        B, T, nb, d = x.shape
        x = x.transpose(1, 2).reshape(B * nb, T, d)
        x = c.transformer(time_tr, x, c.per_head)
        x = x.reshape(B, nb, T, d).transpose(1, 2).reshape(B * T, nb, d)
        x = c.transformer(freq_tr, x)
        return x.reshape(B, T, nb, d)

class Head(nn.Module):
    def __init__(s, c): super().__init__(); s.c = c
    def forward(s, x, spec):
        c = s.c; m = c.m
        B, S, Fq, T, C = spec.shape
        sr = spec.permute(0, 2, 1, 3, 4).reshape(B, Fq * S, T, 2)
        nb, d = x.shape[2], x.shape[3]
        x = x.reshape(B * T, nb, d)
        outs = []
        for i, mlp in enumerate(m.mask_estimators[0].to_freqs):
            h = mlp[0](x[:, i]); a, g = h.chunk(2, dim=1); outs.append(a * g.sigmoid())
        mask = torch.cat(outs, dim=1).reshape(B, T, -1, 2).permute(0, 2, 1, 3)
        mask = torch.cat([mask, torch.zeros_like(mask[:, :1])], dim=1)
        mavg = (torch.index_select(mask, 1, c.g0) + torch.index_select(mask, 1, c.g1)) * c.w[None, :, None, None]
        sre, sim_ = sr[..., 0], sr[..., 1]; mre, mim = mavg[..., 0], mavg[..., 1]
        est = torch.stack([sre * mre - sim_ * mim, sre * mim + sim_ * mre], dim=-1)
        return est.reshape(B, Fq, S, T, 2).permute(0, 2, 1, 3, 4)

if __name__ == '__main__':
    os.makedirs('parts', exist_ok=True)
    core = MelBandCore(load_model()).eval()
    spec = torch.from_numpy(np.load('chunk_spec.npy'))
    Bd = torch.export.Dim('batch', min=1, max=16); Td = torch.export.Dim('frames', min=16, max=4096)
    with torch.no_grad():
        x = Front(core)(spec)
        torch.onnx.export(Front(core).eval(), (spec,), dynamo=True, opset_version=18, input_names=['spec'], output_names=['x'],
                          dynamic_shapes={'spec': {0: Bd, 3: Td}}, external_data=False).save('parts/front.onnx')
        for i in range(len(core.m.layers)):
            torch.onnx.export(Layer(core, i).eval(), (x,), dynamo=True, opset_version=18, input_names=['x'], output_names=['y'],
                              dynamic_shapes={'x': {0: Bd, 1: Td}}, external_data=False).save(f'parts/layer{i}.onnx')
            x = Layer(core, i)(x)
            print('layer', i, flush=True)
        torch.onnx.export(Head(core).eval(), (x, spec), dynamo=True, opset_version=18, input_names=['x', 'spec'], output_names=['est'],
                          dynamic_shapes={'x': {0: Bd, 1: Td}, 'spec': {0: Bd, 3: Td}}, external_data=False).save('parts/head.onnx')
        ref = core(spec).numpy(); np.save('ref_est.npy', ref)
    print('done')
