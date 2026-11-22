#!/usr/bin/env bash
# run_host_tests.sh —— 只编译纯 C 逻辑测试，不需要 ESP-IDF。
# 用法：bash tools/run_host_tests.sh
# 注意：需要本机有 cc/gcc/clang；Windows Git Bash 下若无 cc，请用
#       Python 镜像测试 tools/test_story_engine.py（逻辑等价）。
set -euo pipefail
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cc_bin="${CC:-cc}"

if ! command -v "$cc_bin" >/dev/null 2>&1; then
    echo "未找到 C 编译器（cc/gcc/clang）。请改用 Python 镜像测试："
    echo "    python tools/test_story_engine.py"
    exit 2
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

"$cc_bin" -std=c11 -Wall -Wextra -Werror \
    -I"$repo_root/main/app" \
    "$repo_root/tests/test_story_engine.c" \
    "$repo_root/main/app/story_data.c" \
    "$repo_root/main/app/story_engine.c" \
    -o "$tmp_dir/test_story_engine"
"$tmp_dir/test_story_engine"

echo "Host tests: PASS"
