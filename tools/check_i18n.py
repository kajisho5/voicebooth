#!/usr/bin/env python3
"""VoiceBooth の多言語対応チェック（CI でも実行する）

1. resources/i18n/*.json のキーが全言語でそろっているか
2. 各キーの差し込み {0} {1} … が全言語で同じか / 空の訳が無いか
3. ソースで使っているキー（tr("key") と、キー表に載っている名前空間の文字列）が表にあるか
4. ソースに画面向けの非 ASCII 文字列の直書きが無いか（コメントは除く。データ用ファイルは除外）

使い方: python3 tools/check_i18n.py   （問題があれば終了コード 1）
"""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
I18N = ROOT / "resources" / "i18n"
SRC = ROOT / "app"

# 非 ASCII のデータ（歌詞・曲名などの仮データ）を持つことが許されるファイル
DATA_FILES = {
    "app/ui/DummySession.cpp",
    "app/song/LyricsImport.cpp",   # 区間の見出しの語（サビ・Aメロ…）を照合するためのデータ
    "app/audio/DeviceRules.cpp",   # 機器名の語（スピーカー・ヘッドホン…）を照合するためのデータ
}

PLACEHOLDER = re.compile(r"\{(\d+)\}")
KEY_LITERAL = re.compile(r'"([a-z][a-zA-Z0-9]*(?:\.[a-zA-Z0-9]+)+)"')
TR_CALL = re.compile(r'\btr\s*\(\s*"([^"]+)"')


def strip_comments(code: str) -> str:
    """// と /* */ を消す（文字列・文字リテラルの中は残す）"""
    out, i, n = [], 0, len(code)
    while i < n:
        c = code[i]
        if c in "\"'":
            q, j = c, i + 1
            while j < n and code[j] != q:
                j += 2 if code[j] == "\\" else 1
            out.append(code[i:j + 1]); i = j + 1
        elif code.startswith("//", i):
            j = code.find("\n", i); i = n if j < 0 else j
        elif code.startswith("/*", i):
            j = code.find("*/", i + 2); i = n if j < 0 else j + 2
        else:
            out.append(c); i += 1
    return "".join(out)


def string_literals(code: str):
    for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', code):
        yield m.start(), m.group(1)


def main() -> int:
    errors, warnings = [], []

    tables = {p.stem: json.loads(p.read_text(encoding="utf-8")) for p in sorted(I18N.glob("*.json"))}
    if "en" not in tables or "ja" not in tables:
        errors.append("ja.json と en.json が必要です")
        return report(errors, warnings)

    all_keys = set().union(*tables.values())
    for lang, t in tables.items():
        for k in sorted(all_keys - set(t)):
            errors.append(f"[{lang}] キーがありません: {k}")
        for k, v in t.items():
            if not isinstance(v, str) or not v.strip():
                errors.append(f"[{lang}] 訳が空です: {k}")

    for k in sorted(all_keys):
        sets = {lang: set(PLACEHOLDER.findall(t.get(k, ""))) for lang, t in tables.items()}
        if len({frozenset(s) for s in sets.values()}) > 1:
            errors.append(f"差し込みが言語で違います: {k} {sets}")

    namespaces = {k.split(".")[0] for k in all_keys}
    used = set()

    for path in sorted(SRC.rglob("*")):
        if path.suffix not in (".cpp", ".h"):
            continue
        rel = path.relative_to(ROOT).as_posix()
        code = strip_comments(path.read_text(encoding="utf-8"))

        for m in TR_CALL.finditer(code):
            used.add(m.group(1))
        for m in KEY_LITERAL.finditer(code):
            if m.group(1).split(".")[0] in namespaces:
                used.add(m.group(1))

        if rel in DATA_FILES or rel.startswith("app/i18n/"):
            continue
        for pos, lit in string_literals(code):
            if any(ord(ch) > 127 for ch in lit):
                line = code.count("\n", 0, pos) + 1
                errors.append(f"{rel}:{line}: 表示文字列の直書き（tr() を使う）: \"{lit[:40]}\"")

    for k in sorted(used - all_keys):
        errors.append(f"表にないキーを使っています: {k}")
    for k in sorted(all_keys - used):
        warnings.append(f"使われていないキー: {k}")

    return report(errors, warnings, len(all_keys), list(tables))


def report(errors, warnings, nkeys=0, langs=()):
    for w in warnings:
        print("warning:", w)
    for e in errors:
        print("error:", e)
    if errors:
        print(f"NG  {len(errors)} errors")
        return 1
    print(f"OK  {nkeys} keys x {len(langs)} languages ({', '.join(langs)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
