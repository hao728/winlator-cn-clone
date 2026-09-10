# Winlator 共存版调试指南

> 本文档面向需要排查 win-fg 帧生成、共存闪退、容器启动失败等问题的用户。

---

## 一、win-fg 帧生成调试

### 1.1 确认 layer 是否加载

开启容器详情 → 高级 → **调试模式**，启动容器后导出日志。

日志中应出现以下关键词（按出现顺序）：

```
# 1. 环境变量设置
WIN_FG_ENABLE=1
WIN_FG_PERF_PRESET=1
WIN_FG_MULTIPLIER=2

# 2. Vulkan loader 识别 layer
VK_LAYER_WIN_framegen

# 3. layer 初始化
winfg: layer initialized
winfg: optical flow engine ready

# 4. 运行时插帧
winfg: interpolated frame
```

**如果日志中没有 `VK_LAYER_WIN_framegen`**：
- 确认 `assets/winfg/VkLayer_win_framegen.json` 存在
- 确认 `library_path` 为 `/usr/lib/libwin_fg.so`（绝对路径）
- 确认 rootfs 中 `/usr/lib/libwin_fg.so` 已解压

### 1.2 确认 rootfs 中 layer 文件

容器启动后，在容器内执行（通过 Wine 命令行或 Termux）：

```bash
# 检查 layer 库
ls -la /usr/lib/libwin_fg.so

# 检查 layer manifest
cat /usr/share/vulkan/implicit_layer.d/VkLayer_win_framegen.json

# 检查环境变量
echo $WIN_FG_ENABLE
echo $WIN_FG_PERF_PRESET
echo $WIN_FG_MULTIPLIER
```

### 1.3 性能档位说明

| 档位 | PERF_PRESET | 光流精度 | 适用场景 |
|---|---|---|---|
| 质量 | 0 | 最精细 (flowFinest=1) | 慢节奏游戏、过场动画 |
| 平衡 | 1 | 中等 (flowFinest=2) | 大多数游戏（默认） |
| 性能 | 2 | 最粗 (flowFinest=3) | 快节奏动作游戏 |

### 1.4 倍率说明

| 倍率 | MULTIPLIER | 理论帧率 | 实际提升 |
|---|---|---|---|
| 2x | 2 | 原始 ×2 | ~25-50%（guest layer 丢帧） |
| 3x | 3 | 原始 ×3 | 开销大，不推荐 |
| 4x | 4 | 原始 ×4 | 开销极大，仅测试用 |

> **注意**：guest Vulkan layer 模式下，host compositor 可能丢弃部分生成帧。这是架构固有局限，非 bug。

---

## 二、共存版调试

### 2.1 卡加载无法进入容器

**最常见原因**：包名不等长导致 rootfs 内硬编码路径错误。

检查步骤：
1. 确认 `coexist.properties` 中 `app.id` 为 12 字符（含点）
   - `com.winlator` = 12 字符 ✓
   - `org.winlator` = 12 字符 ✓
   - `com.example.winlator` = 19 字符 ✗
2. 确认 `patch-assets.py` 已对所有 `.tzst` 文件执行等长替换
3. 检查 Manifest 中 FileProvider authority 使用 `${applicationId}`

### 2.2 闪退

抓取 logcat：

```bash
adb logcat -d | grep -i "winlator\|sigsegv\|sigabrt\|fatal\|art\|debug"
```

常见闪退原因：
- **图形驱动不兼容**：切换 Turnip ↔ Vortek
- **rootfs 损坏**：清除应用数据重新解压
- **内存不足**：关闭其他应用，降低分辨率

### 2.3 验证三层包名一致化

```bash
# 1. Manifest 层
aapt dump xmltree app.apk AndroidManifest.xml | grep -i "authority\|package"

# 2. Java 层（反编译后）
grep -r "com.winlator\|BuildConfig.APPLICATION_ID\|getPackageName" smali/

# 3. assets rootfs 层（解压后）
grep -r "com.winlator" rootfs/  # 应为 0 结果（已全部替换）
grep -r "org.winlator" rootfs/  # 应有结果
```

---

## 三、CI 构建调试

### 3.1 构建失败常见原因

| 错误 | 原因 | 解决 |
|---|---|---|
| `BuildConfig cannot find symbol` | Java 文件 import 了不存在的 BuildConfig 字段 | 检查 app/build.gradle 的 buildConfigField |
| `ContentsFragment duplicate class` | 文件覆盖错误 | 确认 ContentsFragment.java 未被误覆盖 |
| `libadrenotools.so missing` | 自检误报 | libadrenotools 是静态库 .a，不单独出现 |
| `rootfs.tzst corrupted` | patch-assets.py 直接在压缩流替换 | 必须解压→替换→重压缩 |
| `NDK not found` | NDK 版本不匹配 | 确认 24.0.8215888 |

### 3.2 查看构建日志

1. 进入 Actions → 点击失败的 run
2. 展开失败的 step
3. 搜索 `error:` / `FAILED` / `FAILURE`
4. 最后 80 行通常包含关键错误

### 3.3 自检项说明

CI 构建完成后执行 7 项静态自检：

1. APK 非空且 > 100MB
2. AndroidManifest 包名正确
3. assets 包含 rootfs.tzst
4. assets 包含 winfg/libwin_fg.so
5. Java 层无硬编码 `com.winlator`
6. rootfs 内无残留 `com.winlator` 路径
7. 签名有效

全部通过 → 正式版（Latest）；有跳过项 → pre-release。

---

## 四、性能调优建议

### 4.1 游戏内设置

- 分辨率：不超过 1280×720（win-fg 光流计算量与分辨率平方成正比）
- 垂直同步：开启（win-fg 需要稳定帧率输入）
- 帧率限制：设为屏幕刷新率 ÷ 倍率（如 60Hz ÷ 2x = 30fps）

### 4.2 容器设置

- 图形驱动：Turnip（Adreno）或 Vortek（兼容）
- DXVK_HUD：`fps,version`（可实时看帧率）
- 关闭不必要的后台容器

### 4.3 win-fg 调优

- 低帧率游戏（<30fps）：用 2x + 质量档
- 中帧率游戏（30-60fps）：用 2x + 平衡档
- 高帧率游戏（>60fps）：不建议开 win-fg（开销大于收益）

---

## 五、提交 Bug 报告

提交 Issue 时请附上：

1. **设备信息**：型号、GPU、Android 版本
2. **APK 版本**：Release tag 或 commit hash
3. **问题描述**：复现步骤、预期行为、实际行为
4. **日志**：`Download/winfg_log_*.txt` 或 logcat 输出
5. **截图**：如有界面异常

> 无日志的 Bug 报告可能无法定位问题。

