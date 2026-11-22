# -*- coding: utf-8 -*-
"""从应用源码提取所有中文/标点码点，生成字体覆盖检查清单。

用途（对应仓库"中文字体检查清单"）：
  1. 扫描 main/app/*.c 里出现的字符串字面量，收集非 ASCII 码点。
  2. 输出 docs/font_chars.txt（去重后的字符）与码点范围。
  3. 若提供了字体文件（--ttf），用 fontTools 检查字体是否覆盖这些码点。

运行：
  python tools/check_font_coverage.py
  python tools/check_font_coverage.py --ttf /c/Windows/Fonts/simhei.ttf

注意：LVGL 内置 CJK 子集（lv_font_source_han_sans_sc_16_cjk）的覆盖
无法用本脚本直接读取；真机必须按本脚本输出的字符清单逐字检查显示。
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
APP_DIR = REPO / "main" / "app"
OUT = REPO / "docs" / "font_chars.txt"

# 匹配 C 字符串字面量里的文字（简化：取所有引号内的内容）
STR_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def unescape(s: str) -> str:
    # 处理 \n \t \" \\ 等常见转义；不处理 \u（我们的文案直接写 UTF-8）
    return (s.replace(r"\n", "\n").replace(r"\t", "\t")
             .replace(r"\"", '"').replace(r"\\", "\\"))


def strip_comments(text: str) -> str:
    """去掉 // 行注释与 /* */ 块注释，避免把注释里的中文算进字体清单。
    简化实现：逐字符扫描，跳过字符串内部（字符串里的 // 不是注释）。"""
    out: list[str] = []
    i, n = 0, len(text)
    in_str = False
    while i < n:
        ch = text[i]
        if in_str:
            out.append(ch)
            if ch == "\\" and i + 1 < n:      # 转义字符，原样带上一个
                out.append(text[i + 1]); i += 2; continue
            if ch == '"':
                in_str = False
            i += 1
            continue
        if ch == '"':
            in_str = True; out.append(ch); i += 1; continue
        if ch == '/' and i + 1 < n and text[i + 1] == '/':
            while i < n and text[i] != "\n":
                i += 1
            continue
        if ch == '/' and i + 1 < n and text[i + 1] == '*':
            i += 2
            while i + 1 < n and not (text[i] == '*' and text[i + 1] == '/'):
                i += 1
            i += 2
            continue
        out.append(ch)
        i += 1
    return "".join(out)


def collect_chars() -> set[str]:
    chars: set[str] = set()
    for path in sorted(APP_DIR.glob("*.c")):
        text = strip_comments(path.read_text(encoding="utf-8"))
        for m in STR_RE.finditer(text):
            for ch in unescape(m.group(1)):
                if ord(ch) > 127 and not ch.isspace():
                    chars.add(ch)
    return chars


def check_ttf(ttf: Path, chars: set[str]) -> int:
    try:
        from fontTools.ttLib import TTFont
    except ImportError:
        print("未安装 fontTools，跳过 TTF 覆盖检查。可 pip install fonttools。")
        return 0
    font = TTFont(str(ttf))
    cmap = set()
    for table in font["cmap"].tables:
        cmap.update(table.cmap.keys())
    missing = [c for c in chars if ord(c) not in cmap]
    if missing:
        print(f"[WARN] 字体 {ttf.name} 缺少 {len(missing)} 个码点: {''.join(missing)}")
        return 1
    print(f"[OK] 字体 {ttf.name} 覆盖全部 {len(chars)} 个字符")
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ttf", type=Path, help="可选：用 fontTools 检查该 TTF 的覆盖")
    args = ap.parse_args()

    chars = collect_chars()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("".join(sorted(chars)), encoding="utf-8")
    ranges = sorted({ord(c) for c in chars})
    print(f"应用共用到 {len(chars)} 个非 ASCII 字符，已写入 {OUT}")
    print("码点范围: " + ", ".join(f"U+{r:04X}" for r in ranges[:40]) +
          (" ..." if len(ranges) > 40 else ""))

    if args.ttf:
        return check_ttf(args.ttf, chars)
    print("提示：真机必须按 docs/font_chars.txt 逐字检查显示（LVGL 内置子集无法离线读取）。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
