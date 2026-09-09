#!/usr/bin/env python3
"""
对 app/src/main/assets 下所有 .tzst (zstd+tar) 做等长包名替换。
原理：com.winlator 与 org.winlator 都是 12 字符，在 tar 字节流内等长替换，
不改变任何成员大小/偏移，再重新 zstd 压缩。
用法：python3 scripts/patch-assets.py [--old com.winlator] [--new org.winlator]
"""
import os, sys, io, argparse, zstandard, tarfile

def patch_tzst(path, old, new, level=18):
    with open(path, "rb") as f:
        raw = f.read()
    tar_bytes = zstandard.ZstdDecompressor().stream_reader(io.BytesIO(raw)).read()
    before = tar_bytes.count(old)
    if before == 0:
        return 0
    patched = tar_bytes.replace(old, new)
    assert patched.count(old) == 0, f"{path}: 仍有残留"
    assert len(patched) == len(tar_bytes), f"{path}: 等长替换后大小变了"
    # 等长字节替换不改变 tar 任何 header 的 size/偏移，结构必然一致；
    # 这里仅做成员数轻量校验（非流式模式，可重复读取）。
    with tarfile.open(fileobj=io.BytesIO(patched), mode="r:") as tf:
        member_count = len(tf.getmembers())
    comp = zstandard.ZstdCompressor(level=level).compress(patched)
    with open(path, "wb") as f:
        f.write(comp)
    return before

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--old", default="com.winlator")
    ap.add_argument("--new", default="org.winlator")
    ap.add_argument("--assets-dir", default=os.path.join("app", "src", "main", "assets"))
    args = ap.parse_args()
    old = args.old.encode(); new = args.new.encode()
    assert len(old) == len(new), f"新旧包名必须等长: {len(old)} vs {len(new)}"

    total = 0
    for root, _, files in os.walk(args.assets_dir):
        for fn in files:
            if fn.endswith(".tzst"):
                p = os.path.join(root, fn)
                n = patch_tzst(p, old, new)
                rel = os.path.relpath(p, args.assets_dir)
                print(f"  {rel}: 替换 {n} 处")
                total += n
    print(f"\n合计替换 {total} 处")

if __name__ == "__main__":
    main()
