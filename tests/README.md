# 测试指南

## 自动化测试（推荐）

```bash
# 在项目根目录执行
python3 tests/run_tests.py
```

脚本会自动完成编译（如未编译）、运行全部测试用例（单机 + 网络），并生成 `tests/test_report.md` 测试报告。

测试覆盖：
- 单机模式：tar/index 打包、RLE/Huffman 压缩、XOR/Vigenere 加密、全功能组合
- 网络模式（启用`127.0.0.1`，即以本地机器为服务端口）：用户注册/登录、远程备份/还原、远程备份列表、加密传输
- 健壮性：错误密码、重复注册、非法参数、连接拒绝
- 性能：压缩率对比

## 手动测试

### 测试数据

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

---

## 第一部分：单机模式测试

在项目根目录（`backup/`）下执行：

### 1. 编译

```bash
mkdir -p build && cd build && cmake .. && make -j$(nproc) && cd ..
```
编译成功后，可执行文件位于 `build/client/backup`。

### 2. 测试打包（tar）

```bash
./build/client/backup backup tests/testdata tests/output_tar.bak --pack tar
./build/client/backup restore tests/output_tar.bak tests/restored_tar
diff -r tests/testdata tests/restored_tar
```

### 3. 测试打包（index）

```bash
./build/client/backup backup tests/testdata tests/output_idx.bak --pack index
./build/client/backup restore tests/output_idx.bak tests/restored_idx
diff -r tests/testdata tests/restored_idx
```

### 4. 测试打包 + 压缩（RLE）

```bash
./build/client/backup backup tests/testdata tests/output_rle.bak --pack tar --compress rle
# 观察：big_repeat.txt 有 50KB 重复数据，RLE 压缩效果应该很明显
ls -lh tests/output_tar.bak tests/output_rle.bak
./build/client/backup restore tests/output_rle.bak tests/restored_rle
diff -r tests/testdata tests/restored_rle
```

### 5. 测试打包 + 压缩（Huffman）

```bash
./build/client/backup backup tests/testdata tests/output_huff.bak --pack tar --compress huffman
ls -lh tests/output_tar.bak tests/output_huff.bak
./build/client/backup restore tests/output_huff.bak tests/restored_huff
diff -r tests/testdata tests/restored_huff
```

### 6. 测试加密（XOR）

```bash
./build/client/backup backup tests/testdata tests/output_xor.bak --pack tar --encrypt xor --password mypass
# 查看加密后的文件是否无法直接读懂
xxd tests/output_xor.bak | head -5
./build/client/backup restore tests/output_xor.bak tests/restored_xor --password mypass
diff -r tests/testdata tests/restored_xor
```

### 7. 测试加密（Vigenere）

```bash
./build/client/backup backup tests/testdata tests/output_vig.bak --pack tar --encrypt vigenere --password mypass
./build/client/backup restore tests/output_vig.bak tests/restored_vig --password mypass
diff -r tests/testdata tests/restored_vig
```

### 8. 全功能组合

```bash
./build/client/backup backup tests/testdata tests/output_full.bak \
    --pack index --compress huffman --encrypt xor --password s3cret

ls -lh tests/output_full.bak

# 还原时不指定算法（自动从文件头识别）
./build/client/backup restore tests/output_full.bak tests/restored_full --password s3cret

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
./build/client/backup restore tests/output_xor.bak tests/restored_bad --password wrongpass
# 看内容是否已损坏：
cat tests/restored_bad/hello.txt
```

---

## 第二部分：网络模式（网盘模式）测试

网络模式测试中使用`127.0.0.1`即为以本地机器为服务端，如果有服务器可做服务端，IP配置为服务器的IP即可。

### 前置：编译

```bash
mkdir -p build && cd build && cmake .. && make -j$(nproc) && cd ..
```

### 测试 N1：服务器启动

```bash
# 启动服务器（后台运行）
# 服务器绑定 0.0.0.0:18848，允许来自任何 IP 的连接
./build/client/backup server start --port 18848 --storage /tmp/test_server_data &
SERVER_PID=$!
sleep 1

# 验证服务器在运行
kill -0 $SERVER_PID && echo "Server is running" || echo "Server failed to start"
```

### 测试 N2：用户注册与登录

```bash
# 注册新用户
./build/client/backup user register \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass
# 预期: User registered successfully.

# 重复注册（应失败）
./build/client/backup user register \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass
# 预期: Error: registration failed (user may already exist).

# 正确密码登录
./build/client/backup user login \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass
# 预期: Login successful.

# 错误密码登录（应失败）
./build/client/backup user login \
    --server 127.0.0.1:18848 \
    --username testuser --password wrongpass
# 预期: Error: login failed (wrong password?).
```

### 测试 N3：远程备份

```bash
# 远程备份（打包 = tar）
./build/client/backup remote-backup tests/testdata \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass \
    --pack tar
# 预期: Backup complete! ID: backup_000001

# 查看服务器上的文件
ls -la /tmp/test_server_data/testuser/backup_000001/
# 预期: header.bin  payload.bin  metadata.bin
```

### 测试 N4：远程还原

```bash
# 远程还原
./build/client/backup remote-restore /tmp/test_restore_remote \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass

# 数据完整性验证
diff -r tests/testdata /tmp/test_restore_remote
# 预期: 无差异（特殊文件如 FIFO 会提示类型差异，属于 diff 工具的正常行为）

# 验证普通文件内容
cat /tmp/test_restore_remote/hello.txt
# 预期: Hello World!

# 验证符号链接
readlink /tmp/test_restore_remote/subdir/link_to_hello
# 预期: ../hello.txt

# 验证管道
test -p /tmp/test_restore_remote/myfifo && echo "FIFO OK"
# 预期: FIFO OK

# 验证文件权限
stat -c '%a' /tmp/test_restore_remote/data.txt
# 预期: 600
```

### 测试 N5：列出备份

```bash
./build/client/backup remote-list \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass
# 预期: 显示备份列表（含 backup ID 和时间戳）
```

### 测试 N6：远程备份 + 压缩 + 文件加密

```bash
# 全功能远程备份
./build/client/backup remote-backup tests/testdata \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass \
    --pack index --compress huffman \
    --encrypt xor --file-password filepass

# 检查服务器端存储的是加密数据
xxd /tmp/test_server_data/testuser/backup_000002/payload.bin | head -3
# 预期: 不可读的二进制数据

# 正确密码还原
./build/client/backup remote-restore /tmp/test_restore_enc \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass \
    --file-password filepass
    
diff -r tests/testdata /tmp/test_restore_enc
# 预期: 无差异

# 错误文件密码还原（数据应损坏）
./build/client/backup remote-restore /tmp/test_restore_bad \
    --server 127.0.0.1:18848 \
    --username testuser --password testpass \
    --file-password wrongpass
cat /tmp/test_restore_bad/hello.txt
# 预期: 乱码（XOR 加密 + 错误密码 = 无法正确解密）
```

### 测试 N7：错误处理

```bash
# 连接不存在用户（用户未注册就直接备份）
./build/client/backup remote-backup tests/testdata \
    --server 127.0.0.1:18848 \
    --username nonexist --password testpass \
    --pack tar
# 预期: Login failed, trying to register... (注册成功则继续备份)

# 连接不存在的服务器
./build/client/backup remote-list \
    --server 127.0.0.1:19999 \
    --username testuser --password testpass
# 预期: Connection refused.

# 未备份过的用户尝试还原
./build/client/backup user register \
    --server 127.0.0.1:18848 \
    --username newuser --password newpass
./build/client/backup remote-restore /tmp/test_restore_empty \
    --server 127.0.0.1:18848 \
    --username newuser --password newpass
# 预期: No backup found.
```

> **故障排除**：
> - `Connection refused` → 检查服务器防火墙：`sudo ufw allow 8848/tcp`
> - `No route to host` → 检查两台电脑是否在同一网段
> - `Connection timed out` → 检查路由器是否开启了 AP 隔离（客户端隔离）功能

---

## 清理

```bash
# 停止服务器
kill $(pgrep -f "backup server") 2>/dev/null

# 清理单机模式测试文件
rm -rf tests/output_*.bak tests/restored_*

# 清理网络模式测试文件
rm -rf /tmp/test_server_data /tmp/test_server_data2
rm -rf /tmp/test_restore_remote /tmp/test_restore_enc /tmp/test_restore_bad /tmp/test_restore_empty
```
