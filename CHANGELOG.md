# 更新日志

本项目遵循 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/) 格式，
版本号遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

---

## [Unreleased]

### 新增
- 待添加

### 修复
- 待添加

### 变更
- 待添加

### 已知限制
- 待添加

---

## [v11.2-coexist-winfg-v0.3.0] - 2026-09-10

### 新增
- 集成 win-fg v0.3.0 帧生成引擎（Vulkan layer，MIT）
- 容器详情页增加帧生成设置面板（开关/性能档位/倍率/调试模式）
- 一键导出 win-fg 调试日志功能
- CI 自动拉取 win-fg 上游最新 release
- 每日自动同步上游 winlator-cn

### 修复
- 三层包名一致化（Manifest / Java / assets rootfs），解决改包名后卡加载问题
- rootfs 等长包名替换（解压→替换→重压缩+三重校验）
- 自检脚本修复 libadrenotools.so 误报（静态库不单独出现）

### 变更
- 发布模式改为 tag-only：push 只编译，打 `v*` tag 才发布 Release
- 提交历史清洗为单条 baseline

### 已知限制
- 包名必须 12 字符等长（glibc 硬约束）
- win-fg guest layer 模式下实际帧率提升约为理论值的 25-50%
- Vortek 驱动下 win-fg 可能丢帧，Turnip 驱动效果最佳

---

## 历史版本

更早版本的提交历史已在 v11.2-coexist-winfg-v0.3.0 中清洗合并。
如需查看历史，请访问 [上游仓库](https://github.com/hostei33/winlator-cn)。

---

[Unreleased]: https://github.com/hao728/winlator-cn-clone/compare/v11.2-coexist-winfg-v0.3.0...HEAD
[v11.2-coexist-winfg-v0.3.0]: https://github.com/hao728/winlator-cn-clone/releases/tag/v11.2-coexist-winfg-v0.3.0
