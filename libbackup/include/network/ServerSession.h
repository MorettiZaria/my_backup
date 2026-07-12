#ifndef BACKUP_SERVERSESSION_H
#define BACKUP_SERVERSESSION_H

#include "network/NetworkSocket.h"
#include "network/UserManager.h"
#include "network/ServerStorage.h"
#include "network/TransportEncryptor.h"
#include <string>
#include <vector>
#include <cstdint>

/// 单个客户端会话：处理握手→认证→命令执行
class ServerSession {
public:
    ServerSession(NetworkSocket socket, const std::string& storagePath);

    /// 运行会话（阻塞，处理完毕后返回）
    void run();

private:
    NetworkSocket socket_;
    ServerStorage storage_;
    UserManager userMgr_;
    TransportEncryptor encryptor_;
    std::string currentUser_;
    bool authenticated_ = false;

    uint64_t sendSeq_ = 0;
    uint64_t recvSeq_ = 0;

    std::vector<uint8_t> serverChallenge_;  // 8 字节随机挑战码

    // 带传输加密的消息收发
    bool sendEncrypted(const NetworkMessage& msg);
    NetworkMessage receiveEncrypted();

    // 协议处理（msg 参数为 run() 中已接收的消息，避免重复 recv）
    bool handleHandshake();
    bool handleLogin(NetworkMessage& msg);
    bool handleRegister(NetworkMessage& msg);
    bool handleBackup(NetworkMessage& firstMsg);
    bool handleRestore(NetworkMessage& msg);
    bool handleListBackups();
    void handleLogout();

    // 辅助
    void sendError(uint16_t code, const std::string& msg);
    bool sendResponse(uint16_t type, uint8_t status);
};

#endif // BACKUP_SERVERSESSION_H
