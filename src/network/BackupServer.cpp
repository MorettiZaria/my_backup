#include "network/BackupServer.h"
#include "network/ServerSession.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <cstring>

BackupServer::BackupServer(uint16_t port, const std::string& storagePath)
    : port_(port), storagePath_(storagePath) {}

void BackupServer::sigchldHandler(int) {
    // 收割所有已终止的子进程（非阻塞）
    while (waitpid(-1, nullptr, WNOHANG) > 0);
}

bool BackupServer::start() {
    // 安装信号处理
    struct sigaction sa{};
    sa.sa_handler = sigchldHandler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    // 忽略 SIGPIPE（防止写入已关闭的 socket 导致进程终止）
    signal(SIGPIPE, SIG_IGN);

    // 创建 socket
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ < 0) {
        std::cerr << "Error: socket() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(serverFd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Error: bind() failed: " << std::strerror(errno) << std::endl;
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }

    if (listen(serverFd_, 16) < 0) {
        std::cerr << "Error: listen() failed: " << std::strerror(errno) << std::endl;
        ::close(serverFd_);
        serverFd_ = -1;
        return false;
    }

    std::cout << "=== Backup Server ===" << std::endl;
    std::cout << "Listening on 0.0.0.0:" << port_ << std::endl;
    std::cout << "Storage: " << storagePath_ << std::endl;
    std::cout << "Press Ctrl+C to stop." << std::endl;

    running_ = true;

    while (running_) {
        struct sockaddr_in clientAddr{};
        socklen_t addrLen = sizeof(clientAddr);
        int clientFd = accept(serverFd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

        if (clientFd < 0) {
            if (errno == EINTR) continue;  // 被信号打断，继续
            if (!running_) break;
            std::cerr << "Error: accept() failed: " << std::strerror(errno) << std::endl;
            continue;
        }

        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, sizeof(clientIP));
        std::cout << "[Server] Connection from " << clientIP << ":"
                  << ntohs(clientAddr.sin_port) << std::endl;

        pid_t pid = fork();
        if (pid < 0) {
            std::cerr << "Error: fork() failed." << std::endl;
            ::close(clientFd);
            continue;
        }

        if (pid == 0) {
            // 子进程
            ::close(serverFd_);
            ServerSession session(NetworkSocket(clientFd), storagePath_);
            session.run();
            std::cout << "[Server] Session ended for " << clientIP << std::endl;
            _exit(0);
        } else {
            // 父进程
            ::close(clientFd);
        }
    }

    if (serverFd_ >= 0) {
        ::close(serverFd_);
        serverFd_ = -1;
    }

    std::cout << "Server stopped." << std::endl;
    return true;
}

void BackupServer::stop() {
    running_ = false;
}
