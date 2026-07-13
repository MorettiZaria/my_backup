# my_backup — 数据备份与还原系统

将目录树打包、压缩、加密为单个 `.bak` 备份文件，支持**单机模式**和**客户端-服务器网络模式**。
无需任何外部依赖即可运行（GUI 除外）。

## 环境要求

| 要求 | 说明 |
|------|------|
| 操作系统 | Linux（使用 POSIX API） |
| 编译器 | GCC 8+ 或 Clang 7+（需支持 C++17） |
| 构建工具 | CMake 3.16+ |
| GUI 依赖 | Qt6 Widgets（可选，不影响命令行构建） |

## 编译

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

编译产物：

| 文件 | 说明 |
|------|------|
| `build/client/backup` | CLI 命令行工具（全部功能：本地/远程备份还原、用户管理、服务器启动） |
| `build/server/backup-server` | 专用服务器守护进程 |
| `build/gui/backup-gui` | Qt6 图形界面（需 `sudo apt install qt6-base-dev`） |

> 未安装 Qt6 时，GUI 目标自动跳过，CLI 和服务器照常编译。

## 启动 GUI

```bash
build/gui/backup-gui
```

界面包含 **6 个选项卡**：

| 选项卡 | 功能 |
|--------|------|
| 本地备份 | 选择目录 → 打包/压缩/加密 → 生成 `.bak` |
| 本地还原 | 选择 `.bak` → 还原到目标目录 |
| 远程备份 | 填写服务器地址和账号，自定义备份名称 → 备份到服务器 |
| 远程还原 | 从服务器下载备份 → 还原到本地 |
| 远程列表 | 查看服务器上的备份历史（含备份名称和 ID） |
| 用户管理 | 在远程服务器上注册新用户 |

## 启动服务器

```bash
# 前台运行
build/server/backup-server --port 8848 --storage ./server_data
```

## 命令行使用

完整命令参考见 [client/README.md](client/README.md)。

## 项目结构

```
my_backup/
├── CMakeLists.txt          # 顶层构建（一键编译所有目标）
├── libbackup/              # 核心算法库（打包/压缩/加密/网络协议/用户管理）
│   ├── include/            #   头文件：core/ pack/ compress/ encrypt/ metadata/ network/
│   └── src/                #   对应 .cpp 实现
├── client/                 # CLI 客户端 (backup)
│   └── src/main.cpp        #   所有命令行功能入口
├── server/                 # 服务器守护进程 (backup-server)
│   └── src/main.cpp        #   轻量级服务器入口
├── gui/                    # Qt6 图形界面 (backup-gui)
│   ├── include/            #   头文件：tabs/ workers/
│   └── src/                #   源文件
├── tests/                  # 测试数据和脚本
├── deploy/                 # systemd 服务 + 配置模板
└── doc/                    # 文档
```

## 技术特性

- **打包算法**: tar、index（自实现，零依赖）
- **压缩算法**: RLE、Huffman（自实现）
- **加密算法**: XOR、Vigenère（自实现）
- **网络协议**: 自定义二进制协议，8 字节头部 + 可变载荷，大端序
- **服务器并发**: `fork()` 每连接一个子进程
- **认证**: salt + SHA-256 哈希密码存储
- **传输加密**: XOR LCG 流密码，每消息独立 IV

## 部署到云服务器

详见 [doc/ECS_DEPLOY.md](doc/ECS_DEPLOY.md)。

## License

MIT License — 详见 [LICENSE](LICENSE)
