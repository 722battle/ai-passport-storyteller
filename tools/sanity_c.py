# -*- coding: utf-8 -*-
"""无编译器环境下的 C 源码静态自查：括号/注释配对 + 声明-定义粗查。

不能替代真正的编译，但能发现明显的括号失衡、未闭合注释、缺失实现等低级错误。
运行：python tools/sanity_c.py
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
APP = REPO / "main" / "app"
MAIN = REPO / "main"
FILES = sorted(APP.glob("*.c")) + [MAIN / "main.c"]


def strip_strings_comments(text: str) -> str:
    out: list[str] = []
    i, n = 0, len(text)
    state = "code"  # code | str | char | line_comment | block_comment
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        if state == "code":
            if c == '"':
                state = "str"; i += 1; continue
            if c == "'":
                state = "char"; i += 1; continue
            if c == "/" and nxt == "/":
                state = "line_comment"; i += 2; continue
            if c == "/" and nxt == "*":
                state = "block_comment"; i += 2; continue
            out.append(c); i += 1; continue
        if state == "str":
            if c == "\\":
                i += 2; continue
            if c == '"':
                state = "code"
            i += 1; continue
        if state == "char":
            if c == "\\":
                i += 2; continue
            if c == "'":
                state = "code"
            i += 1; continue
        if state == "line_comment":
            if c == "\n":
                state = "code"; out.append("\n")
            i += 1; continue
        if state == "block_comment":
            if c == "*" and nxt == "/":
                state = "code"; i += 2; continue
            if c == "\n":
                out.append("\n")
            i += 1; continue
    if state in ("str", "char", "block_comment"):
        raise ValueError(f"未闭合的字符串/字符/块注释 (state={state})")
    return "".join(out)


def check_balanced(text: str, path: Path) -> list[str]:
    errs: list[str] = []
    code = strip_strings_comments(text)
    pairs = {")": "(", "]": "[", "}": "{"}
    stack: list[tuple[str, int]] = []
    line = 1
    for ch in code:
        if ch == "\n":
            line += 1
        elif ch in "([{":
            stack.append((ch, line))
        elif ch in ")]}":
            if not stack or stack[-1][0] != pairs[ch]:
                errs.append(f"{path.name}:{line}: 括号不匹配 '{ch}'")
                break
            stack.pop()
    if stack:
        ch, ln = stack[-1]
        errs.append(f"{path.name}:{ln}: 未闭合括号 '{ch}'")
    return errs


def main() -> int:
    all_errs: list[str] = []
    for path in FILES:
        text = path.read_text(encoding="utf-8")
        try:
            all_errs += check_balanced(text, path)
        except ValueError as e:
            all_errs.append(f"{path.name}: {e}")

    # 粗查：头文件里声明的 story_* 函数在 .c 里是否有定义
    defs = ""
    for path in FILES:
        defs += path.read_text(encoding="utf-8")
    for h in sorted(APP.glob("*.h")):
        for m in re.finditer(r"\b(story_\w+)\s*\([^;]*\);", h.read_text(encoding="utf-8")):
            name = m.group(1)
            if not re.search(r"\b" + re.escape(name) + r"\s*\(", defs):
                all_errs.append(f"{h.name}: 声明了 {name}() 但无定义")

    if all_errs:
        for e in all_errs:
            print("ERROR:", e)
        return 1
    print(f"sanity_c.py: PASS ({len(FILES)} C files, braces/quotes balanced, decls defined)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
