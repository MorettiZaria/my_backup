# CLAUDE.md — my_backup 项目指南

## 项目概述

C++17 数据备份与还原系统，支持 CLI + Qt6 GUI + 客户端-服务器网络架构。
所有算法（打包/压缩/加密）自实现，零外部依赖（GUI 除外）。

## 目录结构

```
my_backup/
├── CMakeLists.txt          # 顶层构建（一键编译所有目标）
├── libbackup/              # 共享核心静态库
│   ├── include/            #   头文件：core/ pack/ compress/ encrypt/ metadata/ network/
│   └── src/                #   对应 .cpp 实现
├── client/                 # CLI 客户端 (backup)
│   └── src/main.cpp        #   所有命令行功能入口
├── server/                 # 服务器守护进程 (backup-server)
│   └── src/main.cpp        #   轻量级服务器入口
├── gui/                    # Qt6 图形界面 (backup-gui)
│   ├── include/            #   GUI 头文件：tabs/ workers/
│   └── src/                #   GUI 源文件
├── tests/                  # 测试数据和脚本
├── deploy/                 # systemd 服务 + 配置模板
└── doc/                    # 文档
```

## 构建

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

构建产物：
- `build/client/backup` — CLI 工具（所有功能）
- `build/server/backup-server` — 服务器守护进程
- `build/gui/backup-gui` — Qt6 GUI（需安装 Qt6）

## 测试

```bash
# 快速验证
./build/client/backup backup tests/testdata /tmp/test.bak --pack tar
./build/client/backup restore /tmp/test.bak /tmp/restored
diff -r tests/testdata /tmp/restored/testdata

# 完整自动化测试
python3 tests/run_tests.py
```

## 编码约定

- **C++ 标准**: C++17
- **缩进**: 4 空格
- **类名**: PascalCase (`BackupEngine`, `NetworkSocket`)
- **方法/变量**: camelCase (`restoreMetadata`, `packStrategy_`)
- **成员变量后缀**: `_` (下划线)
- **Include guard**: `BACKUP_XXX_H` (libbackup), `GUI_XXX_H` (gui)
- **策略模式**: 所有算法通过抽象接口 (`IPackStrategy`, `ICompressStrategy`, `IEncryptStrategy`) + Manager 类注册/选择

## 架构原则

1. **`libbackup`** 是纯算法库，不含 `main()`，被所有目标链接
2. **网络协议**: 自定义二进制协议，8 字节头部 + 可变载荷，大端序
3. **服务器并发**: `fork()` 每连接一个子进程
4. **认证**: salt + SHA-256 哈希密码存储
5. **传输加密**: XOR LCG 流密码，每消息独立 IV（序列号）
