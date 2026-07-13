# backup — 命令行使用说明

## 命令格式

```
# 单机模式
backup backup  <源目录>   <输出文件.bak> [选项]
backup restore <备份文件> <目标目录>     [选项]

# 网络模式（服务器）
backup server start --port <端口> --storage <存储目录>

# 网络模式（客户端）
backup remote-backup  <源目录>   --server <host:port> --username <用户名> --password <密码> [选项]
backup remote-restore <目标目录> --server <host:port> --username <用户名> --password <密码> [选项]
backup remote-list              --server <host:port> --username <用户名> --password <密码>

# 用户管理
backup user register --server <host:port> --username <用户名> --password <密码>
backup user login    --server <host:port> --username <用户名> --password <密码>
```

## 选项

### 单机模式

| 选项 | 取值 | 说明 |
|------|------|------|
| `--pack` | `tar` / `index` | 打包算法（默认 `tar`） |
| `--compress` | `rle` / `huffman` | 压缩算法（不指定 = 不压缩） |
| `--encrypt` | `xor` / `vigenere` | 加密算法（不指定 = 不加密） |
| `--password` | 字符串 | 文件加密密码（启用加密时必需） |

### 网络模式

| 选项 | 取值 | 说明 |
|------|------|------|
| `--server` | `host:port` | 服务器地址（必需） |
| `--username` | 字符串 | 登录用户名（必需） |
| `--password` | 字符串 | 服务器登录密码（必需） |
| `--file-password` | 字符串 | 文件加密密码（对应 `--password`） |
| `--pack` | `tar` / `index` | 打包算法（默认 `tar`） |
| `--compress` | `rle` / `huffman` | 压缩算法 |
| `--encrypt` | `xor` / `vigenere` | 加密算法 |
| `--backup-name` | 字符串 | 自定义备份名称（留空则自动生成 ID） |
| `--backup-id` | 字符串 | 还原指定备份 ID（默认 = 最新） |

## 使用示例

### 单机模式

```bash
# 只打包
backup backup /home/user/documents ./docs.bak --pack tar
backup restore ./docs.bak /home/user/restored

# 打包 + 压缩
backup backup /home/user/documents ./docs.bak --pack index --compress rle

# 打包 + 加密
backup backup /home/user/documents ./docs.bak --pack tar --encrypt xor --password mysecret
backup restore ./docs.bak /home/user/restored --password mysecret

# 全功能：打包 + 压缩 + 加密
backup backup /home/user/photos ./photos.bak \
    --pack index --compress huffman --encrypt vigenere --password s3cur3!
```

### 网络模式

```bash
# 启动服务器
backup-server --port 8848 --storage ./server_data

# 注册用户
backup user register --server 127.0.0.1:8848 --username zaria --password mypass

# 远程备份（自动生成 ID）
backup remote-backup /home/user/documents \
    --server 192.168.1.100:8848 --username zaria --password mypass --pack tar

# 远程备份（自定义备份名称）
backup remote-backup /home/user/documents \
    --server 192.168.1.100:8848 --username zaria --password mypass \
    --backup-name "文档备份_202407" --pack tar

# 远程还原
backup remote-restore /home/user/restored \
    --server 192.168.1.100:8848 --username zaria --password mypass

# 列出远程备份
backup remote-list --server 192.168.1.100:8848 --username zaria --password mypass

# 远程备份 + 文件加密
backup remote-backup /home/user/photos \
    --server 192.168.1.100:8848 --username zaria --password mypass \
    --pack index --compress huffman --encrypt xor --file-password s3cur3!
```

> 注意区分两个密码：`--password` 是服务器登录密码，`--file-password` 是 `.bak` 文件加解密密钥。

## 备份文件格式

```
字节偏移        内容
────────────────────────────────────────
0–3            MAGIC: 0x424B5550 ("BKUP")
4–7            VERSION
8–11           flags (bit0=PACK, bit1=COMPRESS, bit2=ENCRYPT)
12             packAlgo
13             compressAlgo
14             encryptAlgo
15–18          metaSize
19–N           metaBytes
N+1–EOF        payload
```

还原时无需指定算法参数，程序自动从文件头读取。
