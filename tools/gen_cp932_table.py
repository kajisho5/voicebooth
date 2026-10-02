#!/usr/bin/env python3
"""CP932（Windows の Shift_JIS）の 2 バイト文字 → Unicode の表を作る（DESIGN 7.5.3）

歌詞の txt を OS の変換に頼らず、Win / Mac / Linux で同じ結果に読むため。
Python の cp932 コーデック（Microsoft の対応表）から作る。

使い方: python3 tools/gen_cp932_table.py > app/song/Cp932Table.inc
"""

LEADS = list(range(0x81, 0xA0)) + list(range(0xE0, 0xFD))
TRAIL_FIRST, TRAIL_LAST = 0x40, 0xFC


def main():
    values = []
    for lead in LEADS:
        for trail in range(TRAIL_FIRST, TRAIL_LAST + 1):
            try:
                ch = bytes([lead, trail]).decode("cp932")
                values.append(ord(ch) if len(ch) == 1 else 0)
            except UnicodeDecodeError:
                values.append(0)

    print("// 生成物：tools/gen_cp932_table.py で作る。手で直さない")
    print("// CP932 の 2 バイト文字。先頭 0x81-0x9F / 0xE0-0xFC（60 個）× 後続 0x40-0xFC（189 個）。0 は割り当てなし")
    print(f"static constexpr int cp932TrailCount = {TRAIL_LAST - TRAIL_FIRST + 1};")
    print(f"static const unsigned short cp932Table[{len(values)}] = {{")
    for i in range(0, len(values), 16):
        print("    " + ", ".join(f"0x{v:04X}" for v in values[i:i + 16]) + ",")
    print("};")


if __name__ == "__main__":
    main()
