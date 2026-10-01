#!/usr/bin/env python3
"""edit list の無い m4a に iTunes 形式の iTunSMPB（頭と尻の詰め物・本当の長さ）を書き足す。

iTunes で作った m4a と同じ形（moov/udta/meta/ilst/---- の mean / name / data）にして、
Mp4Gapless の iTunSMPB 側の経路を、実際のデコーダ（Windows の Media Foundation / Mac の Core Audio）で確かめる。

使い方: add_itunsmpb.py <in.m4a（-use_editlist 0 で作ったもの）> <out.m4a> <priming> <valid_samples>
moov がファイルの最後にある（mdat が前）ことを前提にする。moov の中身を伸ばしても mdat の位置は変わらない。
"""
import struct
import sys


def box(kind, body):
    return struct.pack(">I", len(body) + 8) + kind + body


def children(data):
    out, pos = [], 0
    while pos + 8 <= len(data):
        size, kind = struct.unpack(">I4s", data[pos:pos + 8])
        if size < 8:
            break
        out.append((kind, data[pos + 8:pos + size]))
        pos += size
    return out


def main():
    src, dst, priming, valid = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    data = open(src, "rb").read()
    top = children(data)
    kinds = [k for k, _ in top]
    assert b"moov" in kinds and kinds.index(b"mdat") < kinds.index(b"moov"), "moov must be after mdat"

    # mdhd の長さ（全フレーム）から尻の詰め物を出す
    moov = dict(children(dict(top)[b"moov"]))
    trak = dict(children(moov[b"trak"]))
    assert b"edts" not in trak, "make the input with -use_editlist 0"
    mdhd = dict(children(trak[b"mdia"]))[b"mdhd"]
    total = struct.unpack(">I", mdhd[16:20])[0]
    padding = total - priming - valid
    assert padding >= 0, (total, priming, valid)

    text = " %08X %08X %08X %016X" % (0, priming, padding, valid) + " 00000000" * 8
    item = box(b"----",
               box(b"mean", b"\0\0\0\0" + b"com.apple.iTunes")
               + box(b"name", b"\0\0\0\0" + b"iTunSMPB")
               + box(b"data", struct.pack(">II", 1, 0) + text.encode("ascii")))
    meta = box(b"meta", b"\0\0\0\0" + box(b"hdlr", b"\0" * 8 + b"mdir" + b"appl" + b"\0" * 9) + box(b"ilst", item))
    udta = box(b"udta", meta)

    # 既存の udta（ffmpeg の ©too など）は捨てて差し替える
    moov_body = b"".join(box(k, v) for k, v in children(dict(top)[b"moov"]) if k != b"udta") + udta
    out = b"".join(box(k, v) if k != b"moov" else box(b"moov", moov_body) for k, v in top)
    open(dst, "wb").write(out)
    print("%s: priming %d, padding %d, valid %d (total %d)" % (dst, priming, padding, valid, total))


if __name__ == "__main__":
    main()
