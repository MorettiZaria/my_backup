# 数据备份与还原系统

将目录树打包、压缩、加密为单个 `.bak` 备份文件，并可完整还原（含元数据与特殊文件）。

## 项目完成要求

- [x] **备份**：将指定目录递归扫描，打包为单一归档文件
- [x] **还原**：从备份文件完整恢复目录树，包含所有文件、目录、符号链接、管道等特殊文件
- [x] **打包算法 ×2**：tar 格式（类 POSIX TAR）+ index 格式（类 ZIP，支持随机访问）
- [x] **压缩算法 ×2**：RLE 行程编码 + Huffman 编码
- [x] **加密算法 ×2**：XOR 流密码（LCG 密钥流）+ Vigenere 多表置换
- [x] **算法自实现**：全部算法用 C++ 标准库 + POSIX 实现，零外部依赖
- [x] **策略模式**：每种功能可插拔（新增算法只需实现接口并注册）
- [x] **步骤可选**：打包/压缩/加密每一步都可独立启用或跳过，用户自由组合
- [x] **自动识别**：还原时从备份文件头部自动读取启用的算法，无需用户指定
- [x] **元数据保留**：文件权限、UID/GID、时间戳（atime/mtime/ctime）
- [x] **特殊文件支持**：符号链接、管道 (FIFO)、块设备、字符设备
- [x] **错误密码检测**：加密备份用错误密码还原时数据损坏（XOR/Vigenere 均为对称加密）
- [x] **CMake 构建**：使用 CMake 3.16+ 管理项目

## 所需环境

| 要求 | 说明 |
|------|------|
| 操作系统 | Linux（使用 POSIX API：`symlink`、`mkfifo`、`mknod`、`chmod`、`chown` 等） |
| 编译器 | GCC 8+ 或 Clang 7+（需支持 C++17） |
| 构建工具 | CMake 3.16+ |
| 依赖库 | 无（仅 C++ 标准库 + POSIX 系统调用） |
| 权限 | 还原块设备/字符设备、恢复 UID/GID 需要 root 权限（普通文件无需） |

## 编译

```bash
# 在项目根目录（backup/）下执行
mkdir -p build && cd build
cmake ..
make
```

编译成功后，可执行文件为 `build/backup`。

> **备选（直接 g++）**：
> ```bash
> g++ -std=c++17 -I include -o backup main.cpp src/core/*.cpp src/metadata/*.cpp src/pack/*.cpp src/compress/*.cpp src/encrypt/*.cpp
> ```
> 此方式生成的可执行文件在项目根目录 `./backup`。

## 运行

### 命令行格式

```
build/backup backup  <源目录>   <输出文件.bak> [选项]
build/backup restore <备份文件> <目标目录>     [选项]
```

### 选项

| 选项 | 取值 | 说明 |
|------|------|------|
| `--pack` | `tar` / `index` | 打包算法（默认 `tar`） |
| `--compress` | `rle` / `huffman` | 压缩算法（不指定 = 不压缩） |
| `--encrypt` | `xor` / `vigenere` | 加密算法（不指定 = 不加密） |
| `--password` | 任意字符串 | 加密密码（启用加密时必需） |
| `--help` | — | 显示帮助信息 |

> **注意**：还原时无需指定 `--pack` / `--compress` / `--encrypt`，程序自动从备份文件头部读取。仅当备份时用了加密，还原时才需提供 `--password`。

### 使用示例

```bash
# 只打包
./build/backup backup /home/user/documents ./docs.bak --pack tar
./build/backup restore ./docs.bak /home/user/restored

# 打包 + 压缩
./build/backup backup /home/user/documents ./docs.bak --pack index --compress rle
./build/backup restore ./docs.bak /home/user/restored

# 打包 + 加密
./build/backup backup /home/user/documents ./docs.bak --pack tar --encrypt xor --password mysecret
./build/backup restore ./docs.bak /home/user/restored --password mysecret

# 全功能：打包 + 压缩 + 加密
./build/backup backup /home/user/photos ./photos.bak \
    --pack index --compress huffman --encrypt vigenere --password s3cur3!
./build/backup restore ./photos.bak /home/user/restored --password s3cur3!
```

### 测试

项目提供了静态测试数据与手动测试步骤，详见 [`tests/README.md`](tests/README.md)。

```bash
# 快速验证（在项目根目录执行）
mkdir -p build && cd build && cmake .. && make -j$(nproc) && cd ..
./build/backup backup tests/testdata tests/output_tar.bak --pack tar
./build/backup restore tests/output_tar.bak tests/restored_tar
diff -r tests/testdata tests/restored_tar
```

## 支持的文件类型

| 类型 | 备份 | 还原 |
|------|------|------|
| 普通文件 | 内容完整保存 | ✅ `creat` + 写入 |
| 目录 | 记录结构 | ✅ `mkdir` 重建 |
| 符号链接 | 记录目标路径 | ✅ `symlink` 重建 |
| 管道 (FIFO) | 记录元数据 | ✅ `mkfifo` 重建 |
| 块设备 | 记录设备号 | ✅ `mknod` 重建（需 root） |
| 字符设备 | 记录设备号 | ✅ `mknod` 重建（需 root） |
| Unix 域套接字 | 记录元数据 | 跳过（不可跨会话重建） |

## 保留的元数据

- 文件权限 → `chmod`
- 属主 UID / 属组 GID → `chown`（需 root）
- 访问时间 / 修改时间 → `utimensat`

## 算法说明

### 打包

| 算法 | 原理 | 特点 |
|------|------|------|
| `tar` | 每个文件 512B header + 数据 + 512B 对齐填充，末尾 1024B 全零 | 格式标准，顺序读写 |
| `index` | 数据区 + 中央目录 + EOCD 尾记录（signature `IDX1`） | 支持随机访问，可快速列出内容 |

### 压缩

| 算法 | 原理 | 适用场景 |
|------|------|----------|
| `rle` | 行程编码：控制字节 bit7 区分重复块/原始块 | 大量重复数据（日志、空白填充等） |
| `huffman` | 频率统计 → 最小堆建树 → DFS 编码 → 位流输出 | 通用压缩 |

### 加密

| 算法 | 原理 | 密码 |
|------|------|------|
| `xor` | LCG 伪随机密钥流（seed 从密码派生）+ 逐字节异或 | 任意长度 |
| `vigenere` | 多表置换：`(明文 + 密钥[i % len]) mod 256` | 任意长度 |

> 所有算法均为自实现，不依赖 zlib、OpenSSL 等第三方库。

## 备份文件格式

```
字节偏移        内容
────────────────────────────────────────
0–3            MAGIC: 0x424B5550 ("BKUP")
4–7            VERSION: 1
8–11           flags (bit0=PACK, bit1=COMPRESS, bit2=ENCRYPT)
12             packAlgo (0=none, 1=tar, 2=index)
13             compressAlgo (0=none, 1=rle, 2=huffman)
14             encryptAlgo (0=none, 1=xor, 2=vigenere)
15–18          metaSize（元数据块字节数）
19–N           metaBytes（序列化的 FileInfo 列表）
N+1–EOF        payload（经 打包→压缩→加密 处理后的数据）
```

## 项目结构

```
backup/
├── CMakeLists.txt
├── README.md
├── main.cpp                          # CLI 入口
├── include/
│   ├── core/       FileInfo.h  FileScanner.h  BackupEngine.h  RestoreEngine.h
│   ├── metadata/   MetadataSerializer.h  MetadataStore.h
│   ├── pack/       IPackStrategy.h  TarPackStrategy.h  IndexPackStrategy.h  PackManager.h
│   ├── compress/   ICompressStrategy.h  RleCompressStrategy.h  HuffmanCompressStrategy.h  CompressManager.h
│   └── encrypt/    IEncryptStrategy.h  XorEncryptStrategy.h  VigenereEncryptStrategy.h  EncryptManager.h
├── src/           (与 include/ 一一对应的 .cpp 实现)
└── tests/
    ├── README.md              # 手动测试步骤（10 个场景）
    └── testdata/              # 静态测试数据（19 个条目，含特殊文件）
```
