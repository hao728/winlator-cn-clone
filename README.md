# Winlator 共存版

基于 [hostei33/winlator-cn](https://github.com/hostei33/winlator-cn)（Winlator 11.2 cn.03 glibc 版），改包名后可与原版 `com.winlator` 共存，且功能完整不缩水。

## 共存原理（三层一致化）

glibc 版 rootfs 内 163 个二进制文件（libc.so.6、libX11.so、wineserver 等）硬编码了 `/data/data/com.winlator/files/rootfs/` 前缀——这是编译 glibc 时的 `--prefix`，烧进 ELF 字符串表。改包名后沙箱禁止访问旧目录，会导致容器卡加载或闪退。

本项目通过三层一致化解决：
1. **Manifest 层**：FileProvider / MTDataFiles authority 改用 `${applicationId}`
2. **Java 层**：5 个文件的硬编码路径改用 `BuildConfig.APPLICATION_ID` / `getPackageName()` 动态拼接
3. **assets 层**：所有 .tzst 包先 zstd 解压 → tar 字节流等长替换 → 校验 → 重新压缩

## 快速开始

### 1. Fork 上游仓库

Fork https://github.com/hostei33/winlator-cn 到你的账号。

### 2. 覆盖共存文件

将本项目的以下文件**覆盖**到你的 fork 仓库（保持目录结构）：

```
coexist.properties                              ← 包名配置（改这里）
app/build.gradle                                ← 占位符版，CI 自动替换
app/src/main/AndroidManifest.xml                ← authority 动态化
gradle.properties                               ← 完整文件（末尾有换行）
app/src/main/java/com/winlator/container/Container.java
app/src/main/java/com/winlator/core/AppUtils.java
app/src/main/java/com/winlator/core/FileUtils.java
app/src/main/java/com/winlator/widget/EnvVarsView.java
app/src/main/java/com/winlator/winhandler/WinHandler.java
app/src/main/cpp/gladiorenderer/CMakeLists.txt  ← 去掉 -Werror 兼容 NDK 24
scripts/patch-assets.py                         ← assets 等长替换（核心）
scripts/apply-config.sh                         ← 应用配置到 build.gradle
scripts/pre-release-check.sh                    ← 发布前自检
scripts/generate-changelog.sh                   ← 生成更新日志
app/src/main/java/com/winlator/XServerDisplayActivity.java  ← 容器启动解压 win-fg layer
app/src/main/assets/winfg/README.md             ← win-fg 目录说明（.so 由 CI 自动下载）
THIRD_PARTY_LICENSES/win-fg-LICENSE             ← win-fg MIT 许可证（合规必备）
.github/workflows/build-coexist.yml             ← 主构建+发布（含 win-fg 自动拉取）
.github/workflows/sync-upstream.yml             ← 自动同步上游（含 win-fg 版本跟踪）
.github/workflows/android-ci.yml                ← 调试用（仅编译 Java，可选）
```

### 3. 开启 Actions 写权限

Settings → Actions → General → Workflow permissions → **Read and write permissions**

### 4. 触发构建

push 任意非 .md 文件到 main 分支，或在 Actions 页面手动运行「构建共存版APK」。

> 注意：`.md` 文件改动不会触发构建（`paths-ignore`）。

## 改包名

编辑 `coexist.properties`：

```properties
app.id=org.winlator   # 必须 12 字符（与 com.winlator 等长）
```

可用示例（均为 12 字符）：`org.winlator`、`net.winlator`、`io.winlator`

**为什么必须 12 字符？** 等长替换不改变 ELF 字符串表长度，不破坏二进制结构。不等长替换会导致 rootfs 内二进制文件损坏。改完后 push 即可，CI 会自动应用到所有层。

## 自动发布与 Latest 判断

构建成功后自动发布 Release，发布类型由自检结果决定：

| 自检结果 | 发布类型 | Latest |
|---|---|---|
| 全部通过，无跳过项 | 正式版（release） | 自动设为 Latest |
| 有跳过项（aapt/apksigner 工具缺失） | 预发布版（pre-release） | 不覆盖 Latest |

自检项：APK 大小（≥100MB）、包名、签名、5 个动态 native 渲染器、核心 assets、native 库大小（≥5MB）。

## 自动同步上游

`sync-upstream.yml` 每天北京时间 02:00 自动同步上游，也可手动运行。

- 合并策略：`-X ours`，共存改动文件优先保留我们的版本
- 若上游更新了共存改动文件，会发出 **warning** 注解，需人工核对是否丢失上游功能
- 合并后 push 到 main 会自动触发构建发布
- **同时跟踪 win-fg 上游版本**：检测到 win-fg 新 release 会自动更新 `.winfg-version` 并触发构建

## 帧生成（win-fg 集成）

本项目集成了 [win-fg](https://github.com/The412Banner/win-fg)（MIT License）帧生成引擎，为容器内 DXVK/Vulkan 应用提供插帧能力。

### 工作原理

- win-fg 以 **Vulkan implicit layer** 形态挂进 guest 的 Vulkan loader，hook `CreateSwapchainKHR` / `QueuePresentKHR`，在两帧之间合成中间帧
- 采用 clean-room 光学流（Lucas-Kanade 仿射 + TV-L1 正则化 + FSR3 光学流重实现）+ anti-ghost + 遮挡门控 + HUD 排除
- 对 Turnip 和 Vortek 两种驱动均生效（layer 挂 loader 层，与 ICD 无关）

### 自动跟随上游（不 fork）

- **不 fork win-fg 源码**：MIT 协议允许直接使用，win-fg 与 winlator 完全解耦，fork 只会增加维护负担
- CI 构建时自动从 win-fg 官方最新 release 下载预编译 `libwin_fg.so`（`releases/latest/download/` 重定向 URL，零 API 调用不限流）
- `sync-upstream.yml` 每日检测 win-fg 版本，有更新自动触发构建
- 许可证合规：`THIRD_PARTY_LICENSES/win-fg-LICENSE` 保留完整 MIT 文本

### 开关

- 默认启用：容器环境变量 `WIN_FG_ENABLE=1`（已在 `DEFAULT_ENV_VARS` 中）
- 关闭：容器设置 → 环境变量，删除 `WIN_FG_ENABLE=1`，重启容器
- 进阶调优：可在容器环境变量添加 `WIN_FG_PERF_PRESET=0|1|2`（性能档位）、`WIN_FG_MODEL=3|4`（模型）、`WIN_FG_MULTIPLIER=2|3|4`（倍率）等，详见 [win-fg config.hpp](https://github.com/The412Banner/win-fg/blob/master/src/config.hpp)

### 注意

- win-fg layer 形态的插帧效率取决于显示链是否丢弃生成帧，需真机验证
- 插帧本身耗 GPU（约 2.5–4.5ms/帧，1080p Balanced），低端 GPU 可能负优化
- 仅对 Vulkan 应用（DXVK）生效，OpenGL 应用不受影响

## 自定义签名（可选）

在仓库 Settings → Secrets and variables → Actions 添加：
- `KEYSTORE_BASE64`：keystore 文件 base64 编码
- `KEYSTORE_PASSWORD`
- `KEY_ALIAS`
- `KEY_PASSWORD`

不配置则使用自动生成的 debug 签名（也能安装运行，只是无法上架应用商店）。

## 文件说明

| 文件 | 作用 |
|---|---|
| `coexist.properties` | 包名配置（唯一需要日常修改的文件） |
| `scripts/patch-assets.py` | assets 等长包名替换（先解压→替换→重压缩） |
| `scripts/apply-config.sh` | 读取配置，替换 build.gradle 占位符 |
| `scripts/pre-release-check.sh` | 发布前自检（含 win-fg 集成检查） |
| `scripts/generate-changelog.sh` | 对比上次 Release tag 生成更新日志（含 win-fg 版本） |
| `app/src/main/java/com/winlator/XServerDisplayActivity.java` | 容器启动时解压 win-fg layer 到 rootfs |
| `app/src/main/assets/winfg/` | win-fg 资源目录（.so 由 CI 自动下载，勿手动提交） |
| `THIRD_PARTY_LICENSES/win-fg-LICENSE` | win-fg MIT 许可证（合规必备） |
| `.github/workflows/build-coexist.yml` | 主构建流程（配置→拉取win-fg→替换→编译→自检→发布） |
| `.github/workflows/sync-upstream.yml` | 每日同步上游 + win-fg 版本跟踪 |
| `.github/workflows/android-ci.yml` | 调试用（仅编译 Java 不打包，已禁用自动触发） |

## 限制与注意

- **包名必须 12 字符**（glibc 硬约束，无法绕过）
- 与原版容器数据不互通（沙箱隔离）
- 必须卸载旧版共存版才能安装不同签名的新版本
- CI 仅验证编译通过和静态自检，运行稳定性需真机测试
- 上游大版本升级（如 glibc 变更）后，建议人工验证共存功能
- win-fg 帧生成效果需真机验证（layer 形态可能存在生成帧丢弃问题），不满意可删除 `WIN_FG_ENABLE=1` 关闭
