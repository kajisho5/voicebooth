"""BS-RoFormer（anvuew、GPL-3.0）を VoiceBoothSeparator の分割 ONNX にするための共通部分（2026-10-02）。

- ckpt と yaml の読み込み（MSST の models/bs_roformer/bs_roformer.py、MIT を使う）
- 書き出し用に書き直した本体 BSCore（スペクトログラム → 目的の音のスペクトログラム。STFT / iSTFT は外＝C++ 側）
- front / layer i / head に分けたモジュール（アプリの分割ランナーと同じ並び）
- numpy の STFT / iSTFT（torch.stft の center=True・reflect・周期 Hann・正規化なしと同じ。hop を引数にとる）

Mel-Band RoFormer（Kim）の道具（common.py・export_parts.py）とは別。こちらは common.py を読まない。
"""
import os, sys
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import yaml

HERE = os.path.dirname(os.path.abspath(__file__))

N_FFT = 2048
HOP = 512                # BS-RoFormer の stft_hop_length（yaml の model.stft_hop_length）
CHUNK = 352800           # アプリのチャンク（8.0 秒 @ 44.1 kHz）。hop 512 で 690 フレーム
SR = 44100


def add_msst_path(msst=None):
    """MSST（ZFTurbo/Music-Source-Separation-Training、MIT）のクローンを import できるようにする"""
    p = os.path.abspath(msst or os.environ.get('VB_MSST') or os.path.join(HERE, 'msst'))
    if not os.path.exists(os.path.join(p, 'models', 'bs_roformer', 'bs_roformer.py')):
        raise SystemExit(f'MSST のクローンが見つからない：{p}（--msst か VB_MSST で指定）')
    if p not in sys.path:
        sys.path.insert(0, p)
    return p


# ---------------------------------------------------------------- 読み込み
class _Loader(yaml.SafeLoader):
    pass


# anvuew の yaml は freqs_per_bands などに !!python/tuple を使う（SafeLoader のまま tuple だけ通す）
_Loader.add_constructor('tag:yaml.org,2002:python/tuple', lambda l, n: tuple(l.construct_sequence(n)))


def load_yaml(path):
    with open(path, encoding='utf-8') as f:
        return yaml.load(f, Loader=_Loader)


def load_bs(cfg_path, ckpt_path, msst=None):
    """yaml と ckpt から MSST の BSRoformer を作る（strict。足りない・余る重みがあれば止める）"""
    add_msst_path(msst)
    from models.bs_roformer.bs_roformer import BSRoformer  # noqa: E402
    cfg = load_yaml(cfg_path)
    mc = dict(cfg['model'])
    mc['use_torch_checkpoint'] = False   # 学習用の設定。推論・書き出しでは使わない
    m = BSRoformer(**mc)
    sd = torch.load(ckpt_path, map_location='cpu', weights_only=True)
    if isinstance(sd, dict) and 'state_dict' in sd:
        sd = sd['state_dict']
    m.load_state_dict(sd, strict=True)
    m.eval()
    assert m.stft_kwargs['hop_length'] == HOP and m.stft_kwargs['n_fft'] == N_FFT, 'hop 512・n_fft 2048 以外は未対応'
    return m, cfg


# ---------------------------------------------------------------- numpy STFT（torch.stft center=True・reflect・周期 Hann・正規化なし）
def hann_periodic(n):
    return (0.5 - 0.5 * np.cos(2 * np.pi * np.arange(n) / n)).astype(np.float32)


WINDOW = hann_periodic(N_FFT)


def stft_np(x, hop=HOP):
    """x: (..., N) float32 -> (..., 1025, T, 2) float32 (re, im)"""
    pad = N_FFT // 2
    xp = np.pad(x, [(0, 0)] * (x.ndim - 1) + [(pad, pad)], mode='reflect')
    n_frames = 1 + (xp.shape[-1] - N_FFT) // hop
    idx = np.arange(N_FFT)[None, :] + hop * np.arange(n_frames)[:, None]
    frames = xp[..., idx] * WINDOW
    spec = np.fft.rfft(frames.astype(np.float64), axis=-1)
    spec = np.swapaxes(spec, -1, -2)
    return np.stack([spec.real, spec.imag], axis=-1).astype(np.float32)


def istft_np(spec, length, hop=HOP):
    """spec: (..., 1025, T, 2) -> (..., length)。torch.istft(center=True) と同じ"""
    z = spec[..., 0].astype(np.float64) + 1j * spec[..., 1]
    z = np.swapaxes(z, -1, -2)
    frames = np.fft.irfft(z, n=N_FFT, axis=-1) * WINDOW
    T = frames.shape[-2]
    total = N_FFT + hop * (T - 1)
    out = np.zeros(frames.shape[:-2] + (total,))
    wsum = np.zeros(total)
    w2 = WINDOW.astype(np.float64) ** 2
    for t in range(T):
        out[..., t * hop:t * hop + N_FFT] += frames[..., t, :]
        wsum[t * hop:t * hop + N_FFT] += w2
    pad = N_FFT // 2
    out = out[..., pad:pad + length]
    w = wsum[pad:pad + length]
    return (out / np.where(w > 1e-11, w, 1.0)).astype(np.float32)


def snr_db(ref, est):
    ref = np.asarray(ref, np.float64); est = np.asarray(est, np.float64)
    return float(10 * np.log10(np.sum(ref ** 2) / max(np.sum((ref - est) ** 2), 1e-30)))


# ---------------------------------------------------------------- 書き出し用の本体
def _rot_half(x):
    x = x.unflatten(-1, (-1, 2))
    x1, x2 = x[..., 0], x[..., 1]
    return torch.stack((-x2, x1), dim=-1).flatten(-2)


def _rmsnorm(n, x):
    return F.normalize(x, dim=-1) * (n.scale * n.gamma)


class BSCore(nn.Module):
    """MSST BSRoformer.forward から STFT / iSTFT を外し、ONNX に出しやすい形に書き直したもの。
    in : spec (B, 2, 1025, T, 2)  [バッチ, チャンネル (L,R), 周波数ビン, フレーム, (re, im)]
    out: est  (B, 2, 1025, T, 2)  = 複素マスク × spec（DC は 0）。hop 512 の STFT は外
    per_head：時間方向の attention を 1 ヘッドずつ回す（ピークメモリ 1/8。アプリの 8 秒チャンクでは 690 フレーム）"""

    def __init__(self, m, per_head=True):
        super().__init__()
        assert not m.skip_connection and all(len(b) == 2 for b in m.layers), 'linear / skip のバージョンは未対応'
        assert m.num_stems == 1 and m.stereo, 'num_stems 1・ステレオのモデルだけ'
        self.m, self.per_head = m, per_head

    def attn(self, a, x, per_head=False):
        x = _rmsnorm(a.norm, x)
        b, n, _ = x.shape
        h = a.heads
        qkv = a.to_qkv(x).reshape(b, n, 3, h, -1).permute(2, 0, 3, 1, 4)
        q, k, v = qkv[0], qkv[1], qkv[2]
        inv = a.rotary_embed.freqs
        pos = torch.arange(n, dtype=x.dtype, device=x.device)
        fr = (pos[:, None] * inv[None, :]).repeat_interleave(2, dim=-1)
        cos, sin = fr.cos(), fr.sin()
        q = q * cos + _rot_half(q) * sin
        k = k * cos + _rot_half(k) * sin
        if per_head:
            out = torch.cat([torch.matmul((torch.matmul(q[:, i:i + 1], k[:, i:i + 1].transpose(-1, -2)) * a.scale).softmax(dim=-1),
                                          v[:, i:i + 1]) for i in range(h)], dim=1)
        else:
            sim = torch.matmul(q, k.transpose(-1, -2)) * a.scale
            out = torch.matmul(sim.softmax(dim=-1), v)
        gates = a.to_gates(x).transpose(1, 2).unsqueeze(-1).sigmoid()
        out = (out * gates).transpose(1, 2).reshape(b, n, -1)
        return a.to_out(out)

    def transformer(self, tr, x, per_head=False):
        for a, ff in tr.layers:
            x = self.attn(a, x, per_head) + x
            x = ff(x) + x
        if isinstance(tr.norm, nn.Identity):
            return x
        return _rmsnorm(tr.norm, x)

    def front(self, spec):
        m = self.m
        B, S, Fq, T, C = spec.shape
        sr = spec.permute(0, 2, 1, 3, 4).reshape(B, Fq * S, T, 2)            # b (f s) t c
        x = sr.permute(0, 2, 1, 3).reshape(B * T, Fq * S * 2)                # (b t) ((f s) c)。rank 2 のまま
        x = torch.stack([net(p) for p, net in zip(x.split(m.band_split.dim_inputs, dim=1), m.band_split.to_features)], dim=1)
        return x.reshape(B, T, x.shape[1], x.shape[2])                       # b t bands d

    def layer(self, i, x):
        time_tr, freq_tr = self.m.layers[i]
        B, T, nb, d = x.shape
        x = x.transpose(1, 2).reshape(B * nb, T, d)
        x = self.transformer(time_tr, x, self.per_head)
        x = x.reshape(B, nb, T, d).transpose(1, 2).reshape(B * T, nb, d)
        x = self.transformer(freq_tr, x)
        return x.reshape(B, T, nb, d)

    def head(self, x, spec):
        m = self.m
        B, S, Fq, T, C = spec.shape
        sr = spec.permute(0, 2, 1, 3, 4).reshape(B, Fq * S, T, 2)
        nb, d = x.shape[2], x.shape[3]
        x = _rmsnorm(m.final_norm, x).reshape(B * T, nb, d)
        outs = []
        for i, mlp in enumerate(m.mask_estimators[0].to_freqs):
            h = mlp[0](x[:, i])                  # MLP -> (bt, 2*dim_in)
            a_, g = h.chunk(2, dim=1)            # GLU を書き下す（rank 2）
            outs.append(a_ * g.sigmoid())
        mask = torch.cat(outs, dim=1).reshape(B, T, Fq * S, 2).permute(0, 2, 1, 3)  # b (f s) t c
        sre, sim_ = sr[..., 0], sr[..., 1]
        mre, mim = mask[..., 0], mask[..., 1]
        est = torch.stack([sre * mre - sim_ * mim, sre * mim + sim_ * mre], dim=-1)
        est = est.reshape(B, Fq, S, T, 2).permute(0, 2, 1, 3, 4)
        if m.zero_dc:
            est = torch.cat([torch.zeros_like(est[:, :, :1]), est[:, :, 1:]], dim=2)
        return est

    def forward(self, spec):
        x = self.front(spec)
        for i in range(len(self.m.layers)):
            x = self.layer(i, x)
        return self.head(x, spec)


class BSFront(nn.Module):
    """front.onnx：spec -> x"""

    def __init__(self, core):
        super().__init__(); self.core = core

    def forward(self, spec):
        return self.core.front(spec)


class BSLayer(nn.Module):
    """layer{i}.onnx：x -> y"""

    def __init__(self, core, i):
        super().__init__(); self.core, self.i = core, i

    def forward(self, x):
        return self.core.layer(self.i, x)


class BSHead(nn.Module):
    """head.onnx：x, spec -> est"""

    def __init__(self, core):
        super().__init__(); self.core = core

    def forward(self, x, spec):
        return self.core.head(x, spec)
