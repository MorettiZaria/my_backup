#ifndef BACKUP_BACKUPSERVER_H
#define BACKUP_BACKUPSERVER_H

#include "network/ServerConfig.h"
#include <string>
#include <cstdint>
#include <csignal>
#include <vector>
#include <sys/types.h>

/// 备份服务器：监听端口，fork 处理每个客户端连接
class BackupServer {
public:
    /// 从配置构造
    explicit BackupServer(const ServerConfig& config);

    /// 启动服务器（阻塞，直到收到 SIGTERM/SIGINT 或调用 stop()）
    [[nodiscard]] bool start();

    /// 停止服务器（可从信号处理器中调用）
    void stop();

    /// 获取实例指针（用于信号处理器）
    static BackupServer* instancePtr;

private:
    ServerConfig config_;
    int serverFd_ = -1;
    volatile sig_atomic_t running_ = false;

    /// 活跃子进程 PID 列表（用于优雅关闭时等待）
    std::vector<pid_t> childPids_;

    /// 收割已终止子进程，并从列表中移除
    void reapChildren();

    static void sigchldHandler(int);
    static void sigtermHandler(int);
};

#endif // BACKUP_BACKUPSERVER_H
