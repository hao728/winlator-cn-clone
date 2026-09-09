#!/usr/bin/env python3
"""
Winlator 共存版 assets 等长包名替换脚本

原理：glibc rootfs 中 163 个二进制文件硬编码了 /data/data/com.winlator/files/rootfs/
前缀（编译 glibc 时的 --prefix，烧进 ELF 字符串表）。不等长替换会破坏 ELF 结构，
因此只支持与 com.winlator 等长（12字符）的包名。

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
    # 基本格式校验
    parts = app_id.split(".")
    if len(parts) < 2 or any(not p for p in parts):
        print(f"[ERROR] 包名格式不合法: '{app_id}'")
        sys.exit(1)
    print(f"[OK] 包名校验通过: {app_id} (长度 {len(app_id)})")


def patch_tar_zst(filepath, old, new):
    """对 .tzst (tar.zst) 文件做等长字节替换，保持 tar 结构不变"""
    if not os.path.exists(filepath):
        return 0
    with open(filepath, "rb") as f:
        raw = f.read()
    old_bytes = old.encode("utf-8")
    new_bytes = new.encode("utf-8")
    count = raw.count(old_bytes)
    if count == 0:
        return 0
    # 等长替换，直接字节替换，不改变文件大小和 tar 结构
    patched = raw.replace(old_bytes, new_bytes)
    with open(filepath, "wb") as f:
        f.write(patched)
    return count


def patch_plain_file(filepath, old, new):
    """对普通文本/二进制文件做等长字节替换"""
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
    return total, results


def verify_no_residual(assets_dir, old):
    """验证替换后无残留（排除不可能替换的情况）"""
    residual = 0
    for root, dirs, files in os.walk(assets_dir):
        for fname in files:
            fpath = os.path.join(root, fname)
            try:
                with open(fpath, "rb") as f:
                    raw = f.read()
                residual += raw.count(old.encode("utf-8"))
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

    total, results = patch_assets_dir(args.assets_dir, ORIG_PKG, app_id)

    print(f"\n替换完成，共 {total} 处")
    print("各文件替换统计:")
    for rel, cnt in sorted(results.items(), key=lambda x: -x[1]):
        print(f"  {cnt:4d}  {rel}")

    residual = verify_no_residual(args.assets_dir, ORIG_PKG)
    if residual > 0:
        print(f"\n[WARN] 仍有 {residual} 处残留（可能在无法替换的二进制结构中）")
    else:
        print(f"\n[OK] 无残留，替换完整")

    print(f"\n[完成] assets 已适配包名 {app_id}")


if __name__ == "__main__":
    main()
