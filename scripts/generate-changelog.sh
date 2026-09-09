#!/usr/bin/env bash
# ============================================================
# Winlator 共存版更新日志生成脚本
# 对比上次 Release tag 与当前 commit，生成 markdown changelog
# ============================================================
set -euo pipefail

OUTPUT_FILE="${1:-RELEASE_NOTES.md}"
APP_ID="${2:-org.winlator}"

# 找到最近的 coexist- 前缀 tag
LAST_TAG=$(git tag --list 'coexist-*' --sort=-creatordate 2>/dev/null | head -1 || echo "")

echo "生成更新日志..."
echo "上次发布 tag: ${LAST_TAG:-无（首次发布）}"

{
  echo "## Winlator 共存版（${APP_ID}）"
  echo ""
  echo "- 包名：\`${APP_ID}\`（可与原版 com.winlator 共存）"
  echo "- 构建时间：$(date '+%Y-%m-%d %H:%M UTC')"
  echo "- Commit：\`$(git rev-parse --short HEAD)\`"
  echo ""

  if [ -n "$LAST_TAG" ]; then
    RANGE="${LAST_TAG}..HEAD"
    COMMIT_COUNT=$(git rev-list --count "$RANGE" 2>/dev/null || echo "0")
    echo "### 本次更新（${COMMIT_COUNT} 个提交）"
    echo ""

    # 上游提交（来自 hostei33/winlator-cn 的合并）
    UPSTREAM_COMMITS=$(git log "$RANGE" --oneline --grep="Merge remote-tracking branch" 2>/dev/null || true)
    if [ -n "$UPSTREAM_COMMITS" ]; then
      echo "**上游同步：**"
      echo "$UPSTREAM_COMMITS" | while read -r line; do
        echo "- $line"
      done
      echo ""
    fi

    # 我们的提交（排除上游合并）
    OUR_COMMITS=$(git log "$RANGE" --oneline --invert-grep --grep="Merge remote-tracking branch" 2>/dev/null || true)
    if [ -n "$OUR_COMMITS" ]; then
      echo "**共存版改动：**"
      echo "$OUR_COMMITS" | while read -r line; do
        echo "- $line"
      done
      echo ""
    fi

    if [ -z "$UPSTREAM_COMMITS" ] && [ -z "$OUR_COMMITS" ]; then
      echo "_无新提交_"
      echo ""
    fi
  else
    echo "### 首次发布"
    echo ""
    echo "- 基于上游 hostei33/winlator-cn 构建"
    echo "- 改包名共存，含完整 rootfs / box64 / 驱动"
    echo ""
  fi

  echo "### 安装说明"
  echo ""
  echo "1. 卸载旧版共存版（如有）"
  echo "2. 安装本 APK"
  echo "3. 首次启动会自动解压 rootfs（约需 1-2 分钟）"
  echo "4. 可与原版 com.winlator 共存，数据独立"
  echo ""
  echo "### 限制"
  echo ""
  echo "- 包名固定为 ${APP_ID}（与原版等长 12 字符，glibc 硬约束）"
  echo "- 与原版容器数据不互通"
  echo "- CI 仅验证编译通过，运行稳定性需真机测试"

} > "$OUTPUT_FILE"

echo "更新日志已写入: $OUTPUT_FILE"
echo "---"
cat "$OUTPUT_FILE"
