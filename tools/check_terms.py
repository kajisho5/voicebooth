#!/usr/bin/env python3
"""日本語の言葉の決まりを確かめる（CLAUDE.md「言葉の決まり」）。

- 単独の「版」（「の版」「新しい版」「<版>」など）は使わない。「バージョン」と書く（持ち主の決まり）
  漢字・カタカナ・英字の後に付く言い方（ベータ版・スマホ版・欧文版・Windows 版 など）はよい
- 日本語を書くファイルだけを見る（中国語の README・翻訳表は「版本」が正しいので見ない）
- 使う人が読む日本語（アプリの文言・README・リリースノート）は、一般的なアプリの言葉づかいと
  「公用文作成の考え方」（文化審議会、2022。こと・とき・ところ・ものはひらがな）にそろえる（下の UI_RULES）

使い方：python3 tools/check_terms.py        （見つかれば一覧を出して 1 で終わる）
        python3 tools/check_terms.py --fix  （単独の「版」を「バージョン」に置き換える）
"""
import re
import subprocess
import sys

WORD = "版"

# 使う人が読む日本語だけに当てる決まり（見つけた言い方, 直し方）
USER_FACING = re.compile(r"^(?:resources/i18n/ja\.json|README\.md|packaging/release-notes\.md)$")
HIRA = "ぁ-ゖ"
UI_RULES = [
    (re.compile(rf"(?<=[{HIRA}])時(?![間刻代点計速差])"), "「〜時」は「〜とき」（例：開くとき・ないとき）"),
    (re.compile(rf"(?<=[{HIRA}])(?<!か)所(?!有|属|要|在)"), "「〜所」は「ところ」「部分」「箇所」（数える「か所」はよい）"),
    (re.compile(r"確かめ"), "「確かめる」は「確認する」"),
    (re.compile(r"裏で"), "「裏で」は「バックグラウンドで」"),
    (re.compile(r"無い|無け|無し"), "「無い」は「ない」"),
    (re.compile(r"受け取り中"), "「受け取り中」は「ダウンロード中」"),
    (re.compile(r"申告"), "「申告値」は「機器の報告値」"),
]

SKIP = re.compile(r"^(?:third_party/|CLAUDE\.md$|tools/check_terms\.py$|\.claude/reminder\.md$|"
                  r"README\.(?!md$)[^/]+\.md$|resources/i18n/(?!ja\.json$)[^/]+\.json$)")
TEXT = re.compile(r"\.(?:md|json|cpp|h|mm|py|yml|yaml|sh|ps1|iss|txt|cmake)$|(?:^|/)CMakeLists\.txt$")


def files():
    out = subprocess.check_output(["git", "ls-files"], text=True)
    return [f for f in out.split() if TEXT.search(f) and not SKIP.search(f)]


def compound(line, i):
    """「〜版」の形（前が漢字・カタカナ・英数字。英数字との間の空白 1 つはよい）"""
    j = i - 1
    if j >= 0 and line[j] == " ":
        return j >= 1 and line[j - 1].isascii() and line[j - 1].isalnum()
    if j < 0:
        return False
    c = line[j]
    return ("\u4e00" <= c <= "\u9fff") or ("\u30a0" <= c <= "\u30ff") or (c.isascii() and c.isalnum())


def bad_spans(line):
    return [i for i, ch in enumerate(line) if ch == WORD and not compound(line, i)]


def main():
    fix = "--fix" in sys.argv[1:]
    found = 0
    for path in files():
        try:
            with open(path, encoding="utf-8") as fh:
                lines = fh.read().split("\n")
        except (UnicodeDecodeError, FileNotFoundError):
            continue
        changed = False
        for n, line in enumerate(lines):
            if USER_FACING.search(path):
                for rule, hint in UI_RULES:
                    if rule.search(line):
                        found += 1
                        print(f"{path}:{n + 1}: {hint}: {line.strip()[:120]}")
            spans = bad_spans(line)
            if not spans:
                continue
            found += len(spans)
            if fix:
                for i in reversed(spans):
                    line = line[:i] + "バージョン" + line[i + 1:]
                lines[n] = line
                changed = True
            else:
                print(f"{path}:{n + 1}: 「{WORD}」ではなく「バージョン」と書く: {line.strip()[:120]}")
        if changed:
            with open(path, "w", encoding="utf-8") as fh:
                fh.write("\n".join(lines))
    if fix:
        print(f"replaced {found}")
        return 0
    if found:
        print(f"NG  {found} 件")
        return 1
    print("OK  言葉の決まり")
    return 0


if __name__ == "__main__":
    sys.exit(main())
