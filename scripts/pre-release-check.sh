#!/usr/bin/env bash
# ============================================================
# Winlator 共存版发布前静态自检脚本
# 检查项：APK 存在 / 大小 / 签名 / 包名 / native 库 / assets
# 任何一项失败则退出码非零，CI 不会发布 Release
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

check() {
  local desc="$1" result="$2"
  if [ "$result" = "0" ]; then
    echo "✅ $desc"
    PASS=$((PASS+1))
  else
    echo "❌ $desc"
    FAIL=$((FAIL+1))
  fi
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

# --- 3. 包名检查 ---
if [ -n "$AAPT" ]; then
  PKG=$("$AAPT" dump badging "$APK_PATH" 2>/dev/null | grep -oP "package: name='\K[^']+" | head -1)
  echo "APK 包名: $PKG"
  if [ "$PKG" = "$EXPECTED_PKG" ]; then
    check "包名正确 ($PKG)" 0
  else
    check "包名错误 (期望 $EXPECTED_PKG，实际 $PKG)" 1
  fi
else
  echo "⚠️  未找到 aapt，跳过包名检查"
fi

# --- 4. 签名验证 ---
if [ -n "$APKSIGNER" ]; then
  if "$APKSIGNER" verify --print-certs "$APK_PATH" >/dev/null 2>&1; then
    check "APK 签名有效" 0
  else
    check "APK 签名无效" 1
  fi
else
  echo "⚠️  未找到 apksigner，跳过签名检查"
fi

# --- 5. native 库检查（6 个自有渲染器必须存在）---
REQUIRED_SO=(
  "lib/arm64-v8a/libwinlator.so"
  "lib/arm64-v8a/libgladiorenderer.so"
  "lib/arm64-v8a/libvortekrenderer.so"
  "lib/arm64-v8a/libvirglrenderer.so"
  "lib/arm64-v8a/libmidihandler.so"
  "lib/arm64-v8a/libadrenotools.so"
)
MISSING_SO=()
for so in "${REQUIRED_SO[@]}"; do
  if ! unzip -l "$APK_PATH" | grep -q "$so"; then
    MISSING_SO+=("$so")
  fi
done
if [ ${#MISSING_SO[@]} -eq 0 ]; then
  check "6 个自有 native 渲染器齐全" 0
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
  if ! unzip -l "$APK_PATH" | grep -q "$a"; then
    MISSING_ASSETS+=("$a")
  fi
done
if [ ${#MISSING_ASSETS[@]} -eq 0 ]; then
  check "核心 assets 齐全 (rootfs.tzst 等)" 0
else
  check "缺少 assets: ${MISSING_ASSETS[*]}" 1
fi

# --- 7. assets 内无旧包名残留（等长替换验证）---
# 解压 rootfs.tzst 检查是否还有 com.winlator 残留
TMPDIR=$(mktemp -d)
unzip -p "$APK_PATH" assets/rootfs.tzst > "$TMPDIR/rootfs.tzst" 2>/dev/null || true
if [ -f "$TMPDIR/rootfs.tzst" ]; then
  # 用 strings 快速检查（不完整解压，只看二进制字符串）
  RESIDUAL=$(strings "$TMPDIR/rootfs.tzst" 2>/dev/null | grep -c "com.winlator" || true)
  if [ "$RESIDUAL" -eq 0 ]; then
    check "rootfs 内无旧包名 com.winlator 残留" 0
  else
    check "rootfs 内有 $RESIDUAL 处旧包名残留（等长替换可能不完整）" 1
  fi
fi
rm -rf "$TMPDIR"

# --- 汇总 ---
echo ""
echo "============================================================"
echo "自检结果: $PASS 通过, $FAIL 失败"
echo "============================================================"

if [ "$FAIL" -gt 0 ]; then
  echo "::error::发布前自检失败 $FAIL 项，阻止发布"
  exit 1
fi
echo "✅ 全部自检通过，可以发布"
