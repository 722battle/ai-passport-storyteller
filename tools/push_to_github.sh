#!/usr/bin/env bash
# push_to_github.sh —— 把本工程推到你的 GitHub 仓库，用于触发云编译。
#
# 用法（两步）：
#   1. 在 GitHub 网页新建一个【空】仓库（不要勾选 README/.gitignore/license），
#      拿到地址，例如 https://github.com/<你的用户名>/ai-passport-storyteller.git
#   2. 编辑本文件顶部 REPO_URL，改成你的地址，然后运行：
#        bash tools/push_to_github.sh
#
# 脚本做了什么：设置远端 -> 推送 feature 分支 -> 打 tag 触发 Build firmware 工作流。
# 推送成功后，去 GitHub 仓库的 Actions 页看 "Build firmware" 运行结果；
# 也可手动点 Run workflow（分支选 feature/huawei-storyteller）。

set -euo pipefail

# ======== GitHub 仓库地址（已按你的仓库填好）========
REPO_URL="https://github.com/722battle/ai-passport-storyteller.git"
# ================================================

# 若设置了 GITHUB_TOKEN 环境变量，用令牌构造带鉴权的推送地址；
# 否则用原始地址，推送时由 Git 交互提示输入用户名 + 令牌。
if [[ -n "${GITHUB_TOKEN:-}" ]]; then
    PUSH_URL="https://${GITHUB_TOKEN}@github.com/722battle/ai-passport-storyteller.git"
else
    PUSH_URL="$REPO_URL"
fi

cd "$(dirname -- "${BASH_SOURCE[0]}")/.."

if [[ "$REPO_URL" == *"YOUR_USERNAME"* ]]; then
    echo "请先把本脚本顶部的 REPO_URL 改成你的 GitHub 仓库地址。" >&2
    exit 1
fi

BRANCH="$(git branch --show-current)"
echo "当前分支: $BRANCH"

# 若已存在 origin，先更新地址；否则新增（用带令牌的地址，令牌不落盘到仓库配置以外）
if git remote get-url origin >/dev/null 2>&1; then
    git remote set-url origin "$PUSH_URL"
else
    git remote add origin "$PUSH_URL"
fi

echo "推送分支到 GitHub..."
git push -u origin "$BRANCH"

# 打 tag 触发 workflow（tag push 会触发 Build firmware 的 release job）
TAG="storyteller-v1"
echo "打 tag $TAG 触发云编译..."
git tag -f "$TAG" >/dev/null 2>&1 || true
git push -f origin "$TAG"

# 安全：推送完成后把 origin 恢复为不含令牌的地址，避免令牌残留在 .git/config
git remote set-url origin "$REPO_URL"
unset GITHUB_TOKEN

echo ""
echo "已推送。下一步："
echo "  1. 打开 GitHub 仓库 -> Actions 页，等 'Build firmware' 跑完（绿色对勾）。"
echo "  2. 点进这次运行 -> Artifacts -> 下载 passport-storyteller-v1。"
echo "  3. 解压得到 FoloToy-AI-Passport-full.bin，上传到 AI Passport 官方平台。"
echo "  详细步骤见 docs/DEPLOY.zh_CN.md。"
