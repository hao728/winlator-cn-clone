#!/usr/bin/env bash
# ============================================================
# 应用共存配置：读取 coexist.properties，替换 build.gradle 占位符
# 在 CI 构建前运行
# ============================================================
set -euo pipefail

CONFIG_FILE="${1:-coexist.properties}"
BUILD_GRADLE="${2:-app/build.gradle}"

if [ ! -f "$CONFIG_FILE" ]; then
  echo "::error::配置文件不存在: $CONFIG_FILE"
  exit 1
fi

# 读取配置
APP_ID=$(grep -E '^\s*app\.id\s*=' "$CONFIG_FILE" | head -1 | cut -d= -f2- | tr -d ' ')
VERSION_OFFSET=$(grep -E '^\s*version\.code\.offset\s*=' "$CONFIG_FILE" | head -1 | cut -d= -f2- | tr -d ' ')
VERSION_OFFSET="${VERSION_OFFSET:-1000}"

echo "应用共存配置:"
echo "  app.id = $APP_ID"
echo "  version.code.offset = $VERSION_OFFSET"

# 校验包名长度（必须 12 字符）
if [ ${#APP_ID} -ne 12 ]; then
  echo "::error::包名 '$APP_ID' 长度为 ${#APP_ID}，必须为 12 字符（与 com.winlator 等长）"
  exit 1
fi

# 从上游 build.gradle 读取原始 versionCode/versionName
# 注意：我们的 build.gradle 用了占位符，需要先从 git 历史或上游获取原始值
# 这里直接从当前 build.gradle 提取（如果占位符还没被替换）
ORIG_VERSION_CODE=$(grep -E 'versionCode' "$BUILD_GRADLE" | head -1 | grep -oE '[0-9]+' || echo "32")
ORIG_VERSION_NAME=$(grep -E 'versionName' "$BUILD_GRADLE" | head -1 | grep -oE '"[^"]+"' | tr -d '"' || echo "11.2.cn.03")

# 如果是占位符，用默认值
if [ "$ORIG_VERSION_CODE" = "__VERSION_CODE__" ]; then
  ORIG_VERSION_CODE=32
fi
if [ "$ORIG_VERSION_NAME" = "__VERSION_NAME__" ]; then
  ORIG_VERSION_NAME="11.2.cn.03"
fi

NEW_VERSION_CODE=$((ORIG_VERSION_CODE + VERSION_OFFSET))

echo "  原始 versionCode = $ORIG_VERSION_CODE"
echo "  共存版 versionCode = $NEW_VERSION_CODE"
echo "  versionName = $ORIG_VERSION_NAME"

# 替换占位符
sed -i "s/__APP_ID__/$APP_ID/g" "$BUILD_GRADLE"
sed -i "s/__VERSION_CODE__/$NEW_VERSION_CODE/g" "$BUILD_GRADLE"
sed -i "s/__VERSION_NAME__/$ORIG_VERSION_NAME/g" "$BUILD_GRADLE"

echo ""
echo "替换后的 build.gradle 关键行:"
grep -E "applicationId|versionCode|versionName" "$BUILD_GRADLE"

# 输出到 GITHUB_ENV 供后续步骤使用
echo "APP_ID=$APP_ID" >> "${GITHUB_ENV:-/dev/null}"
echo "VERSION_CODE=$NEW_VERSION_CODE" >> "${GITHUB_ENV:-/dev/null}"
echo "VERSION_NAME=$ORIG_VERSION_NAME" >> "${GITHUB_ENV:-/dev/null}"

echo ""
echo "✅ 配置应用完成"
