# 数据备份与还原系统

将目录树打包、压缩、加密为单个 `.bak` 备份文件，并可完整还原（含元数据与特殊文件）。支持**单机模式**和**网络（网盘）模式**。

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
- [x] **错误密码检测**：加密备份用错误密码还原时提示错误，不会崩溃
- [x] **CMake 构建**：使用 CMake 3.16+ 管理项目
- [x] **GUI 图形界面**：基于 Qt6 的图形化备份/还原/远程管理界面
- [x] **网络备份（网盘模式）**：客户端-服务器架构，支持远程备份/还原
- [x] **用户管理**：注册、登录、salt+hash 密码认证
- [x] **元数据管理**：服务器端记录每次备份的文件列表，支持备份历史查看
- [x] **传输加密**：基于 XOR LCG 流密码的网络传输加密
- [x] **跨机器通信**：服务器绑定 `0.0.0.0`，同一局域网/互联网的任意客户端均可连接

## 所需环境

| 要求 | 说明 |
|------|------|
| 操作系统 | Linux（使用 POSIX API：`symlink`、`mkfifo`、`mknod`、`chmod`、`chown` 等） |
| 编译器 | GCC 8+ 或 Clang 7+（需支持 C++17） |
| 构建工具 | CMake 3.16+ |
| 依赖库 | CLI: 无（仅 C++ 标准库 + POSIX 系统调用）<br>GUI: Qt6 Widgets（可选，不影响 CLI 构建） |
| 权限 | 还原块设备/字符设备、恢复 UID/GID 需要 root 权限（普通文件无需） |

## 编译

```bash
# 在项目根目录（backup/）下执行
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

编译成功后，可执行文件为 `build/backup`。如果系统安装了 Qt6，还会额外生成 `build/backup-gui`。

> **备选（直接 g++）**：
> ```bash
> g++ -std=c++17 -I include -o backup main.cpp src/core/*.cpp src/metadata/*.cpp src/pack/*.cpp src/compress/*.cpp src/encrypt/*.cpp src/network/*.cpp
> ```
> 此方式生成的可执行文件在项目根目录 `./backup`。

## 运行

### 命令行格式

```
# 单机模式
build/backup backup  <源目录>   <输出文件.bak> [选项]
build/backup restore <备份文件> <目标目录>     [选项]

# 网络模式（服务器）
build/backup server start --port <端口> --storage <存储目录>

# 网络模式（客户端）
build/backup remote-backup  <源目录>   --server <host:port> --username <用户名> --password <密码> [选项]
build/backup remote-restore <目标目录> --server <host:port> --username <用户名> --password <密码> [选项]
build/backup remote-list              --server <host:port> --username <用户名> --password <密码>

# 用户管理
build/backup user register --server <host:port> --username <用户名> --password <密码>
build/backup user login    --server <host:port> --username <用户名> --password <密码>
```

### 单机模式选项

| 选项 | 取值 | 说明 |
|------|------|------|
| `--pack` | `tar` / `index` | 打包算法（默认 `tar`） |
| `--compress` | `rle` / `huffman` | 压缩算法（不指定 = 不压缩） |
| `--encrypt` | `xor` / `vigenere` | 加密算法（不指定 = 不加密） |
| `--password` | 任意字符串 | 加密密码（启用加密时必需） |
| `--help` | — | 显示帮助信息 |

> **注意**：还原时无需指定 `--pack` / `--compress` / `--encrypt`，程序自动从备份文件头部读取。仅当备份时用了加密，还原时才需提供 `--password`。

### 网络模式选项

| 选项 | 取值 | 说明 |
|------|------|------|
| `--port` | 数字 | 服务器监听端口（默认 `8848`） |
| `--storage` | 路径 | 服务器数据存储目录（默认 `./server_data`） |
| `--server` | `host:port` | 服务器地址（必需） |
| `--username` | 字符串 | 用户名（必需） |
| `--password` | 字符串 | 服务器登录密码（必需） |
| `--file-password` | 字符串 | 文件加密密码（对应单机模式的 `--password`） |
| `--pack` | `tar` / `index` | 打包算法（远程备份时默认 `tar`） |
| `--compress` | `rle` / `huffman` | 压缩算法（不指定 = 不压缩） |
| `--encrypt` | `xor` / `vigenere` | 加密算法（不指定 = 不加密） |
| `--backup-id` | 字符串 | 还原指定备份 ID（默认 = 最新备份） |

> **注意区分两个密码**：`--password` 是服务器登录密码，`--file-password` 是 `.bak` 文件加解密密钥。

### 使用示例

#### 单机模式

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

#### 网络模式（网盘模式）

```bash
# === 启动服务器（在服务器电脑上执行） ===
./build/backup server start --port 8848 --storage ./server_data
# 输出: Listening on 0.0.0.0:8848

# === 用户注册（在任意客户端电脑上执行） ===
./build/backup user register \
    --server 192.168.1.100:8848 \
    --username zaria --password mypass
# 输出: User registered successfully.

# === 远程备份 ===
./build/backup remote-backup /home/user/documents \
    --server 192.168.1.100:8848 \
    --username zaria --password mypass \
    --pack tar
# 输出: Backup complete! ID: backup_000001

# === 列出远程备份 ===
./build/backup remote-list \
    --server 192.168.1.100:8848 \
    --username zaria --password mypass

# === 远程还原 ===
./build/backup remote-restore /home/user/restored \
    --server 192.168.1.100:8848 \
    --username zaria --password mypass

# === 远程备份 + 文件加密 ===
./build/backup remote-backup /home/user/photos \
    --server 192.168.1.100:8848 \
    --username zaria --password mypass \
    --pack index --compress huffman --encrypt xor --file-password s3cur3!

# === 远程还原（需提供文件密码） ===
./build/backup remote-restore /home/user/restored \
    --server 192.168.1.100:8848 \
    --username zaria --password mypass \
    --file-password s3cur3!
```

> **跨机器使用**：服务器绑定 `0.0.0.0`，同一 WiFi/局域网下的其他电脑将 `--server` 中的 IP 替换为服务器的局域网 IP（用 `ip addr` 查看）即可连接。如果连接被拒绝，检查防火墙（`sudo ufw allow 8848`）和路由器 AP 隔离设置。

## GUI 图形界面

如果系统安装了 Qt6（`sudo apt install qt6-base-dev`），CMake 会自动检测并构建 GUI 目标。

### 启动 GUI

```bash
# 从 build 目录启动
cd build
./backup-gui
```

### GUI 功能概览

界面包含 5 个选项卡：

| 选项卡 | 功能 |
|--------|------|
| 📦 本地备份 | 选择源目录 → 设置保存目录和文件名 → 选打包/压缩/加密 → 生成 `.bak` |
| 📂 本地还原 | 选择 `.bak` 文件 → 选目标目录 → 输入密码 → 还原 |
| ☁ 远程备份 | 连接服务器 → 选择源目录 → 备份到远程网盘 |
| 📥 远程还原 | 连接服务器 → 选目标目录 → 从远程网盘还原 |
| 📋 远程列表 | 连接服务器 → 查看服务器上的备份历史 |

### GUI 特有安全机制

- **输出文件冲突检测**：备份时若 `.bak` 文件已存在，弹窗阻止并提示更换文件名
- **还原冲突检测**：还原前自动读取备份文件元数据，检查目标目录中是否有同名文件，有冲突时弹窗列出并阻止
- **错误密码保护**：加密备份输错密码不会崩溃，提示「可能原因：密码错误、文件损坏或格式不匹配」
- **顶级目录保留**：备份 `/path/to/folder` 后，还原时会自动创建 `folder/` 目录，而非散落文件
- **文件名自动补全**：输入文件名时若未写 `.bak` 后缀，自动补上

## 网络架构

```
┌──────────────────────────┐        TCP/8848       ┌──────────────────────────┐
│   客户端 (任意电脑)         │ ◄──────────────────► │   服务器 (备份存储电脑)      │
│                           │   自定义二进制协议      │                           │
│   backup remote-backup    │                       │   backup server start     │
│   backup remote-restore   │   传输加密 (XOR LCG)    │   绑定 0.0.0.0:8848       │
│   backup remote-list      │                       │   fork() 多进程并发        │
│   backup user register    │   salt+hash 认证       │   users.db 用户管理        │
└──────────────────────────┘                       └──────────────────────────┘
```

**服务器存储结构**：
```
server_data/
├── users.db                    # 用户数据库（salt + hash）
└── <username>/
    └── backup_000001/
        ├── header.bin          # .bak 文件头部
        ├── payload.bin         # .bak 载荷（打包/压缩/加密后）
        └── metadata.bin        # 文件元数据列表（备份历史记录）
```

### 测试

项目提供了静态测试数据与手动测试步骤，详见 [`tests/README.md`](tests/README.md)。

```bash
# 快速验证单机模式（在项目根目录执行）
mkdir -p build && cd build && cmake .. && make -j$(nproc) && cd ..
./build/backup backup tests/testdata tests/output_tar.bak --pack tar
./build/backup restore tests/output_tar.bak tests/restored_tar
diff -r tests/testdata tests/restored_tar

# 快速验证网络模式
./build/backup server start --port 8848 --storage /tmp/test_server &
./build/backup user register --server 127.0.0.1:8848 --username test --password test
./build/backup remote-backup tests/testdata --server 127.0.0.1:8848 --username test --password test --pack tar
./build/backup remote-restore /tmp/test_restore --server 127.0.0.1:8848 --username test --password test
diff -r tests/testdata /tmp/test_restore
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

### 传输加密

| 算法 | 原理 | 范围 |
|------|------|------|
| XOR LCG 流密码 | 从用户密码 + 服务器 salt 派生会话密钥，每消息独立 IV（序列号） | 网络传输层 |

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

## 网络协议

### 消息格式

```
字节偏移        内容
────────────────────────────────────────
0–1            type (消息类型，大端序 uint16)
2–3            reserved (保留)
4–7            payloadLen (载荷长度，大端序 uint32)
8–N            payload (载荷)
```

### 认证流程

```
客户端                         服务器
  │──── CLIENT_HELLO ─────────►│
  │◄─── SERVER_HELLO ──────────│  (含 8 字节随机挑战码)
  │                            │
  │──── LOGIN_REQUEST ────────►│  (仅用户名)
  │◄─── LOGIN_SALT ────────────│  (返回注册时的盐值)
  │──── LOGIN_PROOF ──────────►│  (computeHash(password, salt))
  │◄─── LOGIN_RESPONSE ────────│  (OK / 密码错误)
  │                            │
  │==== 后续通信（传输加密）=====│
```

## 项目结构

```
backup/
├── CMakeLists.txt
├── README.md
├── main.cpp                          # CLI 入口（单机 + 网络）
├── include/
│   ├── core/       FileInfo.h  FileScanner.h  BackupEngine.h  RestoreEngine.h
│   ├── metadata/   MetadataSerializer.h  MetadataStore.h
│   ├── pack/       IPackStrategy.h  TarPackStrategy.h  IndexPackStrategy.h  PackManager.h
│   ├── compress/   ICompressStrategy.h  RleCompressStrategy.h  HuffmanCompressStrategy.h  CompressManager.h
│   ├── encrypt/    IEncryptStrategy.h  XorEncryptStrategy.h  VigenereEncryptStrategy.h  EncryptManager.h
│   └── network/    NetworkProtocol.h  NetworkSocket.h  UserManager.h  ServerStorage.h
│                   TransportEncryptor.h  ServerSession.h  BackupServer.h
│                   NetworkBackupClient.h  NetworkRestoreClient.h
├── src/           (与 include/ 一一对应的 .cpp 实现)
├── gui/                     # Qt6 图形界面
│   ├── main.cpp              # GUI 入口
│   ├── MainWindow.h/.cpp     # 主窗口（选项卡容器）
│   ├── StrategySetup.h/.cpp  # 策略注册工厂
│   ├── tabs/                 # 各功能选项卡
│   │   ├── LocalBackupTab    # 本地备份
│   │   ├── LocalRestoreTab   # 本地还原
│   │   ├── RemoteBackupTab   # 远程备份
│   │   ├── RemoteRestoreTab  # 远程还原
│   │   └── RemoteListTab     # 远程列表
│   └── workers/              # 后台工作线程（避免阻塞 UI）
│       ├── BackupWorker      # 本地备份
│       ├── RestoreWorker     # 本地还原
│       ├── RemoteBackupWorker
│       ├── RemoteRestoreWorker
│       └── RemoteListWorker
└── tests/
    ├── README.md              # 手动测试步骤（含网络功能测试）
    └── testdata/              # 静态测试数据（19 个条目，含特殊文件）
```
