#ifndef BACKUP_NETWORKSOCKET_H
#define BACKUP_NETWORKSOCKET_H

#include "NetworkProtocol.h"
#include <string>
#include <cstdint>

/// RAII socket 包装器，处理 TCP 连接、消息收发
class NetworkSocket {
public:
    NetworkSocket();
    explicit NetworkSocket(int fd);
    ~NetworkSocket();

    // 禁止拷贝，允许移动
    NetworkSocket(const NetworkSocket&) = delete;
    NetworkSocket& operator=(const NetworkSocket&) = delete;
    NetworkSocket(NetworkSocket&& other) noexcept;
    NetworkSocket& operator=(NetworkSocket&& other) noexcept;

    // 客户端：连接远程服务器
    [[nodiscard]] bool connect(const std::string& host, uint16_t port);

    // 服务器：绑定并监听（INADDR_ANY，接受来自任何网络的连接）
    [[nodiscard]] bool bindAndListen(uint16_t port, int backlog = 16);

    // 接受客户端连接，返回新的 socket
    NetworkSocket accept();

    // 发送/接收带帧的消息
    [[nodiscard]] bool sendMessage(const NetworkMessage& msg);
    NetworkMessage receiveMessage();

    // 设置接收超时（秒），0=无超时
    [[nodiscard]] bool setReceiveTimeout(int seconds);

    // 状态
    [[nodiscard]] bool isValid() const;
    int fd() const;
    void close();

private:
    int fd_;

    // 确保写入/读取指定字节数（处理 TCP 部分读写）
    bool sendAll(const uint8_t* data, size_t len);
    bool recvAll(uint8_t* data, size_t len);
};

#endif // BACKUP_NETWORKSOCKET_H
