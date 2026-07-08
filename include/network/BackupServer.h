#ifndef BACKUP_BACKUPSERVER_H
#define BACKUP_BACKUPSERVER_H

#include <string>
#include <cstdint>

/// 备份服务器：监听端口，fork 处理每个客户端连接
class BackupServer {
public:
    BackupServer(uint16_t port, const std::string& storagePath);

    /// 启动服务器（阻塞，直到 stop() 被调用或收到信号）
    bool start();

    /// 停止服务器
    void stop();

private:
    uint16_t port_;
    std::string storagePath_;
    int serverFd_ = -1;
    volatile bool running_ = false;

    static void sigchldHandler(int);
};

#endif // BACKUP_BACKUPSERVER_H
