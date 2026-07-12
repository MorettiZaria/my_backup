# my_backup — 数据备份与还原系统

将目录树打包、压缩、加密为单个 `.bak` 备份文件，支持**单机模式**和**网络（网盘）模式**。

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
| `build/client/backup` | 命令行工具（备份/还原/远程/用户管理/服务器启动） |
| `build/server/backup-server` | 专用服务器守护进程 |
| `build/gui/backup-gui` | Qt6 图形界面（需 `sudo apt install qt6-base-dev`） |

## 启动 GUI

```bash
build/gui/backup-gui
```

界面包含 5 个选项卡：本地备份、本地还原、远程备份、远程还原、远程列表。

## 启动服务器

```bash
# 前台运行
build/server/backup-server --port 8848 --storage ./server_data

# 或使用 systemd（安装后）
sudo make install
sudo systemctl start backup-server
```

## 命令行使用

详细命令参数见 [client/README.md](client/README.md)。

## 项目结构

```
my_backup/
├── CMakeLists.txt          # 一键构建
├── libbackup/              # 核心算法库（打包/压缩/加密/网络）
├── client/                 # CLI 客户端
├── server/                 # 服务器守护进程
├── gui/                    # Qt6 图形界面
├── tests/                  # 测试
└── deploy/                 # 部署配置
```
> `doc/` 为本地文档目录，不上传至 GitHub。

## License

MIT License — 详见 [LICENSE](LICENSE)
