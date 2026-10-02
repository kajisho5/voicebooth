"""Shared helpers: model loading, ONNX-friendly core, numpy STFT/iSTFT, chunked demix."""
import os, sys, subprocess, time
import numpy as np
import torch
import torch.nn as nn
import torch.nn.functional as F
import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, 'msst'))
from models.bs_roformer.mel_band_roformer import MelBandRoformer  # noqa: E402

CFG_PATH = os.path.join(HERE, 'msst/configs/KimberleyJensen/config_vocals_mel_band_roformer_kj.yaml')
CKPT = os.path.join(HERE, 'ckpt/MelBandRoformer.ckpt')
SONG = os.environ.get('VB_SONG', '')   # 確かめに使う手元の曲（リポジトリには入れない）

N_FFT, HOP, WIN = 2048, 441, 2048
CHUNK = 352800          # config audio.chunk_size (8.0 s @ 44.1 kHz)
NUM_OVERLAP = 2         # config inference.num_overlap
T_FRAMES = CHUNK // HOP + 1  # 801


def load_cfg():
    return yaml.load(open(CFG_PATH), Loader=yaml.FullLoader)


def load_model():
    cfg = load_cfg()
    m = MelBandRoformer(**cfg['model'])
    sd = torch.load(CKPT, map_location='cpu', weights_only=True)
    m.load_state_dict(sd, strict=True)
    m.eval()
    return m


def load_excerpt(start=60.0, dur=30.0, sr=44100):
    cmd = ['ffmpeg', '-v', 'error', '-ss', str(start), '-t', str(dur), '-i', SONG,
           '-f', 'f32le', '-ac', '2', '-ar', str(sr), '-']
    raw = subprocess.run(cmd, check=True, capture_output=True).stdout
    return np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).T.copy()  # (2, N)


# ---------------------------------------------------------------- numpy STFT (matches torch.stft center=True, reflect, periodic hann)
def hann_periodic(n):
    return (0.5 - 0.5 * np.cos(2 * np.pi * np.arange(n) / n)).astype(np.float32)


WINDOW = hann_periodic(WIN)


def stft_np(x):
    """x: (..., N) float32 -> (..., 1025, T, 2) float32 (re, im)."""
    pad = N_FFT // 2
    xp = np.pad(x, [(0, 0)] * (x.ndim - 1) + [(pad, pad)], mode='reflect')
    n_frames = 1 + (xp.shape[-1] - N_FFT) // HOP
    idx = np.arange(N_FFT)[None, :] + HOP * np.arange(n_frames)[:, None]
    frames = xp[..., idx] * WINDOW  # (..., T, n_fft)
    spec = np.fft.rfft(frames.astype(np.float64), axis=-1)  # (..., T, 1025)
    spec = np.swapaxes(spec, -1, -2)
    return np.stack([spec.real, spec.imag], axis=-1).astype(np.float32)


def istft_np(spec, length=None):
    """spec: (..., 1025, T, 2) -> (..., N). Matches torch.istft(center=True)."""
    z = spec[..., 0].astype(np.float64) + 1j * spec[..., 1]
    z = np.swapaxes(z, -1, -2)  # (..., T, 1025)
    frames = np.fft.irfft(z, n=N_FFT, axis=-1) * WINDOW
    T = frames.shape[-2]
    total = N_FFT + HOP * (T - 1)
    out = np.zeros(frames.shape[:-2] + (total,))
    wsum = np.zeros(total)
    for t in range(T):
        out[..., t * HOP:t * HOP + N_FFT] += frames[..., t, :]
        wsum[t * HOP:t * HOP + N_FFT] += WINDOW.astype(np.float64) ** 2
    pad = N_FFT // 2
    if length is None:
        length = total - 2 * pad
    out = out[..., pad:pad + length]
    w = wsum[pad:pad + length]
    return (out / np.where(w > 1e-11, w, 1.0)).astype(np.float32)


# ---------------------------------------------------------------- export-friendly core
def _rot_half(x):
    x = x.unflatten(-1, (-1, 2))
    x1, x2 = x[..., 0], x[..., 1]
    return torch.stack((-x2, x1), dim=-1).flatten(-2)


def _rmsnorm(n, x):
    return F.normalize(x, dim=-1) * (n.scale * n.gamma)


class MelBandCore(nn.Module):
    """Spectrogram in -> separated (vocal) spectrogram out.

    in : spec (B, 2, 1025, T, 2)  [batch, channel(L,R), freq bin, frame, (re,im)]
    out: est  (B, 2, 1025, T, 2)  vocal estimate (mask applied, DC zeroed)
    STFT/iSTFT stay outside (C++ side).
    """

    def __init__(self, m: MelBandRoformer, per_head=True):
        super().__init__()
        self.m = m
        self.per_head = per_head
        fi = m.freq_indices  # (Ftot,) into the (f s) axis of length 2050
        nbf = m.num_bands_per_freq.repeat_interleave(2)  # per (f s) row
        Fs = nbf.shape[0]
        Ftot = fi.shape[0]
        # for each output row, up to 2 source positions in Ftot (pad -> Ftot = zero row)
        idx = torch.full((Fs, 2), Ftot, dtype=torch.long)
        cnt = torch.zeros(Fs, dtype=torch.long)
        for p, r in enumerate(fi.tolist()):
            idx[r, cnt[r]] = p
            cnt[r] += 1
        assert torch.equal(cnt, nbf)
        self.register_buffer('g0', idx[:, 0].clone(), persistent=False)
        self.register_buffer('g1', idx[:, 1].clone(), persistent=False)
        w = 1.0 / nbf.float().clamp(min=1e-8)
        if m.zero_dc:
            w[0:2] = 0.0  # zero DC bin of both channels (rows f=0,s=0/1)
        self.register_buffer('w', w, persistent=False)

    def attn(self, a, x, per_head=False):
        x = _rmsnorm(a.norm, x)
        b, n, _ = x.shape
        h = a.heads
        qkv = a.to_qkv(x).reshape(b, n, 3, h, -1).permute(2, 0, 3, 1, 4)
        q, k, v = qkv[0], qkv[1], qkv[2]
        inv = a.rotary_embed.freqs  # (32,)
        pos = torch.arange(n, dtype=x.dtype, device=x.device)
        fr = (pos[:, None] * inv[None, :]).repeat_interleave(2, dim=-1)  # (n, 64)
        cos, sin = fr.cos(), fr.sin()
        q = q * cos + _rot_half(q) * sin
        k = k * cos + _rot_half(k) * sin
        if per_head:  # long sequences (time axis): one head at a time -> 1/8 peak attention memory
            out = torch.cat([torch.matmul((torch.matmul(q[:, i:i + 1], k[:, i:i + 1].transpose(-1, -2)) * a.scale).softmax(dim=-1),
                                          v[:, i:i + 1]) for i in range(h)], dim=1)
        else:
            sim = torch.matmul(q, k.transpose(-1, -2)) * a.scale
            out = torch.matmul(sim.softmax(dim=-1), v)
        gates = a.to_gates(x).transpose(1, 2).unsqueeze(-1).sigmoid()  # b h n 1
        out = (out * gates).transpose(1, 2).reshape(b, n, -1)
        return a.to_out(out)

    def transformer(self, tr, x, per_head=False):
        for a, ff in tr.layers:
            x = self.attn(a, x, per_head) + x
            x = ff(x) + x
        return _rmsnorm(tr.norm, x)

    def forward(self, spec):
        m = self.m
        B, S, Fq, T, C = spec.shape
        sr = spec.permute(0, 2, 1, 3, 4).reshape(B, Fq * S, T, 2)  # b (f s) t c
        x = torch.index_select(sr, 1, m.freq_indices)  # b Ftot t c
        x = x.permute(0, 2, 1, 3).reshape(B * T, -1)  # (b t) (Ftot c)  -- keep rank 2 for export
        x = torch.stack([net(p) for p, net in zip(x.split(m.band_split.dim_inputs, dim=1), m.band_split.to_features)], dim=1)
        nb, d = x.shape[1], x.shape[2]
        x = x.reshape(B, T, nb, d)
        for time_tr, freq_tr in m.layers:
            x = x.transpose(1, 2).reshape(B * nb, T, d)
            x = self.transformer(time_tr, x, self.per_head)
            x = x.reshape(B, nb, T, d).transpose(1, 2).reshape(B * T, nb, d)
            x = self.transformer(freq_tr, x)
            x = x.reshape(B, T, nb, d)
        x = x.reshape(B * T, nb, d)
        outs = []
        for i, mlp in enumerate(m.mask_estimators[0].to_freqs):
            h = mlp[0](x[:, i])  # MLP -> (bt, 2*dim_in)
            a, g = h.chunk(2, dim=1)  # GLU written out (rank 2)
            outs.append(a * g.sigmoid())
        mask = torch.cat(outs, dim=1)  # (b t) (Ftot c)
        mask = mask.reshape(B, T, -1, 2).permute(0, 2, 1, 3)  # b Ftot t c
        mask = torch.cat([mask, torch.zeros_like(mask[:, :1])], dim=1)  # pad row
        mavg = (torch.index_select(mask, 1, self.g0) + torch.index_select(mask, 1, self.g1)) \
            * self.w[None, :, None, None]
        sre, sim_ = sr[..., 0], sr[..., 1]
        mre, mim = mavg[..., 0], mavg[..., 1]
        est = torch.stack([sre * mre - sim_ * mim, sre * mim + sim_ * mre], dim=-1)  # b (f s) t c
        return est.reshape(B, Fq, S, T, 2).permute(0, 2, 1, 3, 4)


# ---------------------------------------------------------------- chunked demix (same scheme as MSST utils.model_utils.demix)
def fade_window(size, fade):
    w = np.ones(size, dtype=np.float32)
    w[:fade] = np.linspace(0, 1, fade, dtype=np.float32)
    w[-fade:] = np.linspace(1, 0, fade, dtype=np.float32)
    return w


def demix(mix, run_chunks, batch_size=1, chunk=CHUNK, num_overlap=NUM_OVERLAP):
    """mix (2, N); run_chunks(np (b,2,chunk)) -> np (b,2,chunk)."""
    fade = chunk // 10
    step = chunk // num_overlap
    border = chunk - step
    L0 = mix.shape[-1]
    padded = L0 > 2 * border and border > 0
    if padded:
        mix = np.pad(mix, ((0, 0), (border, border)), mode='reflect')
    win0 = fade_window(chunk, fade)
    result = np.zeros_like(mix)
    counter = np.zeros(mix.shape[-1], dtype=np.float32)
    i, batch, locs = 0, [], []
    while i < mix.shape[1]:
        part = mix[:, i:i + chunk]
        L = part.shape[-1]
        if L < chunk:
            part = np.pad(part, ((0, 0), (0, chunk - L)), mode='reflect' if L > chunk // 2 else 'constant')
        batch.append(part); locs.append((i, L)); i += step
        if len(batch) >= batch_size or i >= mix.shape[1]:
            out = run_chunks(np.stack(batch))
            w = win0.copy()
            if i - step == 0: w[:fade] = 1
            elif i >= mix.shape[1]: w[-fade:] = 1
            for j, (s, L) in enumerate(locs):
                result[:, s:s + L] += out[j, :, :L] * w[:L]
                counter[s:s + L] += w[:L]
            batch.clear(); locs.clear()
    est = result / counter
    if padded:
        est = est[:, border:-border]
    return np.nan_to_num(est)


def make_spec_runner(core_fn):
    """core_fn: np spec (b,2,1025,T,2) -> np est spec. Returns chunk runner for demix."""
    def run(chunks):
        spec = stft_np(chunks)  # (b,2,1025,T,2)
        est = core_fn(spec)
        return istft_np(est, length=chunks.shape[-1])
    return run


def snr_db(ref, est):
    ref = np.asarray(ref, np.float64); est = np.asarray(est, np.float64)
    return 10 * np.log10(np.sum(ref ** 2) / max(np.sum((ref - est) ** 2), 1e-30))


def peak_rss_mb():
    import resource
    return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024.0
