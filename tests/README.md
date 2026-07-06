# 手动测试指南

## 测试数据

`testdata/` 目录是一个模拟真实场景的目录树，用作备份的**源目录**。

### 包含的文件类型

| 路径 | 类型 | 大小 | 说明 |
|------|------|------|------|
| `hello.txt` | 普通文件 | 13 B | 简单文本 |
| `data.txt` | 普通文件 | 21 B | 多行文本，权限 600 |
| `small.txt` | 普通文件 | 5 B | 极小文件 |
| `one_kb.txt` | 普通文件 | 1023 B | ~1KB 文本 |
| `empty.txt` | 普通文件 | 0 B | 空文件 |
| `random.bin` | 普通文件 | 5 KB | 随机二进制 |
| `all_bytes.bin` | 普通文件 | 256 B | 0x00–0xFF 全覆盖 |
| `subdir/big_repeat.txt` | 普通文件 | 50 KB | 大量重复文本（压缩效果明显） |
| `subdir/deep/.hidden` | 普通文件 | 0 B | 隐藏空文件 |
| `docs/readme.txt` | 普通文件 | 161 B | 中文内容 |
| `docs/four_kb.txt` | 普通文件 | 4 KB | ~4KB 文本 |
| `subdir/` | 目录 | — | 子目录 |
| `subdir/deep/` | 目录 | — | 深层嵌套 |
| `emptydir/` | 目录 | — | 空目录 |
| `docs/` | 目录 | — | 权限 755 |
| `subdir/link_to_hello` | 符号链接 | → `../hello.txt` | 相对路径链接 |
| `subdir/deep/link_to_data` | 符号链接 | → `../../data.txt` | 深层相对链接 |
| `broken_link` | 符号链接 | → `/nonexistent/path` | 指向不存在目标的链接 |
| `myfifo` | 管道 FIFO | — | 命名管道 |

## 手动测试步骤

在项目根目录（`backup/`）下执行：

### 1. 编译

```bash
# 在项目根目录（backup/）下执行
mkdir -p build && cd build && cmake .. && make -j$(nproc) && cd ..
```
编译成功后，可执行文件位于 `build/backup`。

### 2. 测试打包（tar）

```bash
./build/backup backup tests/testdata tests/output_tar.bak --pack tar
./build/backup restore tests/output_tar.bak tests/restored_tar
diff -r tests/testdata tests/restored_tar
```

### 3. 测试打包（index）

```bash
./build/backup backup tests/testdata tests/output_idx.bak --pack index
./build/backup restore tests/output_idx.bak tests/restored_idx
diff -r tests/testdata tests/restored_idx
```

### 4. 测试打包 + 压缩（RLE）

```bash
./build/backup backup tests/testdata tests/output_rle.bak --pack tar --compress rle
# 观察：big_repeat.txt 有 50KB 重复数据，RLE 压缩效果应该很明显
ls -lh tests/output_tar.bak tests/output_rle.bak
./build/backup restore tests/output_rle.bak tests/restored_rle
diff -r tests/testdata tests/restored_rle
```

### 5. 测试打包 + 压缩（Huffman）

```bash
./build/backup backup tests/testdata tests/output_huff.bak --pack tar --compress huffman
ls -lh tests/output_tar.bak tests/output_huff.bak
./build/backup restore tests/output_huff.bak tests/restored_huff
diff -r tests/testdata tests/restored_huff
```

### 6. 测试加密（XOR）

```bash
./build/backup backup tests/testdata tests/output_xor.bak --pack tar --encrypt xor --password mypass
# 查看加密后的文件是否无法直接读懂
xxd tests/output_xor.bak | head -5
./build/backup restore tests/output_xor.bak tests/restored_xor --password mypass
diff -r tests/testdata tests/restored_xor
```

### 7. 测试加密（Vigenere）

```bash
./build/backup backup tests/testdata tests/output_vig.bak --pack tar --encrypt vigenere --password mypass
./build/backup restore tests/output_vig.bak tests/restored_vig --password mypass
diff -r tests/testdata tests/restored_vig
```

### 8. 全功能组合

```bash
./build/backup backup tests/testdata tests/output_full.bak \
    --pack index --compress huffman --encrypt xor --password s3cret

ls -lh tests/output_full.bak

# 还原时不指定算法（自动从文件头识别）
./build/backup restore tests/output_full.bak tests/restored_full --password s3cret

# 逐文件比对
diff -r tests/testdata tests/restored_full
```

### 9. 验证符号链接

```bash
readlink tests/restored_tar/subdir/link_to_hello       # 应输出 ../hello.txt
readlink tests/restored_tar/subdir/deep/link_to_data   # 应输出 ../../data.txt
readlink tests/restored_tar/broken_link               # 应输出 /nonexistent/path
```

### 10. 验证错误密码

```bash
# 用错误密码还原，数据应该损坏
./build/backup restore tests/output_xor.bak tests/restored_bad --password wrongpass
# 看内容是否已损坏：
cat tests/restored_bad/hello.txt
```

### 清理

```bash
rm -rf tests/output_*.bak tests/restored_*
```
