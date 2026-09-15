# Winlator 共存版

基于 [hostei33/winlator-cn](https://github.com/hostei33/winlator-cn) 的个人修改版，可通过 Wine 和 Box86/Box64 在 Android 上运行 Windows (x86_64) 应用程序。

核心改动：**包名可共存** + **win-fg 帧生成** + **每日自动同步上游构建**。

## 下载

[Releases 页面](https://github.com/hao728/winlator-cn-clone/releases) 下载最新 APK。

当前包名：`org.winlator`（可与原版 `com.winlator` 同时安装，数据独立）。

## 特性

- **包名共存**：修改 `coexist.properties` 中的 `app.id` 即可更换包名（需 12 字符等长，如 `org.winlator`、`net.winlator`），CI 自动应用到 Manifest / Java / assets rootfs 三层
- **win-fg 帧生成**：集成 [The412Banner/win-fg](https://github.com/The412Banner/win-fg) 光学流插帧引擎，native 模式编译进 host compositor（libvortekrenderer.so），CI 构建时自动拉取上游最新源码
- **自动同步**：每日 UTC 18:00（北京时间次日 02:00）自动合并上游 winlator-cn 最新提交，有更新则构建并发布 Release
- **中文环境**：默认 `LC_ALL=zh_CN.utf8`，时区 `Asia/Shanghai`

## 包名共存原理

glibc rootfs 内的二进制文件硬编码了 `/data/data/com.winlator/files/rootfs/` 路径，因此改包名必须保持等长（12 字符）。CI 构建时自动完成：

1. `AndroidManifest.xml`：FileProvider authority 使用 `${applicationId}`
2. Java 层：5 处硬编码包名替换为 `BuildConfig.APPLICATION_ID`
3. `assets/rootfs.tzst`：zstd 解压 → tar 等长替换 → 重压缩

不等长替换会破坏 ELF 结构导致闪退。

## win-fg 帧生成

- **集成方式**：native 编译进 `libvortekrenderer.so`，非 Vulkan layer 模式
- **开启位置**：容器详情 → 高级选项 → 帧生成
- **上游跟随**：CI 构建时从 win-fg 上游 master 分支拉取最新源码（framegen.cpp/hpp 等 7 个核心文件），自动跟随更新
- **协议**：MIT

## 编译

依赖：JDK 17、Android SDK、NDK 24.0.8215888、CMake 3.22.1

```bash
git clone https://github.com/hao728/winlator-cn-clone.git
cd winlator-cn-clone
./gradlew assembleRelease
```

修改包名：编辑 `coexist.properties` 的 `app.id`（必须 12 字符），重新构建即可。

## CI 工作流

| 工作流 | 触发 | 行为 |
|---|---|---|
| `build-coexist.yml` | push 到 main / PR | 编译 APK，生成 Artifact，不发布 |
| `sync-upstream.yml` | 每日定时 / 手动 | 合并上游 → 保护定制文件 → 构建 → 自检 → 发布 Release |

同步上游时以下文件受区域保护，不会被上游覆盖：`README.md`、`CHANGELOG.md`、`coexist.properties`、`app/build.gradle`、`gradle.properties`、`AndroidManifest.xml`、`winfg-native.patch`、`docs/`、`.github/`、`scripts/`。

## 已知限制

- 包名必须 12 字符等长（glibc 硬约束）
- 与原版容器数据不互通（独立沙箱）
- win-fg 帧生成效果因设备和游戏而异，需真机测试
- CI 仅验证编译通过，运行稳定性需真机验证

## 致谢与第三方项目

- 原项目 [brunodev85/winlator](https://github.com/brunodev85/winlator)
- 中文上游 [hostei33/winlator-cn](https://github.com/hostei33/winlator-cn)
- win-fg 帧生成 [The412Banner/win-fg](https://github.com/The412Banner/win-fg)
- Bannerlator 参考实现 [The412Banner/Bannerlator](https://github.com/The412Banner/Bannerlator)
- GLIBC Patches by [Termux Pacman](https://github.com/termux-pacman/glibc-packages)
- Wine ([winehq.org](https://www.winehq.org/))
- Box86/Box64 by [ptitSeb](https://github.com/ptitSeb)
- Mesa (Turnip/Zink/VirGL) ([mesa3d.org](https://www.mesa3d.org))
- DXVK ([doitsujin/dxvk](https://github.com/doitsujin/dxvk))
- VKD3D ([gitlab.winehq.org/wine/vkd3d](https://gitlab.winehq.org/wine/vkd3d))

本项目按"原样"提供，仅供学习与个人使用，不保证在所有设备上正常运行。
