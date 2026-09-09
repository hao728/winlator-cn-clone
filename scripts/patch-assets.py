#!/usr/bin/env python3
"""
Winlator 共存版 assets 等长包名替换脚本

原理：glibc rootfs 中 163 个二进制文件硬编码了 /data/data/com.winlator/files/rootfs/
前缀（编译 glibc 时的 --prefix，烧进 ELF 字符串表）。不等长替换会破坏 ELF 结构，
因此只支持与 com.winlator 等长（12字符）的包名。

正确做法：对 .tzst 文件先 zstd 解压 → 在 tar 字节流里等长替换 → 校验 → 重新 zstd 压缩。
绝不能直接在压缩后的字节流上替换，否则会破坏 zstd 压缩流导致 rootfs 损坏。

用法：python3 patch-assets.py [--config coexist.properties] [--assets-dir app/src/main/assets]
"""
import os
import sys
import argparse
import zstandard
import tarfile
import io

ORIG_PKG = "com.winlator"
ORIG_LEN = len(ORIG_PKG)  # 12
ZSTD_LEVEL = 18  # 压缩级别，与上游保持一致


def read_config(config_path):
    """读取 coexist.properties，返回 app.id"""
    cfg = {}
    if not os.path.exists(config_path):
        print(f"[WARN] 配置文件 {config_path} 不存在，使用默认 org.winlator")
        return "org.winlator"
    with open(config_path, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            cfg[k.strip()] = v.strip()
    app_id = cfg.get("app.id", "org.winlator")
    return app_id


def validate_pkg(app_id):
    """校验包名长度，必须与 com.winlator 等长"""
    if len(app_id) != ORIG_LEN:
        print(f"[ERROR] 包名 '{app_id}' 长度为 {len(app_id)}，必须为 {ORIG_LEN} 字符（与 {ORIG_PKG} 等长）")
        print(f"        原因：glibc rootfs 二进制硬编码了路径前缀，不等长替换会破坏 ELF 结构")
        print(f"        可用示例：org.winlator / net.winlator / io.winlator")
        sys.exit(1)
    if app_id == ORIG_PKG:
        print(f"[ERROR] 包名不能与原版相同 '{ORIG_PKG}'，否则无法共存")
        sys.exit(1)
    parts = app_id.split(".")
    if len(parts) < 2 or any(not p for p in parts):
        print(f"[ERROR] 包名格式不合法: '{app_id}'")
        sys.exit(1)
    print(f"[OK] 包名校验通过: {app_id} (长度 {len(app_id)})")


def patch_tar_zst(filepath, old, new):
    """
    对 .tzst (tar.zst) 文件做等长包名替换。
    正确流程：zstd 解压 → tar 字节流等长替换 → 校验 → 重新 zstd 压缩。
    等长替换不改变 tar 任何成员的 size/偏移，结构必然一致。
    """
    if not os.path.exists(filepath):
        return 0
    with open(filepath, "rb") as f:
        raw = f.read()

    old_bytes = old.encode("utf-8")
    new_bytes = new.encode("utf-8")

    # 第一步：zstd 解压
    try:
        tar_bytes = zstandard.ZstdDecompressor().stream_reader(io.BytesIO(raw)).read()
    except Exception as e:
        print(f"  [WARN] {os.path.basename(filepath)}: zstd 解压失败 ({e})，跳过")
        return 0

    # 第二步：在解压后的 tar 字节流里统计并替换
    before = tar_bytes.count(old_bytes)
    if before == 0:
        return 0

    patched = tar_bytes.replace(old_bytes, new_bytes)

    # 第三步：校验
    after_residual = patched.count(old_bytes)
    if after_residual != 0:
        print(f"  [ERROR] {os.path.basename(filepath)}: 替换后仍有 {after_residual} 处残留")
        sys.exit(1)
    if len(patched) != len(tar_bytes):
        print(f"  [ERROR] {os.path.basename(filepath)}: 等长替换后大小变了 ({len(tar_bytes)} -> {len(patched)})")
        sys.exit(1)

    # tar 结构轻量校验（等长替换不改变成员偏移，这里仅确认能正常解析）
    try:
        with tarfile.open(fileobj=io.BytesIO(patched), mode="r:") as tf:
            _ = len(tf.getmembers())
    except Exception as e:
        print(f"  [ERROR] {os.path.basename(filepath)}: 替换后 tar 结构校验失败 ({e})")
        sys.exit(1)

    # 第四步：重新 zstd 压缩
    comp = zstandard.ZstdCompressor(level=ZSTD_LEVEL).compress(patched)
    with open(filepath, "wb") as f:
        f.write(comp)

    return before


def patch_plain_file(filepath, old, new):
    """对普通文本/二进制文件做等长字节替换（非压缩文件可直接替换）"""
    if not os.path.exists(filepath):
        return 0
    with open(filepath, "rb") as f:
        raw = f.read()
    old_bytes = old.encode("utf-8")
    new_bytes = new.encode("utf-8")
    count = raw.count(old_bytes)
    if count == 0:
        return 0
    patched = raw.replace(old_bytes, new_bytes)
    if len(patched) != len(raw):
        print(f"  [ERROR] {os.path.basename(filepath)}: 等长替换后大小变了")
        sys.exit(1)
    with open(filepath, "wb") as f:
        f.write(patched)
    return count


def patch_assets_dir(assets_dir, old, new):
    """遍历 assets 目录，对所有文件做等长替换"""
    total = 0
    results = {}
    for root, dirs, files in os.walk(assets_dir):
        for fname in files:
            fpath = os.path.join(root, fname)
            rel = os.path.relpath(fpath, assets_dir)
            if fname.endswith(".tzst"):
                cnt = patch_tar_zst(fpath, old, new)
            else:
                cnt = patch_plain_file(fpath, old, new)
            if cnt > 0:
                results[rel] = cnt
                total += cnt
                print(f"  {cnt:4d}  {rel}")
    return total, results


def verify_no_residual(assets_dir, old):
    """验证替换后无残留（对 .tzst 需先解压再检查）"""
    residual = 0
    old_bytes = old.encode("utf-8")
    for root, dirs, files in os.walk(assets_dir):
        for fname in files:
            fpath = os.path.join(root, fname)
            try:
                with open(fpath, "rb") as f:
                    raw = f.read()
                if fname.endswith(".tzst"):
                    # 压缩文件需先解压再检查
                    try:
                        tar_bytes = zstandard.ZstdDecompressor().stream_reader(io.BytesIO(raw)).read()
                        residual += tar_bytes.count(old_bytes)
                    except Exception:
                        pass  # 解压失败的文件跳过
                else:
                    residual += raw.count(old_bytes)
            except Exception:
                pass
    return residual


def main():
    parser = argparse.ArgumentParser(description="Winlator 共存版 assets 等长包名替换")
    parser.add_argument("--config", default="coexist.properties", help="配置文件路径")
    parser.add_argument("--assets-dir", default="app/src/main/assets", help="assets 目录")
    args = parser.parse_args()

    app_id = read_config(args.config)
    validate_pkg(app_id)

    if not os.path.isdir(args.assets_dir):
        print(f"[ERROR] assets 目录不存在: {args.assets_dir}")
        sys.exit(1)

    print(f"\n开始替换: {ORIG_PKG} -> {app_id}")
    print(f"assets 目录: {args.assets_dir}")
    print(f"处理方式: zstd解压 → tar流等长替换 → 校验 → 重新zstd压缩\n")

    total, results = patch_assets_dir(args.assets_dir, ORIG_PKG, app_id)

    print(f"\n替换完成，共 {total} 处")

    residual = verify_no_residual(args.assets_dir, ORIG_PKG)
    if residual > 0:
        print(f"\n[WARN] 仍有 {residual} 处残留（可能在无法替换的二进制结构中）")
    else:
        print(f"\n[OK] 无残留，替换完整")

    print(f"\n[完成] assets 已适配包名 {app_id}")


if __name__ == "__main__":
    main()
