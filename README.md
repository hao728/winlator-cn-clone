<p align="center">
  <h1 align="center">Winlator 共存版 (winlator-cn-clone)</h1>
  <p align="center"><b>Windows 应用与游戏在 Android 上运行 — 支持包名共存 + win-fg 帧生成</b></p>
</p>

<p align="center">
  <a href="https://github.com/hao728/winlator-cn-clone/releases/latest">
    <img src="https://img.shields.io/badge/⬇%20下载-最新%20Release-2d9bff?style=for-the-badge&logo=android&logoColor=white" alt="Download Latest Release">
  </a>
  <a href="https://github.com/hao728/winlator-cn-clone/actions">
    <img src="https://img.shields.io/badge/CI-Auto%20Build-52C41A?style=for-the-badge&logo=githubactions&logoColor=white" alt="CI Build">
  </a>
  <img src="https://img.shields.io/badge/Platform-Android%208.0%2B-7a4cff?style=for-the-badge" alt="Platform">
  <img src="https://img.shields.io/badge/License-MIT-ff2d9b?style=for-the-badge" alt="License">
</p>

<p align="center">
  <a href="#-项目声明">项目声明</a> •
  <a href="#-功能特性">功能特性</a> •
  <a href="#-下载安装">下载安装</a> •
  <a href="#-使用说明">使用说明</a> •
  <a href="#-帧生成-win-fg">帧生成</a> •
  <a href="#-已知限制">已知限制</a> •
  <a href="#-编译与-ci">编译与 CI</a> •
  <a href="#-调试指南">调试指南</a> •
  <a href="#-第三方许可">第三方许可</a>
</p>

---

## 📌 项目声明

> **本项目是 [hostei33/winlator-cn](https://github.com/hostei33/winlator-cn) 的个人共存定制 fork。**
>
> 核心改动：
> - **包名共存**：通过三层包名一致化（Manifest / Java / assets rootfs），可与原版 `com.winlator` 同时安装，数据独立。
> - **win-fg 帧生成**：集成 [The412Banner/win-fg](https://github.com/The412Banner/win-fg) Vulkan 光学流插帧引擎（MIT 协议），CI 自动跟随上游最新 release。
> - **自动构建**：GitHub Actions 每日同步上游，push 只编译，打 tag 才发布 Release。
>
> **本项目不是官方 Winlator 发布**，按"原样"提供，仅供学习与个人使用。不保证在所有设备上正常运行。

---

## ✨ 功能特性

| 类别 | 功能 |
|---|---|
| **共存** | 包名可改（需 12 字符等长，如 `org.winlator`），与原版 `com.winlator` 数据独立 |
| **帧生成** | 集成 win-fg v0.3.0，支持 2x/3x/4x 插帧，质量/平衡/性能三档可调 |
| **图形驱动** | Turnip / Vortek / Zink / VirGL / Gladio 五选一切换 |
| **容器管理** | 多容器独立配置，支持 Wine 版本切换、DXVK/VKD3D 组件管理 |
| **中文环境** | 默认 `LC_ALL=zh_CN.utf8`，时区 `Asia/Shanghai` |
| **自动同步** | 每日自动合并上游 winlator-cn 最新提交 |
| **CI 构建** | push 自动编译验证，打 `v*` tag 自动发布 Release |

---

## ⬇ 下载安装

1. 前往 [Releases](https://github.com/hao728/winlator-cn-clone/releases) 下载最新 APK（约 215MB）
2. 卸载旧版共存版（如有）
3. 安装本 APK
4. 首次启动自动解压 rootfs（约 1-2 分钟）
5. 可与原版 `com.winlator` 共存，数据独立

> **包名**：当前 Release 为 `org.winlator`（与原版等长 12 字符，glibc 硬约束）

---

## 📖 使用说明

### 基础使用

1. 打开应用 → 点击右下角 **+** 创建容器
2. 选择 Wine 版本、屏幕分辨率、图形驱动
3. 点击确认，等待容器创建完成
4. 点击容器启动，进入 Windows 环境

### 图形驱动选择

| 驱动 | 适用场景 |
|---|---|
| **Turnip** | Adreno GPU 默认，性能最好 |
| **Vortek** | 兼容 Host 渲染，部分游戏更稳定 |
| **Zink** | OpenGL 转 Vulkan，通用兼容 |
| **VirGL** | 老旧设备兼容 |
| **Gladio** | 实验性 |

---

## 🎞️ 帧生成 (win-fg)

### 开启方法

1. 进入容器详情 → **高级** 标签页
2. 找到 **帧生成 (win-fg)** 区域
3. 勾选 **启用帧生成**
4. 选择性能档位：
   - **质量**：光流最精细，延迟最低，帧率提升较小
   - **平衡**：默认，质量与性能折中
   - **性能**：光流最粗，帧率提升最大，可能有伪影
5. 选择插帧倍率：**2x / 3x / 4x**
6. 勾选 **调试模式** 可输出详细日志（排查问题时开启）
7. 保存并重启容器

### 验证是否生效

- 开启调试模式后，启动容器运行游戏
- 点击 **导出 win-fg 调试日志** 按钮
- 日志保存在 `Download/winfg_log_*.txt`
- 日志中出现 `winfg` / `optical flow` / `interpolation` 关键词即表示 layer 已加载

### 性能参考（win-fg 官方数据，Adreno 750 / 1080p）

| 模式 | 原始帧率 | 插帧后 | 每帧开销 |
|---|---|---|---|
| 平衡 | 45 fps | ~90 fps | 2.5-4.5ms |
| 平衡 | 58 fps | ~115 fps | 2.5-4.5ms |

> **注意**：guest layer 模式下，部分生成帧可能被 host compositor 丢弃，实际帧率提升约为理论值的 25-50%。这是 Vulkan layer 架构的固有局限。

---

## ⚠️ 已知限制

1. **包名必须 12 字符等长**：glibc rootfs 内 163 个二进制硬编码 `/data/data/com.winlator/...`，改包名需等长替换。可用示例：`org.winlator`、`net.winlator`、`io.winlator`
2. **与原版容器数据不互通**：共存版有独立的 `/data/data/org.winlator/` 沙箱
3. **win-fg guest layer 限制**：在 Vortek 假 swapchain 下可能丢帧；Turnip 驱动下效果最佳
4. **CI 仅验证编译通过**：运行稳定性需真机测试
5. **上游同步冲突**：每日自动合并且上游改动与共存改动重叠时，可能需要手动解决

---

## 🔧 编译与 CI

### 本地编译

```bash
# 依赖：JDK 17、Android SDK、NDK 24.0.8215888、CMake 3.22.1
git clone https://github.com/hao728/winlator-cn-clone.git
cd winlator-cn-clone
./gradlew assembleRelease
```

### CI 流水线

| Workflow | 触发 | 行为 |
|---|---|---|
| `build-coexist.yml` | push 到 main | 编译 APK，生成 Artifacts，**不发布** |
| `build-coexist.yml` | push `v*` tag | 编译 + 自检 + 发布 Release |
| `sync-upstream.yml` | 每日定时 | 自动合并上游 winlator-cn 最新提交 |
| `android-ci.yml` | 手动 | 调试用构建 |

### 发布流程

1. 代码合并到 main 后，CI 自动编译验证
2. 确认稳定后，打 tag：`v11.2-coexist-winfg-v0.3.0`
3. CI 自动构建并发布 Release
4. 自检全过 → 自动设为 Latest；有跳过项 → pre-release（需人工验证）

### 修改包名

编辑 `coexist.properties`：
```properties
app.id=org.winlator
app.name=Winlator共存版
version.code.offset=1000
```
> `app.id` 必须为 12 字符（含点），与 `com.winlator` 等长。

---

## 🐛 调试指南

### 导出 win-fg 日志

1. 容器详情 → 高级 → 勾选 **调试模式**
2. 启动容器运行目标程序
3. 返回容器详情 → 点击 **导出 win-fg 调试日志**
4. 日志保存到 `Download/winfg_log_时间戳.txt`

### 常见问题

| 现象 | 可能原因 | 解决方法 |
|---|---|---|
| 卡加载无法进入容器 | 包名不等长 / rootfs 未正确替换 | 确认包名 12 字符，重新构建 |
| 闪退 | 图形驱动不兼容 | 切换 Turnip/Vortek 尝试 |
| win-fg 无效果 | layer 未加载 / 驱动不支持 | 开启调试模式看日志，确认 Vulkan 驱动 |
| 帧率无提升 | host 丢弃生成帧 | guest layer 固有局限，属正常现象 |
| 编译失败 | NDK/CMake 版本不匹配 | 确认 NDK 24.0.8215888 + CMake 3.22.1 |

### 日志关键词

- `WIN_FG_ENABLE` — 环境变量是否设置
- `VK_LAYER_WIN_framegen` — layer 是否被 Vulkan loader 识别
- `optical flow` — 光流计算是否执行
- `interpolation` — 帧插值是否执行

---

## 📜 第三方许可

| 组件 | 协议 | 来源 |
|---|---|---|
| Winlator (winlator-cn) | MIT/GPL | [hostei33/winlator-cn](https://github.com/hostei33/winlator-cn) |
| win-fg 帧生成引擎 | MIT | [The412Banner/win-fg](https://github.com/The412Banner/win-fg) |
| Box64 | MIT | [ptitSeb/box64](https://github.com/ptitSeb/box64) |
| Wine | LGPL | [winehq.org](https://www.winehq.org/) |
| DXVK | zlib | [doitsujin/dxvk](https://github.com/doitsujin/dxvk) |
| Turnip (Mesa) | MIT | [mesa3d.org](https://www.mesa3d.org/) |

> win-fg 完整源码与逐 shader 来源说明见 [The412Banner/win-fg](https://github.com/The412Banner/win-fg)。本项目仅使用其官方 release 预编译 `.so`，不修改源码。

---

## 🙏 致谢

- [hostei33](https://github.com/hostei33) — winlator-cn 上游
- [The412Banner](https://github.com/The412Banner) — win-fg 帧生成引擎 & Bannerlator 参考实现
- [alexvorxx](https://github.com/alexvorxx) — Winlator 原作者
- 所有开源贡献者

---

<p align="center">
  <sub>Built with ❤️ for Android gaming · 按"原样"提供，无任何担保</sub>
</p>
