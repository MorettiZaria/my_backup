#include "network/BackupServer.h"
#include "network/ServerSession.h"
#include "network/Logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstring>
#include <algorithm>

BackupServer* BackupServer::instancePtr = nullptr;

BackupServer::BackupServer(const ServerConfig& config)
    : config_(config) {}

void BackupServer::sigchldHandler(int) {
    // 仅标记，实际收割由 reapChildren() 完成
    // 不做 waitpid，避免与 reapChildren() 竞争
}

void BackupServer::sigtermHandler(int) {
    if (instancePtr) {
        instancePtr->stop();
    }
}

void BackupServer::reapChildren() {
    // 非阻塞收割已终止子进程
    while (true) {
        pid_t pid = waitpid(-1, nullptr, WNOHANG);
        if (pid <= 0) break;
        auto it = std::find(childPids_.begin(), childPids_.end(), pid);
        if (it != childPids_.end()) {
            childPids_.erase(it);
        }
    }
}

bool BackupServer::start() {
    instancePtr = this;
    auto& log = Logger::instance();

    // ---- 安装信号处理 ----
    struct sigaction sa{};

    // SIGCHLD: 收割子进程
    sa.sa_handler = sigchldHandler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    // SIGTERM / SIGINT: 优雅关闭
    sa.sa_handler = sigtermHandler;
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    // 忽略 SIGPIPE（防止写入已关闭的 socket 导致进程终止）
    signal(SIGPIPE, SIG_IGN);

    // ---- 创建 socket ----
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        log.error("socket() failed: " + std::string(std::strerror(errno)));
        return false;
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(config_.port());

    if (bind(serverFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        log.error("bind() failed: " + std::string(std::strerror(errno)));
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }

    if (listen(serverFd_, std::min(config_.maxConnections(), SOMAXCONN)) < 0) {
        log.error("listen() failed: " + std::string(std::strerror(errno)));
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }

    log.info("=== Backup Server ===");
    log.info("Listening on 0.0.0.0:" + std::to_string(config_.port()));
    log.info("Storage: " + config_.storagePath());
    if (!config_.logFile().empty()) {
        log.info("Log file: " + config_.logFile());
    }
    log.info("Press Ctrl+C to stop.");

    running_ = true;

    while (running_) {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = accept(serverFd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

        if (clientFd < 0) {
            if (errno == EINTR) {
                // 被信号打断——可能在关闭
                if (!running_) break;
                continue;
            }
            if (!running_) break;
            log.error("accept() failed: " + std::string(std::strerror(errno)));
            continue;
        }

        // 收割已终止的子进程
        reapChildren();

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
        log.info(std::string("[Server] Connection from ") + clientIP + ":"
                  + std::to_string(ntohs(clientAddr.sin_port)));

        pid_t pid = fork();
        if (pid < 0) {
            log.error("fork() failed.");
            ::close(clientFd);
            continue;
        }

        if (pid == 0) {
            // 子进程：重置信号处理（继承父进程设置不妥）
            signal(SIGTERM, SIG_DFL);
            signal(SIGINT, SIG_DFL);
            ::close(serverFd_);
            ServerSession session(NetworkSocket(clientFd), config_.storagePath());
            session.run();
            log.info(std::string("[Server] Session ended for ") + clientIP);
            _exit(0);
        } else {
            // 父进程：记录子进程 PID
            childPids_.push_back(pid);
            ::close(clientFd);
        }
    }

    // ---- 优雅关闭 ----
    log.info("Shutting down gracefully...");

    // 关闭监听 socket（不再接受新连接）
    if (serverFd_ >= 0) {
        ::close(serverFd_);
        serverFd_ = -1;
    }

    // 等待活跃子进程结束（最多等 10 秒）
    if (!childPids_.empty()) {
        // 先收割所有已退出子进程（清除僵尸进程）
        while (waitpid(-1, nullptr, WNOHANG) > 0);

        // 统计实际仍在运行的子进程
        auto countRunning = [this]() -> int {
            int n = 0;
            for (pid_t pid : childPids_) {
                if (kill(pid, 0) == 0) n++;
            }
            return n;
        };

        int running = countRunning();
        if (running > 0) {
            log.info("Waiting for " + std::to_string(running) + " active session(s) to finish...");

            // 先发 SIGTERM 给所有运行中的子进程
            for (pid_t pid : childPids_) {
                if (kill(pid, 0) == 0) {
                    kill(pid, SIGTERM);
                }
            }

            // 等待最多 10 秒
            int waited = 0;
            while (countRunning() > 0 && waited < 10) {
                sleep(1);
                waited++;
            }
        }

        // 收割所有子进程（包括已退出的）
        while (waitpid(-1, nullptr, WNOHANG) > 0);

        // 超时则强制终止剩余进程
        for (pid_t pid : childPids_) {
            if (kill(pid, 0) == 0) {
                log.warn("Force killing session PID " + std::to_string(pid));
                kill(pid, SIGKILL);
            }
        }

        // 等待所有强制终止完成
        while (waitpid(-1, nullptr, 0) > 0);
    }

    log.info("Server stopped.");
    return true;
}

void BackupServer::stop() {
    running_ = false;
}
