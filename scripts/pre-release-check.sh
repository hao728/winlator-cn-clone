#!/usr/bin/env bash
# ============================================================
# Winlator 共存版发布前静态自检脚本
# 检查项：APK 存在 / 大小 / 签名 / 包名 / native 库 / assets / native大小
# 致命错误 → exit 1 阻止发布
# 警告（工具不可用导致跳过）→ 不阻止发布，但标记为需人工验证（pre-release）
# 全部通过无警告 → 可自动设为 Latest 正式版
# ============================================================
set -euo pipefail

APK_PATH="${1:-}"
EXPECTED_PKG="${2:-org.winlator}"

if [ -z "$APK_PATH" ] || [ ! -f "$APK_PATH" ]; then
  echo "::error::APK 文件不存在: $APK_PATH"
  exit 1
fi

PASS=0
FAIL=0
WARNING=0

check() {
  local desc="$1" result="$2"
  if [ "$result" = "0" ]; then
    echo "✅ $desc"
    PASS=$((PASS+1))
  else
    echo "❌ $desc"
    echo "::error::$desc"
    FAIL=$((FAIL+1))
  fi
}

warn() {
  local desc="$1"
  echo "⚠️  $desc"
  echo "::warning::$desc"
  WARNING=$((WARNING+1))
}

echo "============================================================"
echo "APK 发布前自检: $APK_PATH"
echo "期望包名: $EXPECTED_PKG"
echo "============================================================"

# --- 1. APK 大小（应 > 100MB，含 assets）---
SIZE_BYTES=$(stat -c%s "$APK_PATH" 2>/dev/null || stat -f%z "$APK_PATH" 2>/dev/null)
SIZE_MB=$((SIZE_BYTES / 1024 / 1024))
echo "APK 大小: ${SIZE_MB}MB"
if [ "$SIZE_MB" -ge 100 ]; then
  check "APK 大小合理 (${SIZE_MB}MB >= 100MB，含 assets)" 0
else
  check "APK 大小过小 (${SIZE_MB}MB < 100MB，assets 可能丢失)" 1
fi

# --- 2. 找到 aapt 和 apksigner ---
AAPT=""
APKSIGNER=""
if [ -n "${ANDROID_HOME:-}" ]; then
  AAPT=$(find "$ANDROID_HOME/build-tools" -name "aapt" -type f 2>/dev/null | head -1)
  APKSIGNER=$(find "$ANDROID_HOME/build-tools" -name "apksigner" -type f 2>/dev/null | head -1)
fi
if [ -z "$AAPT" ]; then
  AAPT=$(which aapt 2>/dev/null || echo "")
fi
if [ -z "$APKSIGNER" ]; then
  APKSIGNER=$(which apksigner 2>/dev/null || echo "")
fi
echo "aapt: ${AAPT:-未找到}"
echo "apksigner: ${APKSIGNER:-未找到}"

# --- 3. 包名检查 ---
if [ -n "$AAPT" ]; then
  PKG=$("$AAPT" dump badging "$APK_PATH" 2>/dev/null | grep -oE "package: name='[^']+'" | head -1 | sed "s/package: name='//;s/'//")
  echo "APK 包名: ${PKG:-（未解析到）}"
  if [ "$PKG" = "$EXPECTED_PKG" ]; then
    check "包名正确 ($PKG)" 0
  else
    check "包名错误 (期望 $EXPECTED_PKG，实际 ${PKG:-空})" 1
  fi
else
  warn "未找到 aapt，跳过包名检查（建议人工验证包名）"
fi

# --- 4. 签名验证 ---
if [ -n "$APKSIGNER" ]; then
  if "$APKSIGNER" verify --print-certs "$APK_PATH" >/dev/null 2>&1; then
    check "APK 签名有效" 0
  else
    check "APK 签名无效" 1
  fi
else
  warn "未找到 apksigner，跳过签名检查（建议人工验证签名）"
fi

# --- 5. native 库检查（5 个自有动态渲染器必须存在）---
# 注意：libadrenotools 是静态库(.a)，链接进 vortekrenderer.so，不单独出现在 APK 中
REQUIRED_SO=(
  "lib/arm64-v8a/libwinlator.so"
  "lib/arm64-v8a/libgladiorenderer.so"
  "lib/arm64-v8a/libvortekrenderer.so"
  "lib/arm64-v8a/libvirglrenderer.so"
  "lib/arm64-v8a/libmidihandler.so"
)
MISSING_SO=()
for so in "${REQUIRED_SO[@]}"; do
  if ! unzip -l "$APK_PATH" 2>/dev/null | grep -q "$so"; then
    MISSING_SO+=("$so")
  fi
done
if [ ${#MISSING_SO[@]} -eq 0 ]; then
  check "5 个自有 native 渲染器齐全" 0
else
  check "缺少 native 库: ${MISSING_SO[*]}" 1
fi

# --- 6. assets 检查（rootfs 等核心文件必须存在）---
REQUIRED_ASSETS=(
  "assets/rootfs.tzst"
  "assets/container_pattern.tzst"
)
MISSING_ASSETS=()
for a in "${REQUIRED_ASSETS[@]}"; do
  if ! unzip -l "$APK_PATH" 2>/dev/null | grep -q "$a"; then
    MISSING_ASSETS+=("$a")
  fi
done
if [ ${#MISSING_ASSETS[@]} -eq 0 ]; then
  check "核心 assets 齐全 (rootfs.tzst 等)" 0
else
  check "缺少 assets: ${MISSING_ASSETS[*]}" 1
fi

# --- 6b. win-fg 帧生成引擎检查（assets/winfg/libwin_fg.so 必须存在且 > 1MB）---
WINFG_SO="assets/winfg/libwin_fg.so"
WINFG_MANIFEST="assets/winfg/VkLayer_win_framegen.json"
if unzip -l "$APK_PATH" 2>/dev/null | grep -q "$WINFG_SO"; then
  WINFG_SIZE=$(unzip -l "$APK_PATH" 2>/dev/null | grep "$WINFG_SO" | awk '{print $1}')
  WINFG_MB=$((WINFG_SIZE / 1024 / 1024))
  if [ "$WINFG_MB" -ge 1 ]; then
    check "win-fg 帧生成引擎已集成 (libwin_fg.so ${WINFG_MB}MB)" 0
  else
    check "win-fg libwin_fg.so 过小 (${WINFG_MB}MB < 1MB，下载可能失败)" 1
  fi
else
  check "win-fg 帧生成引擎缺失 ($WINFG_SO 不在 APK 中)" 1
fi
if unzip -l "$APK_PATH" 2>/dev/null | grep -q "$WINFG_MANIFEST"; then
  check "win-fg layer manifest 已集成" 0
else
  check "win-fg layer manifest 缺失" 1
fi

# --- 7. APK 内 lib 目录总大小（确认 native 库不是空壳）---
LIB_SIZE=$(unzip -l "$APK_PATH" 2>/dev/null | grep "lib/arm64-v8a/" | awk '{sum+=$1} END {print int(sum/1024/1024)}')
echo "native 库总大小: ${LIB_SIZE:-0}MB"
if [ "${LIB_SIZE:-0}" -ge 5 ]; then
  check "native 库大小合理 (${LIB_SIZE}MB >= 5MB)" 0
else
  check "native 库过小 (${LIB_SIZE:-0}MB < 5MB，可能未编译)" 1
fi

# --- 汇总 ---
echo ""
echo "============================================================"
echo "自检结果: $PASS 通过, $FAIL 失败, $WARNING 警告(跳过项)"
echo "============================================================"

if [ "$FAIL" -gt 0 ]; then
  echo "::error::发布前自检失败 $FAIL 项，阻止发布（详见上方 ❌ 条目）"
  exit 1
fi

# 输出是否需要人工验证到 GITHUB_ENV（供发布步骤判断 prerelease）
if [ "$WARNING" -gt 0 ]; then
  echo "SELF_CHECK_NEED_MANUAL=1" >> "${GITHUB_ENV:-/dev/null}"
  echo ""
  echo "⚠️  自检通过但有 $WARNING 项跳过，建议人工验证后设为 Latest"
  echo "   发布将标记为 pre-release（预发布版）"
else
  echo "SELF_CHECK_NEED_MANUAL=0" >> "${GITHUB_ENV:-/dev/null}"
  echo ""
  echo "✅ 全部自检通过且无跳过项，可自动设为 Latest 正式版"
fi
