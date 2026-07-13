#ifndef BACKUP_NETWORKBACKUPCLIENT_H
#define BACKUP_NETWORKBACKUPCLIENT_H

#include "network/NetworkSocket.h"
#include "network/TransportEncryptor.h"
#include "core/FileInfo.h"
#include "pack/IPackStrategy.h"
#include "compress/ICompressStrategy.h"
#include "encrypt/IEncryptStrategy.h"
#include <string>
#include <vector>
#include <cstdint>

/// 远程备份客户端：扫描本地目录→构建 .bak→发送到服务器
class NetworkBackupClient {
public:
    NetworkBackupClient(const std::string& host, uint16_t port,
                        const std::string& username,
                        const std::string& password);

    /// 设置自定义备份名称（空字符串=自动生成）
    void setBackupName(const std::string& name);

    /// 执行远程备份
    /// @param packStrategy 打包策略（必须）
    /// @param compressStrategy 压缩策略（nullptr=不压缩）
    /// @param encryptStrategy 加密策略（nullptr=不加密）
    /// @param filePassword 文件加密密码
    [[nodiscard]] bool run(const std::string& sourceDir,
                           IPackStrategy* packStrategy,
                           ICompressStrategy* compressStrategy,
                           IEncryptStrategy* encryptStrategy,
                           const std::string& filePassword = "");

private:
    std::string host_;
    uint16_t port_;
    std::string username_;
    std::string password_;
    std::string backupName_;
    NetworkSocket socket_;
    TransportEncryptor encryptor_;
    uint64_t sendSeq_ = 0;
    uint64_t recvSeq_ = 0;

    std::vector<uint8_t> serverChallenge_;

    bool doHandshake();
    bool doLogin();
    bool doRegister();
    bool sendEncrypted(const NetworkMessage& msg);
    NetworkMessage receiveEncrypted();
};
#endif // BACKUP_NETWORKBACKUPCLIENT_H
