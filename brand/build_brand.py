#!/usr/bin/env python3
"""VoiceBooth ブランド素材の生成（SVG を原本として PNG / ICO / ICNS を作る）

必要: rsvg-convert (librsvg2-bin), png2icns (icnsutils), ImageMagick, fontTools,
      fonts-noto-cjk（韓国語・中国語の README ヘッダー）, fonts-ibm-plex（ベトナム語・トルコ語の README ヘッダー）
使い方: python3 brand/build_brand.py
"""
import subprocess, shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC, OUT = ROOT / "src", ROOT / "out"

# --- 色（DESIGN 4.9） ---------------------------------------------------------
C = dict(
    bg0="#141311", deep="#0D0C0B", panel="#1B1A17", raised="#262420", raisedHi="#302D28",
    line="#34312B", text="#F2EDE3", textDim="#A9A295", textMute="#6F6A60",
    signal="#C6EE6A", ref="#8CC1EE", rec="#FF3B30",
)


# --- ブースマーク（窓＋カプセル＋タリー） ----------------------------------------
def mark(x, y, s, *, ink=C["text"], led=C["signal"], cut="url(#tile)", detail=True, stroke=None, glow=True):
    """(x, y) を左上、s を一辺とする正方形にマークを描く SVG 断片"""
    sw = stroke if stroke is not None else s * 0.085
    inset = sw / 2 + s * 0.02
    wx, wy, ww = x + inset, y + inset, s - inset * 2
    rx = s * 0.22
    parts = []

    if detail:
        # 窓の中（ブースのガラス）
        parts.append(f'<rect x="{wx:.2f}" y="{wy:.2f}" width="{ww:.2f}" height="{ww:.2f}" rx="{rx:.2f}" fill="url(#glass)"/>')

    parts.append(f'<rect x="{wx:.2f}" y="{wy:.2f}" width="{ww:.2f}" height="{ww:.2f}" rx="{rx:.2f}" '
                 f'fill="none" stroke="{ink}" stroke-width="{sw:.2f}"/>')

    # マイクのカプセル
    cw, ch = s * 0.30, s * 0.44
    cx, cy = x + s / 2 - cw / 2, y + s * 0.25
    capFill = "url(#capsule)" if detail else ink
    parts.append(f'<rect x="{cx:.2f}" y="{cy:.2f}" width="{cw:.2f}" height="{ch:.2f}" rx="{cw / 2:.2f}" fill="{capFill}"/>')
    if detail:
        # グリル（上半分の細い溝）
        for i in range(4):
            gy = cy + ch * (0.16 + i * 0.1)
            parts.append(f'<line x1="{cx + cw * 0.22:.2f}" y1="{gy:.2f}" x2="{cx + cw * 0.78:.2f}" y2="{gy:.2f}" '
                         f'stroke="{C["textMute"]}" stroke-opacity="0.55" stroke-width="{s * 0.012:.2f}" stroke-linecap="round"/>')
    # 支柱
    parts.append(f'<rect x="{x + s * 0.465:.2f}" y="{cy + ch - s * 0.01:.2f}" width="{s * 0.07:.2f}" height="{s * 0.15:.2f}" fill="{ink}"/>')

    # タリー（右上の角）
    lx, ly = x + s - inset, y + inset
    if cut:
        parts.append(f'<circle cx="{lx:.2f}" cy="{ly:.2f}" r="{s * 0.19:.2f}" fill="{cut}"/>')
    if glow:
        parts.append(f'<circle cx="{lx:.2f}" cy="{ly:.2f}" r="{s * 0.30:.2f}" fill="url(#ledglow)"/>')
    parts.append(f'<circle cx="{lx:.2f}" cy="{ly:.2f}" r="{s * 0.115:.2f}" fill="{led}"/>')
    if detail:
        parts.append(f'<circle cx="{lx - s * 0.03:.2f}" cy="{ly - s * 0.035:.2f}" r="{s * 0.04:.2f}" fill="#ffffff" fill-opacity="0.6"/>')
    return "\n  ".join(parts)


def defs(tileY0, tileY1, ledColour=C["signal"]):
    return f'''<defs>
  <linearGradient id="tile" x1="0" y1="{tileY0}" x2="0" y2="{tileY1}" gradientUnits="userSpaceOnUse">
    <stop offset="0" stop-color="#36322C"/><stop offset="0.55" stop-color="#1E1C19"/><stop offset="1" stop-color="#121110"/>
  </linearGradient>
  <linearGradient id="glass" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#0B0A09"/><stop offset="1" stop-color="#191714"/>
  </linearGradient>
  <linearGradient id="capsule" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#D9D2C5"/><stop offset="0.45" stop-color="#F7F3EA"/><stop offset="1" stop-color="#C9C1B3"/>
  </linearGradient>
  <radialGradient id="ledglow">
    <stop offset="0" stop-color="{ledColour}" stop-opacity="0.55"/><stop offset="1" stop-color="{ledColour}" stop-opacity="0"/>
  </radialGradient>
  <filter id="grain" x="0" y="0" width="100%" height="100%">
    <feTurbulence type="fractalNoise" baseFrequency="0.9" numOctaves="2" seed="7"/>
    <feColorMatrix values="0 0 0 0 1  0 0 0 0 1  0 0 0 0 1  0 0 0 0.06 0"/>
    <feComposite in2="SourceGraphic" operator="in"/>
  </filter>
  <filter id="shadow" x="-20%" y="-20%" width="140%" height="150%">
    <feDropShadow dx="0" dy="12" stdDeviation="14" flood-color="#000" flood-opacity="0.45"/>
  </filter>
</defs>'''


# --- アプリアイコン -----------------------------------------------------------
def icon_mac(led=C["signal"]):
    """macOS（Big Sur 以降）: 1024 キャンバス、824 の角丸、影込み"""
    t0, t = 100, 824
    m = 500
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
{defs(t0, t0 + t, led)}
<rect x="{t0}" y="{t0}" width="{t}" height="{t}" rx="185" fill="#000" filter="url(#shadow)"/>
<rect x="{t0}" y="{t0}" width="{t}" height="{t}" rx="185" fill="url(#tile)"/>
<rect x="{t0}" y="{t0}" width="{t}" height="{t}" rx="185" fill="#000" filter="url(#grain)"/>
<rect x="{t0 + 1.5}" y="{t0 + 1.5}" width="{t - 3}" height="{t - 3}" rx="183.5" fill="none" stroke="#ffffff" stroke-opacity="0.09" stroke-width="3"/>
  {mark(512 - m / 2, 512 - m / 2 + 8, m, led=led)}
</svg>'''


def icon_win(size=256, led=C["signal"]):
    """Windows: フルブリードの角丸タイル（大きいサイズ用）"""
    s = size
    m = s * 0.62
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{s}" height="{s}" viewBox="0 0 {s} {s}">
{defs(0, s, led)}
<rect x="0" y="0" width="{s}" height="{s}" rx="{s * 0.2}" fill="url(#tile)"/>
<rect x="0" y="0" width="{s}" height="{s}" rx="{s * 0.2}" fill="#000" filter="url(#grain)"/>
<rect x="{s * 0.004}" y="{s * 0.004}" width="{s * 0.992}" height="{s * 0.992}" rx="{s * 0.198}" fill="none" stroke="#ffffff" stroke-opacity="0.09" stroke-width="{s * 0.008}"/>
  {mark(s / 2 - m / 2, s / 2 - m / 2 + s * 0.01, m, led=led)}
</svg>'''


def icon_small(px):
    """16 / 24 / 32 / 48 px 専用。線をピクセルに合わせ、細部と光を省く"""
    # ピクセル単位で手合わせした寸法（px ごと）
    spec = {
        # 線の中心を .0 / .5 に置き、太さと合わせて画素の境界にそろえる
        16: dict(r=3.5, win=(3, 3, 10, 10),     wr=2.5, sw=2, cap=(6, 5, 4, 5),    stem=(7, 10, 2, 1.5),  led=(12.5, 3.5, 2.2, 3.4)),
        24: dict(r=5,   win=(5, 5, 14, 14),     wr=4,   sw=2, cap=(10, 8, 4, 7),   stem=(11, 15, 2, 2),   led=(19, 5, 2.9, 4.6)),
        32: dict(r=6.5, win=(6.5, 6.5, 19, 19), wr=5.5, sw=3, cap=(13, 10, 6, 10), stem=(15, 20, 2, 3.5), led=(25.5, 6.5, 3.6, 5.8)),
        48: dict(r=9.5, win=(10, 10, 28, 28),   wr=8,   sw=4, cap=(20, 15, 8, 15), stem=(23, 30, 2, 5),   led=(38, 10, 5, 8.2)),
    }[px]
    wx, wy, ww, wh = spec["win"]
    cx, cy, cw, ch = spec["cap"]
    sx, sy, sww, sh = spec["stem"]
    lx, ly, lr, cutr = spec["led"]
    sw = spec["sw"]
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{px}" height="{px}" viewBox="0 0 {px} {px}" shape-rendering="geometricPrecision">
{defs(0, px)}
<rect x="0" y="0" width="{px}" height="{px}" rx="{spec["r"]}" fill="url(#tile)"/>
<rect x="{wx}" y="{wy}" width="{ww}" height="{wh}" rx="{spec["wr"]}" fill="none" stroke="{C["text"]}" stroke-width="{sw}"/>
<rect x="{cx}" y="{cy}" width="{cw}" height="{ch}" rx="{cw / 2}" fill="{C["text"]}"/>
<rect x="{sx}" y="{sy}" width="{sww}" height="{sh}" fill="{C["text"]}"/>
<circle cx="{lx}" cy="{ly}" r="{cutr}" fill="url(#tile)"/>
<circle cx="{lx}" cy="{ly}" r="{lr}" fill="{C["signal"]}"/>
</svg>'''


# --- 書き出し -----------------------------------------------------------------
def write(name, svg):
    p = SRC / name
    p.write_text(svg, encoding="utf-8")
    return p


def png(svg_path, out, w, h=None):
    out.parent.mkdir(parents=True, exist_ok=True)
    cmd = ["rsvg-convert", "-w", str(w)] + (["-h", str(h)] if h else []) + ["-o", str(out), str(svg_path)]
    subprocess.run(cmd, check=True)
    return out


def build_icons():
    mac = write("app-icon-mac.svg", icon_mac())
    win = write("app-icon-win.svg", icon_win(256))
    smalls = {px: write(f"app-icon-{px}.svg", icon_small(px)) for px in (16, 24, 32, 48)}

    icons = OUT / "icons"
    # macOS: 16〜1024（@1x / @2x）→ .icns
    for px in (16, 32, 64, 128, 256, 512, 1024):
        png(mac, icons / "mac" / f"icon_{px}.png", px)
    subprocess.run(["png2icns", str(icons / "VoiceBooth.icns")] +
                   [str(icons / "mac" / f"icon_{px}.png") for px in (16, 32, 128, 256, 512, 1024)],
                   check=True, stdout=subprocess.DEVNULL)

    # Windows: 小サイズは専用、64 以上はフルブリード → .ico
    for px, src in smalls.items():
        png(src, icons / "win" / f"icon_{px}.png", px)
    for px in (64, 128, 256):
        png(win, icons / "win" / f"icon_{px}.png", px)
    subprocess.run(["convert"] + [str(icons / "win" / f"icon_{px}.png") for px in (16, 24, 32, 48, 64, 128, 256)] +
                   [str(icons / "VoiceBooth.ico")], check=True)

    # JUCE 用（CMake の ICON_BIG / ICON_SMALL）
    shutil.copy(icons / "mac" / "icon_1024.png", icons / "juce_icon_big_mac.png")
    png(win, icons / "juce_icon_big_win.png", 1024)
    shutil.copy(icons / "win" / "icon_32.png", icons / "juce_icon_small.png")


def build_social_app():
    """SNS 用の共有画像（アプリの画面入り）1280x640。画面は UI_MOCK のダミー（brand/src/app-screen.png。曲・歌詞は作り物）"""
    out = OUT / "marketing"
    W, H = 1280, 640
    shot = SRC / "app-screen.png"
    sw, sh = 880, 532                       # 画面（1100x665 を縮める）
    sx, sy = 452, 64
    wm, _ = wordmark(64, 228, 66, C["text"])
    l1, _ = text_path("歌ってみた専用DAW", 30, 66, 292, "IBMPlexSansJP-Medium.ttf")
    l2, _ = text_path("見て直して、一本渡す。", 30, 66, 334, "IBMPlexSansJP-Medium.ttf")
    en, _ = text_path("A vocal DAW for song covers", 17, 67, 368, "IBMPlexSansJP-Regular.ttf")
    url, _ = text_path("github.com/kajisho5/voicebooth", 17, 67, 584, "IBMPlexMono-Regular.ttf")

    def chip(text, x, y, colour):
        d, w = text_path(text, 16, x + 30, y + 23, "IBMPlexSansJP-Medium.ttf")
        return (f'<rect x="{x}" y="{y}" width="{w + 44:.1f}" height="34" rx="6" fill="{C["raised"]}" stroke="{C["line"]}"/>'
                f'<circle cx="{x + 16}" cy="{y + 17}" r="4.5" fill="{colour}"/>'
                f'<path d="{d}" fill="{C["text"]}" fill-opacity="0.92"/>'), w + 44

    chips, cy = [], 410
    for text, colour in (("無料・オープンソース", C["signal"]), ("Windows / Mac", C["ref"]),
                         ("音程を色で判定", C["signal"]), ("納品パックを一発で", C["rec"])):
        chips.append((text, colour))
    parts, x, y = [], 64, cy
    for text, colour in chips:
        svg_chip, w = chip(text, x, y, colour)
        if x + w > 430:
            x, y = 64, y + 44
            svg_chip, w = chip(text, x, y, colour)
        parts.append(svg_chip)
        x += w + 10

    body = f'''<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>
<rect width="{W}" height="{H}" fill="#000" filter="url(#grain)"/>
{grid(W, H, 64)}
<rect x="{sx + 10}" y="{sy + 18}" width="{sw}" height="{sh}" rx="16" fill="#000" fill-opacity="0.55" filter="url(#soft)"/>
<g clip-path="url(#shotClip)">{img(shot, sx, sy, sw, sh)}</g>
<rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="14" fill="none" stroke="{C["line"]}" stroke-width="1.5"/>
<rect x="{sx - 2}" y="0" width="180" height="{H}" fill="url(#fadeShot)"/>
{mark(64, 64, 92)}
{wm}
<path d="{l1}" fill="{C["text"]}"/>
<path d="{l2}" fill="{C["text"]}"/>
<path d="{en}" fill="{C["textDim"]}"/>
{"".join(parts)}
<path d="{url}" fill="{C["textMute"]}"/>'''
    svg = svg_doc(W, H, body, (0, H)).replace("</defs>", f'''  <clipPath id="shotClip"><rect x="{sx}" y="{sy}" width="{sw}" height="{sh}" rx="14"/></clipPath>
  <filter id="soft" x="-10%" y="-10%" width="120%" height="120%"><feGaussianBlur stdDeviation="18"/></filter>
  <linearGradient id="fadeShot" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="{C["bg0"]}" stop-opacity="0.85"/><stop offset="1" stop-color="{C["bg0"]}" stop-opacity="0"/>
  </linearGradient>
</defs>''')
    png(write("social-preview-app.svg", svg), out / "social-preview-app.png", W)


if __name__ == "__main__":
    SRC.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    build_icons()
    print("OK", OUT)


# =============================================================================
# ロゴ・配布用画像
# =============================================================================
import sys
sys.path.insert(0, str(ROOT))
from text_paths import text_path  # noqa: E402

TAGLINE_JA = "歌ってみた専用DAW — 見て直して、一本渡す。"
TAGLINE_EN = "A vocal DAW for song covers. See it, fix it, hand over one take."
NOTO_CJK_REGULAR = Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc")


def svg_doc(w, h, body, tile=(0, 1)):
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}">
{defs(*tile)}
{body}
</svg>'''


def wordmark(x, baseline, size, fill, anchor="start"):
    d, w = text_path("VoiceBooth", size, x, baseline, "IBMPlexSansJP-SemiBold.ttf", tracking=-0.01, anchor=anchor)
    return f'<path d="{d}" fill="{fill}"/>', w


_mask_id = [0]


def flat_mark(x, y, s, ink, led, cut=None):
    """平面のマーク（ロゴ用：グラデーションや光なし）。
    タリーの周りは透過マスクで抜くので、どんな背景に置いても使える"""
    _mask_id[0] += 1
    mid = f"cut{_mask_id[0]}"
    sw = s * 0.085
    inset = sw / 2 + s * 0.02
    lx, ly = x + s - inset, y + inset
    body = mark(x, y, s, ink=ink, led=led, cut=None, detail=False, glow=False)
    # 最後の要素（LED）以外にマスクをかける
    shapes, led_shape = body.rsplit("\n  ", 1)
    return (f'<mask id="{mid}" maskUnits="userSpaceOnUse" x="{x - s}" y="{y - s}" width="{s * 3}" height="{s * 3}">'
            f'<rect x="{x - s}" y="{y - s}" width="{s * 3}" height="{s * 3}" fill="#fff"/>'
            f'<circle cx="{lx:.2f}" cy="{ly:.2f}" r="{s * 0.19:.2f}" fill="#000"/></mask>\n'
            f'<g mask="url(#{mid})">{shapes}</g>\n{led_shape}')


def build_logos():
    logos = OUT / "logo"
    variants = {
        # 名前: (文字色, LED, 切り欠きの色, 背景)
        "dark":  (C["text"], C["signal"], C["bg0"], C["bg0"]),
        "light": (C["bg0"], "#7FB51F", "#FFFFFF", "#FFFFFF"),
        "mono-white": ("#FFFFFF", "#FFFFFF", C["bg0"], C["bg0"]),
        "mono-black": ("#000000", "#000000", "#FFFFFF", "#FFFFFF"),
    }

    for name, (ink, led, cut, bg) in variants.items():
        # マーク単体（透明背景。切り欠きは背景色で塗るので、置く背景に合わせて選ぶ）
        s = 512
        p = write(f"logo-mark-{name}.svg", svg_doc(s, s, flat_mark(s * 0.06, s * 0.08, s * 0.86, ink, led, cut), (0, s)))
        png(p, logos / f"logo-mark-{name}.png", 512)

        # 横組み：マーク + ワードマーク
        h = 160
        m = 120
        wm, ww = wordmark(m + 44 + 28, h / 2 + 30, 86, ink)
        W = int(28 + m + 44 + ww + 40)
        body = flat_mark(28, (h - m) / 2, m, ink, led, cut) + "\n" + wm
        p = write(f"logo-horizontal-{name}.svg", svg_doc(W, h, body, (0, h)))
        png(p, logos / f"logo-horizontal-{name}.png", W * 2)

        # 縦組み：マーク / ワードマーク / 肩書き
        W, H = 640, 520
        sub, _ = text_path("歌ってみた専用DAW", 30, W / 2, 470, "IBMPlexSansJP-Medium.ttf", tracking=0.08, anchor="middle")
        wm, _ = wordmark(W / 2, 408, 96, ink, anchor="middle")
        body = flat_mark(W / 2 - 120, 40, 240, ink, led, cut) + "\n" + wm + \
               f'\n<path d="{sub}" fill="{ink}" fill-opacity="0.62"/>'
        p = write(f"logo-stacked-{name}.svg", svg_doc(W, H, body, (0, H)))
        png(p, logos / f"logo-stacked-{name}.png", W * 2)


def pitch_motif(x0, x1, ymid, amp, seed=0):
    """ブランドの図柄：お手本の帯（アイスブルー）と自分の線（ライム）"""
    import math
    pts_ref, pts_me = [], []
    n = 180
    steps = [0, 2, 2, 4, 5, 5, 4, 2, 0, -1, 0, 2, 4, 7, 7, 5, 4, 2]   # 音の段（半音）
    for i in range(n + 1):
        t = i / n
        k = t * (len(steps) - 1)
        a, b = int(k), min(int(k) + 1, len(steps) - 1)
        f = k - a
        f = f * f * (3 - 2 * f) if f > 0.82 else 0.0   # 段の終わりで滑らかに移る
        semi = steps[a] + (steps[b] - steps[a]) * min(1.0, (f / 1.0))
        vib = 0.22 * math.sin(t * 90 + seed) if steps[a] in (5, 7) else 0.0
        x = x0 + (x1 - x0) * t
        y = ymid - (semi + vib) * amp
        pts_ref.append((x, y))
        dev = 0.18 * math.sin(t * 23 + 1.3) + 0.1 * math.sin(t * 61)
        pts_me.append((x, y - dev * amp))
    band_w = amp * 0.6
    top = " ".join(f"{x:.1f},{y - band_w / 2:.1f}" for x, y in pts_ref)
    bot = " ".join(f"{x:.1f},{y + band_w / 2:.1f}" for x, y in reversed(pts_ref))
    centre = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts_ref)
    me = " ".join(f"{x:.1f},{y:.1f}" for x, y in pts_me[: int(n * 0.62)])
    hx, hy = pts_me[int(n * 0.62)]
    return f'''<polygon points="{top} {bot}" fill="{C["ref"]}" fill-opacity="0.22"/>
<polyline points="{centre}" fill="none" stroke="{C["ref"]}" stroke-opacity="0.9" stroke-width="{amp * 0.09:.2f}" stroke-linejoin="round"/>
<polyline points="{me}" fill="none" stroke="{C["signal"]}" stroke-opacity="0.18" stroke-width="{amp * 0.55:.2f}" stroke-linejoin="round" stroke-linecap="round"/>
<polyline points="{me}" fill="none" stroke="{C["signal"]}" stroke-width="{amp * 0.2:.2f}" stroke-linejoin="round" stroke-linecap="round"/>
<line x1="{hx:.1f}" y1="{ymid - amp * 10:.1f}" x2="{hx:.1f}" y2="{ymid + amp * 4:.1f}" stroke="{C["signal"]}" stroke-width="{amp * 0.12:.2f}"/>
<circle cx="{hx:.1f}" cy="{hy:.1f}" r="{amp * 0.42:.2f}" fill="{C["deep"]}" stroke="{C["signal"]}" stroke-width="{amp * 0.18:.2f}"/>'''


def grid(w, h, step, colour=C["line"], opacity=0.45):
    lines = [f'<line x1="{x}" y1="0" x2="{x}" y2="{h}"/>' for x in range(0, w + 1, step)]
    lines += [f'<line x1="0" y1="{y}" x2="{w}" y2="{y}"/>' for y in range(0, h + 1, step // 2)]
    return f'<g stroke="{colour}" stroke-opacity="{opacity}" stroke-width="1">' + "".join(lines) + "</g>"


# README ヘッダーの各言語版（README.<lang>.md 用）。肩書きはアプリの翻訳表（app.tagline）と同じ文。
# 日本語は readme-banner.png。韓国語・中国語はその地域の字形にするため Noto Sans CJK の該当フェイスで組む。
# ベトナム語・トルコ語は Plex Sans JP に無い字（ă ơ ư đ と声調の合成字、ğ ş İ）があるので、
# 同じ IBM Plex の欧文版 IBM Plex Sans（fonts-ibm-plex）で組む（1 文の中で書体を混ぜない）
PLEX_SANS_MEDIUM = Path("/usr/share/fonts/truetype/ibm-plex/IBMPlexSans-Medium.ttf")
README_LANGS = {
    "en":      ("A vocal DAW for song covers. See it, fix it, hand over one take.", "IBMPlexSansJP-Medium.ttf", 0),
    "ko":      ("커버곡 녹음 전용 DAW — 보면서 고치고, 한 트랙으로 넘긴다.", NOTO_CJK_REGULAR, 1),
    "zh-Hans": ("翻唱专用 DAW —— 看着修正，交出一轨。", NOTO_CJK_REGULAR, 2),
    "zh-Hant": ("翻唱專用 DAW —— 看著修正，交出一軌。", NOTO_CJK_REGULAR, 3),
    "es":      ("Un DAW vocal para covers. Míralo, corrígelo y entrega una sola toma.", "IBMPlexSansJP-Medium.ttf", 0),
    "pt-BR":   ("Uma DAW vocal para covers. Veja, corrija e entregue um único take.", "IBMPlexSansJP-Medium.ttf", 0),
    "id":      ("DAW vokal untuk lagu cover. Lihat, perbaiki, serahkan satu take.", "IBMPlexSansJP-Medium.ttf", 0),
    "vi":      ("DAW thu giọng cho cover. Nhìn, sửa, giao một bản thu.", PLEX_SANS_MEDIUM, 0),
    "tr":      ("Cover şarkılar için vokal DAW'ı. Gör, düzelt, tek take teslim et.", PLEX_SANS_MEDIUM, 0),
    "de":      ("Eine Vocal-DAW für Coversongs. Sehen, korrigieren, einen Take abgeben.", "IBMPlexSansJP-Medium.ttf", 0),
    "fr":      ("Un DAW vocal pour les reprises. Voyez, corrigez, livrez une seule prise.", "IBMPlexSansJP-Medium.ttf", 0),
}


def readme_banner_svg(tagline, font, index):
    W, H = 1280, 320
    wm, _ = wordmark(272, 168, 76, C["text"])
    size = 24
    tg, tw = text_path(tagline, size, 276, 218, font, font_index=index)
    if 276 + tw > 1000:   # 長い文は少し小さく（右のピッチ線に重ねない）
        size = 21
        tg, tw = text_path(tagline, size, 276, 218, font, font_index=index)
    motif_x = max(840, 276 + tw + 56)
    body = f'''<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>
<rect width="{W}" height="{H}" fill="#000" filter="url(#grain)"/>
{grid(W, H, 48)}
<g opacity="1">{pitch_motif(motif_x, W + 20, 200, 8, seed=2)}</g>
{mark(90, 90, 140)}
{wm}
<path d="{tg}" fill="{C["textDim"]}"/>'''
    return svg_doc(W, H, body, (0, H)), W


def build_readme_banners():
    out = OUT / "marketing"
    for lang, (tagline, font, index) in README_LANGS.items():
        svg, W = readme_banner_svg(tagline, font, index)
        png(write(f"readme-banner-{lang}.svg", svg), out / f"readme-banner-{lang}.png", W)


# README 用の色見本（DESIGN 4.9 のトークン）。どの言語の README でも使えるよう、名前は英語
PALETTE = [
    ("Graphite", "#141311"), ("Panel", "#1B1A17"), ("Keycap", "#262420"), ("Warm white", "#F2EDE3"),
    ("Signal", "#C6EE6A"), ("Reference", "#8CC1EE"), ("Amber", "#F4B942"), ("Coral", "#FF6B5E"), ("Tally", "#FF3B30"),
]


def build_palette():
    W, H, pad, gap = 1280, 200, 40, 12
    n = len(PALETTE)
    sw = (W - pad * 2 - gap * (n - 1)) / n
    parts = [f'<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>']
    for i, (name, hexv) in enumerate(PALETTE):
        x = pad + i * (sw + gap)
        parts.append(f'<rect x="{x:.1f}" y="{pad}" width="{sw:.1f}" height="80" rx="4" fill="{hexv}" stroke="{C["line"]}" stroke-width="1"/>')
        if name in ("Signal", "Tally"):   # LED らしく、光る色には小さな光を
            parts.append(f'<circle cx="{x + sw - 14:.1f}" cy="{pad + 14}" r="4" fill="#ffffff" fill-opacity="0.55"/>')
        nm, _ = text_path(name, 15, x, pad + 108, "IBMPlexSansJP-Medium.ttf")
        hx, _ = text_path(hexv, 13, x, pad + 130, "IBMPlexMono-Regular.ttf")
        parts.append(f'<path d="{nm}" fill="{C["text"]}"/><path d="{hx}" fill="{C["textDim"]}"/>')
    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">' + "".join(parts) + "</svg>"
    png(write("palette.svg", svg), OUT / "marketing" / "palette.png", W)


def build_banners():
    out = OUT / "marketing"

    # GitHub / SNS 共有画像 1280x640
    W, H = 1280, 640
    wm, _ = wordmark(380, 300, 110, C["text"])
    ja, _ = text_path(TAGLINE_JA, 34, 384, 372, "IBMPlexSansJP-Medium.ttf")
    en, _ = text_path(TAGLINE_EN, 22, 386, 414, "IBMPlexSansJP-Regular.ttf")
    langs, _ = text_path("日本語 · English · 한국어 · 简体中文 · 繁體中文", 18, 386, 452, "IBMPlexSansJP-Regular.ttf")
    body = f'''<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>
<rect width="{W}" height="{H}" fill="#000" filter="url(#grain)"/>
{grid(W, H, 64)}
<g opacity="1">{pitch_motif(-20, W + 20, 556, 13)}</g>
<rect x="0" y="0" width="{W}" height="{H}" fill="url(#fadeL)"/>
{mark(110, 170, 220)}
{wm}
<path d="{ja}" fill="{C["text"]}" fill-opacity="0.9"/>
<path d="{en}" fill="{C["textDim"]}"/>
<path d="{langs}" fill="{C["textMute"]}"/>'''
    svg = svg_doc(W, H, body, (0, H)).replace("</defs>", f'''  <linearGradient id="fadeL" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="{C["bg0"]}" stop-opacity="0.75"/><stop offset="0.5" stop-color="{C["bg0"]}" stop-opacity="0.25"/><stop offset="1" stop-color="{C["bg0"]}" stop-opacity="0"/>
  </linearGradient>
</defs>''')
    png(write("social-preview.svg", svg), out / "social-preview.png", W)

    # README ヘッダー 1280x320
    W, H = 1280, 320
    wm, _ = wordmark(272, 168, 76, C["text"])
    ja, _ = text_path(TAGLINE_JA, 24, 276, 218, "IBMPlexSansJP-Medium.ttf")
    body = f'''<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>
<rect width="{W}" height="{H}" fill="#000" filter="url(#grain)"/>
{grid(W, H, 48)}
<g opacity="1">{pitch_motif(840, W + 20, 200, 8, seed=2)}</g>
{mark(90, 90, 140)}
{wm}
<path d="{ja}" fill="{C["textDim"]}"/>'''
    svg = svg_doc(W, H, body, (0, H)).replace("</defs>", f'''  <linearGradient id="fadeIn" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="{C["bg0"]}" stop-opacity="1"/><stop offset="1" stop-color="{C["bg0"]}" stop-opacity="0"/>
  </linearGradient>
</defs>''')
    png(write("readme-banner.svg", svg), out / "readme-banner.png", W)

    build_readme_banners()
    build_palette()

    # favicon（Web 用）
    p = write("favicon.svg", icon_small(32))
    png(p, out / "favicon-32.png", 32)
    png(write("favicon-180.svg", icon_win(180)), out / "apple-touch-icon-180.png", 180)


def build_installers():
    out = OUT / "installer"

    # macOS DMG 背景 660x400（@2x も）
    W, H = 660, 400
    # アイコンの置き場所（Finder 側で指定）: VoiceBooth.app (165, 200) / Applications (495, 200)。名前は Finder が描く
    hint, _ = text_path("Applications にドラッグしてインストール  ·  Drag to Applications to install", 13, W / 2, 360,
                        "IBMPlexSansJP-Regular.ttf", anchor="middle")
    body = f'''<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>
<rect width="{W}" height="{H}" fill="#000" filter="url(#grain)"/>
{grid(W, H, 40, opacity=0.3)}
<rect x="0" y="0" width="{W}" height="64" fill="{C["panel"]}"/>
<line x1="0" y1="64" x2="{W}" y2="64" stroke="{C["line"]}"/>
{flat_mark(24, 18, 28, C["text"], C["signal"], C["panel"])}
<path d="{text_path("VoiceBooth", 18, 64, 39, "IBMPlexSansJP-SemiBold.ttf")[0]}" fill="{C["text"]}"/>
<path d="M 260 200 L 392 200" stroke="{C["textMute"]}" stroke-width="3" stroke-dasharray="2 8" stroke-linecap="round"/>
<path d="M 384 188 L 400 200 L 384 212" fill="none" stroke="{C["signal"]}" stroke-width="3" stroke-linecap="round" stroke-linejoin="round"/>
<path d="{hint}" fill="{C["textMute"]}"/>'''
    p = write("dmg-background.svg", svg_doc(W, H, body, (0, H)))
    png(p, out / "dmg-background.png", W)
    png(p, out / "dmg-background@2x.png", W * 2)

    # Windows インストーラー（Inno Setup）: 左の縦長 164x314 と右上の 55x55（100% / 200%）
    W, H = 164, 314
    wm, _ = wordmark(W / 2, 214, 24, C["text"], anchor="middle")
    sub, _ = text_path("歌ってみた専用DAW", 11, W / 2, 238, "IBMPlexSansJP-Medium.ttf", anchor="middle")
    body = f'''<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>
<rect width="{W}" height="{H}" fill="#000" filter="url(#grain)"/>
{grid(W, H, 24, opacity=0.3)}
<g opacity="0.9">{pitch_motif(-10, W + 10, 292, 2.4, seed=4)}</g>
{mark(W / 2 - 46, 82, 92)}
{wm}
<path d="{sub}" fill="{C["textDim"]}"/>'''
    p = write("installer-wizard.svg", svg_doc(W, H, body, (0, H)))
    png(p, out / "installer-wizard.png", W)
    png(p, out / "installer-wizard@2x.png", W * 2)

    p = write("installer-small.svg", icon_win(55))
    png(p, out / "installer-small.png", 55)
    png(p, out / "installer-small@2x.png", 110)


if __name__ == "__main__":
    build_logos()
    build_banners()
    build_social_app()
    build_installers()
    print("OK all")


# =============================================================================
# 見え方のサンプル（プレビューシート）
# =============================================================================
def img(path, x, y, w, h=None):
    """PNG を埋め込む（rsvg は外部ファイル参照を読まないため data URI にする）"""
    import base64
    h = h if h is not None else w
    data = base64.b64encode(Path(path).read_bytes()).decode()
    return (f'<image href="data:image/png;base64,{data}" x="{x}" y="{y}" width="{w}" height="{h}" '
            f'preserveAspectRatio="xMidYMid meet"/>')


def label(text, x, y, size=15, fill=C["textDim"], font="IBMPlexSansJP-Medium.ttf", anchor="start"):
    d, _ = text_path(text, size, x, y, font, anchor=anchor)
    return f'<path d="{d}" fill="{fill}"/>'


def _png_from_string(svg, out, w):
    import tempfile
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False, encoding="utf-8") as f:
        f.write(svg)
        tmp = Path(f.name)
    try:
        png(tmp, out, w)
    finally:
        tmp.unlink()


def build_preview():
    out = OUT / "preview"
    out.mkdir(parents=True, exist_ok=True)
    I = (OUT / "icons").resolve()
    L = (OUT / "logo").resolve()
    M = (OUT / "marketing").resolve()
    N = (OUT / "installer").resolve()

    # --- 1. アイコンシート ---------------------------------------------------
    W, H = 1600, 1180
    parts = [f'<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>',
             label("VoiceBooth  App Icon", 60, 70, 28, C["text"], "IBMPlexSansJP-SemiBold.ttf"),
             label("アプリアイコン（macOS / Windows）", 60, 104, 16)]

    # 大きく
    parts.append(img(I / "mac" / "icon_1024.png", 40, 140, 460))
    parts.append(label("macOS 1024", 270, 625, 14, anchor="middle"))
    parts.append(img(I / "win" / "icon_256.png", 560, 230, 280))
    parts.append(label("Windows 256", 700, 545, 14, anchor="middle"))

    # サイズの段（暗い背景 / 明るい背景）
    sizes = [16, 24, 32, 48, 64, 128]
    for row, (bg, fg) in enumerate([(C["panel"], C["textDim"]), ("#ECE8E0", "#5E594F")]):
        y0 = 160 + row * 230
        parts.append(f'<rect x="900" y="{y0}" width="660" height="200" rx="12" fill="{bg}"/>')
        x = 930
        for px in sizes:
            src = I / "win" / f"icon_{px}.png"
            parts.append(img(src, x, y0 + 130 - px, px))
            parts.append(label(f"{px}", x + px / 2, y0 + 172, 12, fg, anchor="middle"))
            x += px + 44
    parts.append(label("小さいサイズは線を画素に合わせた専用版（16 / 24 / 32 / 48）", 900, 640, 14))

    # macOS Dock のモック
    y = 690
    parts.append(label("macOS Dock", 60, y, 16, C["text"], "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(f'<rect x="60" y="{y + 20}" width="720" height="130" rx="30" fill="#ffffff" fill-opacity="0.16"/>')
    for i, col in enumerate(["#3E7BF7", "#F2A93B", "#4CC07A"]):
        parts.append(f'<rect x="{90 + i * 120}" y="{y + 38}" width="96" height="96" rx="22" fill="{col}" fill-opacity="0.85"/>')
    parts.append(img(I / "mac" / "icon_1024.png", 440, y + 26, 120))
    parts.append(f'<circle cx="500" cy="{y + 146}" r="3" fill="#ffffff" fill-opacity="0.8"/>')
    parts.append(f'<rect x="590" y="{y + 38}" width="96" height="96" rx="22" fill="#9A6BF2" fill-opacity="0.85"/>')

    # Windows タスクバー / エクスプローラーのモック
    parts.append(label("Windows タスクバー / エクスプローラー", 840, y, 16, C["text"], "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(f'<rect x="840" y="{y + 20}" width="720" height="64" rx="8" fill="#1F1F1F"/>')
    for i in range(5):
        cx = 1040 + i * 56
        if i == 2:
            parts.append(f'<rect x="{cx - 22}" y="{y + 28}" width="44" height="44" rx="6" fill="#ffffff" fill-opacity="0.08"/>')
            parts.append(img(I / "win" / "icon_32.png", cx - 16, y + 34, 32))
            parts.append(f'<rect x="{cx - 8}" y="{y + 76}" width="16" height="3" rx="1.5" fill="#60CDFF"/>')
        else:
            parts.append(f'<rect x="{cx - 14}" y="{y + 38}" width="28" height="28" rx="6" fill="#ffffff" fill-opacity="0.18"/>')
    # エクスプローラーの行
    parts.append(f'<rect x="840" y="{y + 100}" width="720" height="150" rx="8" fill="#2B2B2B"/>')
    for i, (name, ic) in enumerate([("VoiceBooth.exe", I / "win" / "icon_16.png"), ("VoiceBooth.lnk", I / "win" / "icon_16.png")]):
        ry = y + 124 + i * 34
        parts.append(img(ic, 860, ry - 13, 16))
        parts.append(label(name, 888, ry, 14, "#E6E6E6", "IBMPlexSansJP-Regular.ttf"))
    parts.append(img(I / "win" / "icon_48.png", 1380, y + 120, 48))
    parts.append(img(I / "win" / "icon_32.png", 1450, y + 128, 32))
    parts.append(label("48 / 32 / 16 px", 1380, y + 200, 12, "#BDBDBD"))

    # Mac の Finder（明るい背景）
    y2 = 980
    parts.append(f'<rect x="60" y="{y2}" width="720" height="160" rx="12" fill="#F4F2EE"/>')
    for i, px in enumerate([128, 64, 32]):
        x = 100 + i * 200
        parts.append(img(I / "mac" / f"icon_{max(px * 2, 64)}.png", x, y2 + 140 - px - 6, px))
    parts.append(label("Finder（明るい背景）", 560, y2 + 132, 13, "#6F6A60"))

    parts.append(label("同じマーク：窓（ブース）＋マイクのカプセル＋タリーランプ", 840, y2 + 40, 16, C["text"]))
    parts.append(label("タリーはライム（待機・再生）。アプリ内では録音中に赤く灯る", 840, y2 + 72, 14))
    parts.append(label("TakyuPractice の波形アイコンとは別系統（DESIGN 4）", 840, y2 + 102, 14))

    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">{"".join(parts)}</svg>'
    _png_from_string(svg, out / "preview-icons.png", W)

    # --- 2. ロゴ・配布物シート ------------------------------------------------
    W, H = 1600, 1500
    parts = [f'<rect width="{W}" height="{H}" fill="{C["bg0"]}"/>',
             label("VoiceBooth  Brand Kit", 60, 70, 28, C["text"], "IBMPlexSansJP-SemiBold.ttf"),
             label("ロゴ / SNS・README 画像 / インストーラー", 60, 104, 16)]

    # ロゴ
    parts.append(f'<rect x="60" y="140" width="700" height="170" rx="12" fill="{C["bg0"]}" stroke="{C["line"]}"/>')
    parts.append(img(L / "logo-horizontal-dark.png", 90, 165, 640, 120))
    parts.append(f'<rect x="800" y="140" width="740" height="170" rx="12" fill="#FFFFFF"/>')
    parts.append(img(L / "logo-horizontal-light.png", 850, 165, 640, 120))
    parts.append(f'<rect x="60" y="330" width="340" height="300" rx="12" fill="{C["panel"]}"/>')
    parts.append(img(L / "logo-stacked-dark.png", 80, 345, 300, 260))
    parts.append(f'<rect x="420" y="330" width="340" height="300" rx="12" fill="#FFFFFF"/>')
    parts.append(img(L / "logo-stacked-light.png", 440, 345, 300, 260))
    parts.append(f'<rect x="800" y="330" width="180" height="140" rx="12" fill="#3A6B8F"/>')
    parts.append(img(L / "logo-mark-dark.png", 830, 340, 120))
    parts.append(f'<rect x="1000" y="330" width="180" height="140" rx="12" fill="#E7D9B8"/>')
    parts.append(img(L / "logo-mark-light.png", 1030, 340, 120))
    parts.append(f'<rect x="1200" y="330" width="160" height="140" rx="12" fill="{C["bg0"]}" stroke="{C["line"]}"/>')
    parts.append(img(L / "logo-mark-mono-white.png", 1220, 340, 120))
    parts.append(f'<rect x="1380" y="330" width="160" height="140" rx="12" fill="#FFFFFF"/>')
    parts.append(img(L / "logo-mark-mono-black.png", 1400, 340, 120))
    parts.append(label("どの背景色でも使えるよう、タリーの周りは透過で抜いてある", 800, 505, 14))
    parts.append(label("単色版は印刷・刻印・1色の場面用", 800, 532, 14))

    # SNS / README
    parts.append(label("SNS・GitHub 共有画像 1280x640", 60, 680, 16, C["text"], "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(img(M / "social-preview.png", 60, 700, 740, 370))
    parts.append(label("README ヘッダー 1280x320", 840, 680, 16, C["text"], "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(img(M / "readme-banner.png", 840, 700, 700, 175))
    parts.append(label("favicon / apple-touch-icon", 840, 925, 16, C["text"], "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(img(M / "favicon-32.png", 840, 945, 32))
    parts.append(img(M / "apple-touch-icon-180.png", 900, 945, 120))

    # インストーラー
    parts.append(label("macOS DMG 背景 660x400", 60, 1120, 16, C["text"], "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(img(N / "dmg-background@2x.png", 60, 1140, 560, 340))
    parts.append(img(I / "mac" / "icon_256.png", 60 + 165 * 560 / 660 - 54, 1140 + 200 * 340 / 400 - 54, 108))
    parts.append(f'<rect x="{60 + 495 * 560 / 660 - 46}" y="{1140 + 200 * 340 / 400 - 40}" width="92" height="80" rx="10" fill="#4A90E2"/>')
    parts.append(label("VoiceBooth", 60 + 165 * 560 / 660, 1140 + 200 * 340 / 400 + 70, 12, "#ffffff", anchor="middle"))
    parts.append(label("Applications", 60 + 495 * 560 / 660, 1140 + 200 * 340 / 400 + 70, 12, "#ffffff", anchor="middle"))
    parts.append(label("Windows インストーラー 164x314 / 55x55", 680, 1120, 16, C["text"], "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(f'<rect x="680" y="1140" width="860" height="340" rx="8" fill="#F0F0F0"/>')
    parts.append(img(N / "installer-wizard@2x.png", 680, 1140, 177, 340))
    parts.append(label("VoiceBooth セットアップへようこそ", 890, 1190, 20, "#1A1A1A", "IBMPlexSansJP-SemiBold.ttf"))
    parts.append(label("このウィザードは VoiceBooth をインストールします。", 890, 1226, 14, "#444444", "IBMPlexSansJP-Regular.ttf"))
    parts.append(img(N / "installer-small@2x.png", 1460, 1150, 55))
    parts.append(f'<rect x="1300" y="1430" width="100" height="32" rx="4" fill="#ffffff" stroke="#9A9A9A"/>')
    parts.append(f'<rect x="1420" y="1430" width="100" height="32" rx="4" fill="#0067C0"/>')
    parts.append(label("次へ", 1470, 1452, 13, "#ffffff", "IBMPlexSansJP-Medium.ttf", anchor="middle"))
    parts.append(label("戻る", 1350, 1452, 13, "#1A1A1A", "IBMPlexSansJP-Medium.ttf", anchor="middle"))

    svg = f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">{"".join(parts)}</svg>'
    _png_from_string(svg, out / "preview-brand.png", W)


if __name__ == "__main__":
    build_preview()
    print("OK preview")
