#ifndef BACKUP_NETWORKRESTORECLIENT_H
#define BACKUP_NETWORKRESTORECLIENT_H

#include "network/NetworkSocket.h"
#include "network/TransportEncryptor.h"
#include <string>
#include <vector>
#include <cstdint>

/// 远程还原客户端：从服务器获取 .bak → 本地还原
class NetworkRestoreClient {
public:
    NetworkRestoreClient(const std::string& host, uint16_t port,
                         const std::string& username,
                         const std::string& password);

    /// 执行远程还原
    /// @param destDir 还原目标目录
    /// @param backupId 备份 ID（空=使用最新备份）
    /// @param filePassword 文件解密密码
    /// @return 成功返回 true
    [[nodiscard]] bool run(const std::string& destDir,
                           const std::string& backupId = "",
                           const std::string& filePassword = "");

    /// 列出远程备份
    [[nodiscard]] bool listBackups();

private:
    std::string host_;
    uint16_t port_;
    std::string username_;
    std::string password_;
    NetworkSocket socket_;
    TransportEncryptor encryptor_;
    uint64_t sendSeq_ = 0;
    uint64_t recvSeq_ = 0;

    std::vector<uint8_t> serverChallenge_;

    bool doHandshake();
    bool doLogin();
    bool sendEncrypted(const NetworkMessage& msg);
    NetworkMessage receiveEncrypted();
};

#endif // BACKUP_NETWORKRESTORECLIENT_H
