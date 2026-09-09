# Winlator 共存版

基于 [hostei33/winlator-cn](https://github.com/hostei33/winlator-cn)（Winlator 11.2 cn.03 glibc 版），改包名后可与原版共存。

## 快速开始

1. Fork 本仓库
2. 开启 Actions 写权限：Settings → Actions → General → Workflow permissions → Read and write
3. push 到 main，自动构建并发布 Release

## 改包名

编辑 `coexist.properties`：

```properties
app.id=org.winlator   # 必须 12 字符（与 com.winlator 等长）
```

可用：`org.winlator`、`net.winlator`、`io.winlator`

**为什么必须 12 字符？** glibc rootfs 中 163 个二进制文件（libc.so.6 等）硬编码了 `/data/data/com.winlator/files/rootfs/` 前缀，烧进 ELF 字符串表。不等长替换会破坏 ELF 结构导致闪退。等长替换不改变字符串长度，安全。

## 自动同步上游

`sync-upstream.yml` 每天北京时间 02:00 自动同步上游。合并时保留共存改动，若上游更新了共存改动文件会发警告。

## 文件说明

| 文件 | 作用 |
|---|---|
| `coexist.properties` | 包名配置（改这里） |
| `scripts/patch-assets.py` | assets 等长包名替换 |
| `scripts/apply-config.sh` | 应用配置到 build.gradle |
| `scripts/pre-release-check.sh` | 发布前自检（签名/包名/native库/assets） |
| `scripts/generate-changelog.sh` | 生成更新日志 |
| `.github/workflows/build-coexist.yml` | 主构建+发布 |
| `.github/workflows/sync-upstream.yml` | 同步上游 |

## 自定义签名（可选）

在仓库 Settings → Secrets and variables → Actions 添加：
- `KEYSTORE_BASE64`：keystore 文件 base64
- `KEYSTORE_PASSWORD`
- `KEY_ALIAS`
- `KEY_PASSWORD`

不配置则用 debug 签名。

## 限制

- 包名必须 12 字符（glibc 硬约束）
- 与原版容器数据不互通
- CI 仅验证编译通过，运行稳定性需真机测试
