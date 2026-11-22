#!/usr/bin/env bash
# make_font.sh —— 从系统字体生成应用专用中文字库（精确覆盖 UI 文案）。
#
# 用法：bash tools/make_font.sh [字体文件路径]
#   不传字体时默认用 Windows 黑体 /c/Windows/Fonts/simhei.ttf。
#
# 前置：
#   1. 先跑 python tools/check_font_coverage.py 生成 docs/font_chars.txt
#   2. 本机需 node/npx（用 npx 拉起固定版本 lv_font_conv）
#
# 产出：assets/fonts/app_font_16.c（16px, bpp4, lvgl 格式）
#   并在 docs/ASSETS.md 记录来源与授权（黑体为 Windows 自带，仅开发验证用；
#   产品化请换成有商用授权的字体，再重跑本脚本）。
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

font="${1:-/c/Windows/Fonts/simhei.ttf}"
chars_file="$repo_root/docs/font_chars.txt"
out_dir="$repo_root/assets/fonts"
out="$out_dir/app_font_16.c"

if [ ! -f "$font" ]; then
    echo "字体文件不存在: $font" >&2
    exit 1
fi
if [ ! -f "$chars_file" ]; then
    echo "字符清单不存在，请先运行: python tools/check_font_coverage.py" >&2
    exit 1
fi

mkdir -p "$out_dir"

# 把字符清单转成 --symbols 需要的连续字符串（换行去掉）
symbols="$(tr -d '\n\r' < "$chars_file")"

echo "使用字体: $font"
echo "字符数: ${#symbols}"

npx --yes lv_font_conv@1.5.3 \
    --font "$font" \
    --symbols "$symbols" \
    --size 16 --bpp 4 --format lvgl --no-compress \
    --lv-font-name app_font_16 --lv-include "lvgl.h" \
    --output "$out"

echo "已生成: $out"
echo "⚠ 产品化前请确认字体商用授权，并在 docs/ASSETS.md 登记。"
