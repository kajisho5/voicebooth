"""文字列を SVG のパスに変換する（フォントが無い環境でも同じ見た目にするため）"""
from pathlib import Path
import uharfbuzz as hb
from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen

FONTS = Path(__file__).resolve().parent.parent / "resources" / "fonts"
_cache = {}


# 同梱フォントに無い字（ハングル・簡体字）の代替。Noto Sans CJK（fonts-noto-cjk）の TTC 内の番号
NOTO_CJK = Path("/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc")
FALLBACKS = [(NOTO_CJK, 1), (NOTO_CJK, 2)]   # KR, SC


def _load(name, index=0):
    key = (str(name), index)
    if key not in _cache:
        path = name if isinstance(name, Path) else FONTS / name
        data = path.read_bytes()
        face = hb.Face(data, index)
        font = hb.Font(face)
        tt = TTFont(str(path), fontNumber=index) if path.suffix == ".ttc" else TTFont(str(path))
        _cache[key] = (font, tt, tt["head"].unitsPerEm)
    return _cache[key]


def _covers(name, index, ch):
    _, tt, _ = _load(name, index)
    return ord(ch) in tt.getBestCmap()


def _runs(text, font, index=0):
    """同梱フォントに無い字を代替フォントの区間に分ける"""
    runs = []
    for ch in text:
        src = (font, index)
        if not ch.isspace() and not _covers(font, index, ch):
            for fb in FALLBACKS:
                if fb[0].exists() and _covers(fb[0], fb[1], ch):
                    src = fb
                    break
        if runs and runs[-1][0] == src:
            runs[-1][1] += ch
        else:
            runs.append([src, ch])
    return runs


def _shape(text, size, src, tracking):
    hbfont, tt, upem = _load(*src)
    buf = hb.Buffer()
    buf.add_str(text)
    buf.guess_segment_properties()
    hb.shape(hbfont, buf, {"kern": True, "liga": True})
    scale = size / upem
    width = sum(p.x_advance for p in buf.glyph_positions) * scale + tracking * size * len(buf.glyph_infos)
    return buf, tt, scale, width


def text_path(text, size, x=0.0, baseline=0.0, font="IBMPlexSansJP-SemiBold.ttf", tracking=0.0, anchor="start", font_index=0):
    """戻り値: (SVG path の d, 幅 px)。tracking は em 比。font_index は TTC の中の番号（Noto Sans CJK：1=KR、2=SC、3=TC）"""
    shaped = [(_shape(t, size, src, tracking), src) for src, t in _runs(text, font, font_index)]
    total = sum(sh[3] for sh, _ in shaped) - tracking * size

    if anchor == "middle":
        x -= total / 2
    elif anchor == "end":
        x -= total

    commands = []
    cx = x
    for (buf, tt, scale, _), _src in shaped:
        glyphs = tt.getGlyphSet()
        order = tt.getGlyphOrder()
        pen = SVGPathPen(glyphs)
        for info, pos in zip(buf.glyph_infos, buf.glyph_positions):
            t = TransformPen(pen, (scale, 0, 0, -scale, cx + pos.x_offset * scale, baseline - pos.y_offset * scale))
            glyphs[order[info.codepoint]].draw(t)
            cx += pos.x_advance * scale + tracking * size
        commands.append(pen.getCommands())
    return " ".join(commands), total
