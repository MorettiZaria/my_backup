# ECS 云服务器部署指南

本文档说明如何在阿里云 ECS 上将 Backup Server 部署为系统服务。

## 前置条件

- 阿里云 ECS 实例（推荐 Ubuntu 20.04/22.04）
- 已获取 ECS 公网 IP
- 能够 SSH 登录 ECS（root 或有 sudo 权限）

## 步骤一：安全组配置（关键）

ECS 默认不开放入方向端口，需手动添加：

1. 登录 [阿里云控制台](https://ecs.console.aliyun.com/) → **安全组**
2. 入方向 → 手动添加：

| 授权策略 | 协议 | 端口 | 授权对象 |
|----------|------|------|----------|
| 允许 | TCP | 8848 | 0.0.0.0/0 |

> 建议将 `0.0.0.0/0` 改为固定客户端 IP 以提升安全性。

## 步骤二：上传项目

```bash
# 在本地执行
scp -r /path/to/my_backup root@<ECS公网IP>:/root/
```

## 步骤三：编译 & 部署（一条命令）

```bash
ssh root@<ECS公网IP>
cd /root/my_backup

# 安装构建依赖（仅首次）
apt-get install -y cmake g++

# 编译 + 安装（一步完成）
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
sudo make install   # 等价于 cmake --install .
```

`sudo make install` 自动完成：
- 安装二进制到 `/usr/local/bin/backup`
- 写入配置模板 `/etc/backup-server.conf`
- 安装 systemd 服务 `/etc/systemd/system/backup-server.service`
- 创建 `backup` 系统用户
- 创建 `/var/lib/backup-server/data` 数据目录
- `systemctl daemon-reload` + `systemctl enable backup-server`

## 步骤四：修改配置（可选）

```bash
sudo vim /etc/backup-server.conf
```

## 步骤五：启动服务

```bash
sudo systemctl start backup-server
sudo systemctl status backup-server
```

## 客户端连接

使用 ECS **公网 IP**：

```bash
# 注册
backup user register --server <公网IP>:8848 --username myname --password mypass

# 远程备份
backup remote-backup /path/to/data --server <公网IP>:8848 --username myname --password mypass --pack tar

# 远程还原
backup remote-restore /path/to/restore --server <公网IP>:8848 --username myname --password mypass
```

## 运维命令

```bash
systemctl start|stop|restart backup-server   # 启停
systemctl status backup-server               # 状态
journalctl -u backup-server -f               # 实时日志
tail -f /var/log/backup-server.log           # 文件日志
```

## 卸载

```bash
sudo systemctl stop backup-server
sudo systemctl disable backup-server
sudo rm /usr/local/bin/backup
sudo rm /etc/backup-server.conf
sudo rm /etc/systemd/system/backup-server.service
sudo rm -rf /var/lib/backup-server
```

## 故障排查

### 客户端无法连接

1. 安全组是否开放 8848 端口
2. 服务是否运行：`systemctl status backup-server`
3. 监听地址：`ss -tlnp | grep 8848`（应显示 `0.0.0.0:8848`）
4. ECS 内部防火墙：`sudo ufw allow 8848/tcp`

### 服务无法启动

```bash
# 前台运行查看错误
sudo -u backup /usr/local/bin/backup server start --config /etc/backup-server.conf
```
