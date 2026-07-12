#include "network/NetworkSocket.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

NetworkSocket::NetworkSocket() : fd_(-1) {}

NetworkSocket::NetworkSocket(int fd) : fd_(fd) {}

NetworkSocket::~NetworkSocket() {
    close();
}

NetworkSocket::NetworkSocket(NetworkSocket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

NetworkSocket& NetworkSocket::operator=(NetworkSocket&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool NetworkSocket::connect(const std::string& host, uint16_t port) {
    close();

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "Error: socket() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "Error: invalid address: " << host << std::endl;
        close();
        return false;
    }

    if (::connect(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Error: connect() failed: " << std::strerror(errno) << std::endl;
        close();
        return false;
    }

    return true;
}

bool NetworkSocket::bindAndListen(uint16_t port, int backlog) {
    close();

    fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) {
        std::cerr << "Error: socket() failed: " << std::strerror(errno) << std::endl;
        return false;
    }

    // 允许端口复用，方便快速重启
    int opt = 1;
    setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  // ★ 绑定所有网络接口
    addr.sin_port = htons(port);

    if (bind(fd_, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Error: bind() failed: " << std::strerror(errno) << std::endl;
        close();
        return false;
    }

    if (listen(fd_, backlog) < 0) {
        std::cerr << "Error: listen() failed: " << std::strerror(errno) << std::endl;
        close();
        return false;
    }

    return true;
}

NetworkSocket NetworkSocket::accept() {
    struct sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    int clientFd = ::accept(fd_, reinterpret_cast<struct sockaddr*>(&clientAddr), &addrLen);

    if (clientFd < 0) {
        return NetworkSocket();  // 返回无效 socket
    }

    return NetworkSocket(clientFd);
}

bool NetworkSocket::sendMessage(const NetworkMessage& msg) {
    std::vector<uint8_t> data = msg.serialize();
    return sendAll(data.data(), data.size());
}

NetworkMessage NetworkSocket::receiveMessage() {
    // 读取 8 字节头部
    uint8_t header[MESSAGE_HEADER_SIZE];
    if (!recvAll(header, MESSAGE_HEADER_SIZE)) {
        return NetworkMessage();  // 返回空消息（type=0）表示错误
    }

    size_t off = 0;
    uint16_t type = (static_cast<uint16_t>(header[off]) << 8) | header[off + 1];
    off += 2;
    // reserved (skip)
    off += 2;
    uint32_t payloadLen = (static_cast<uint32_t>(header[off]) << 24)
                        | (static_cast<uint32_t>(header[off + 1]) << 16)
                        | (static_cast<uint32_t>(header[off + 2]) << 8)
                        | static_cast<uint32_t>(header[off + 3]);

    // 读取载荷
    std::vector<uint8_t> payload;
    if (payloadLen > 0) {
        payload.resize(payloadLen);
        if (!recvAll(payload.data(), payloadLen)) {
            return NetworkMessage();
        }
    }

    return NetworkMessage(type, std::move(payload));
}

bool NetworkSocket::setReceiveTimeout(int seconds) {
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    return setsockopt(fd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
}

bool NetworkSocket::isValid() const {
    return fd_ >= 0;
}

int NetworkSocket::fd() const {
    return fd_;
}

void NetworkSocket::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

// ===== 私有：处理 TCP 部分读写 =====

bool NetworkSocket::sendAll(const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd_, data + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

bool NetworkSocket::recvAll(uint8_t* data, size_t len) {
    size_t received = 0;
    while (received < len) {
        ssize_t n = ::recv(fd_, data + received, len - received, 0);
        if (n <= 0) return false;  // 错误或对端关闭
        received += static_cast<size_t>(n);
    }
    return true;
}
