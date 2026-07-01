#!/usr/bin/env bash
# sync-github.sh — 把 gitea 仓库同步到 github
#
# 做两件事：
#   1. 剔除 Claude Code 相关文件（CLAUDE.md、docs/superpowers/）
#   2. 改写 commit message：抹掉 "Co-Authored-By: Claude ..." 行，
#      并把那个提到 CLAUDE.md 的 subject 改写成中性措辞
#   3. 把所有 commit 的 author/committer 统一成 mingw <wmwby@126.com>
#
# gitea 原仓库一行不改，它仍是唯一真相源；本脚本只在临时 clone 上操作。
#
# 用法：
#   bash scripts/sync-github.sh <github-remote-url>
#   GITHUB_REMOTE=<url> bash scripts/sync-github.sh
#
# 依赖：git、git-filter-repo（pip install git-filter-repo）、Python 3
# Windows 下用 Git Bash 运行。

set -euo pipefail

SRC="${SRC:-$(cd "$(dirname "$0")/.." && pwd)}"
BRANCH="${BRANCH:-main}"
AUTHOR_NAME="${AUTHOR_NAME:-mingw}"
AUTHOR_EMAIL="${AUTHOR_EMAIL:-wmwby@126.com}"

GITHUB_REMOTE="${1:-${GITHUB_REMOTE:-}}"
if [ -z "$GITHUB_REMOTE" ]; then
  echo "用法: bash scripts/sync-github.sh <github-remote-url>" >&2
  echo "  或: GITHUB_REMOTE=<url> bash scripts/sync-github.sh" >&2
  exit 2
fi

command -v git-filter-repo >/dev/null 2>&1 || {
  echo "缺少 git-filter-repo，请先安装: pip install git-filter-repo" >&2
  exit 1
}

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "==> 克隆 $SRC 到临时目录"
git clone --no-local "$SRC" "$TMP/pcix"
cd "$TMP/pcix"

echo "==> filter-repo: 删 claude 文件 + 改写 message + 统一作者"
export AUTHOR_NAME AUTHOR_EMAIL
git filter-repo \
  --invert-paths \
  --path CLAUDE.md \
  --path docs/superpowers \
  --name-callback 'return os.environ["AUTHOR_NAME"].encode()' \
  --email-callback 'return os.environ["AUTHOR_EMAIL"].encode()' \
  --message-callback '
import os, re
text = message.decode("utf-8", "replace")
# 删掉 "Co-Authored-By: Claude ..." 整行（含可能的前导空格和尾换行）
text = re.sub(r"(?mi)^[ \t]*Co-Authored-By: Claude[^\n]*\n?", "", text)
# 改写那个提到 CLAUDE.md 的 subject
text = text.replace(
    "docs: expand CLAUDE.md into full project guide for cross-machine continuity",
    "docs: expand project guide for cross-machine continuity",
)
# 兜底：万一别处还残留 CLAUDE.md 字样，换成中性词
text = text.replace("CLAUDE.md", "the project guide")
return text.rstrip("\n").encode("utf-8") + b"\n"
' \
  --force

echo "==> 推送到 $GITHUB_REMOTE ($BRANCH)"
git remote add github "$GITHUB_REMOTE"
git push --force github "$BRANCH"

echo "==> 完成。GitHub 上的历史已清洗（claude 文件已剔除、作者已统一、message 已改写）。"
