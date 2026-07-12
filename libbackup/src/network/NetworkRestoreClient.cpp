#include "network/NetworkRestoreClient.h"
#include "network/UserManager.h"
#include "core/RestoreEngine.h"
#include "pack/PackManager.h"
#include "compress/CompressManager.h"
#include "encrypt/EncryptManager.h"
#include "pack/TarPackStrategy.h"
#include "pack/IndexPackStrategy.h"
#include "compress/RleCompressStrategy.h"
#include "compress/HuffmanCompressStrategy.h"
#include "encrypt/XorEncryptStrategy.h"
#include "encrypt/VigenereEncryptStrategy.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>

NetworkRestoreClient::NetworkRestoreClient(const std::string& host, uint16_t port,
                                             const std::string& username,
                                             const std::string& password)
    : host_(host), port_(port), username_(username), password_(password) {}

// ===== 收发 =====

bool NetworkRestoreClient::sendEncrypted(const NetworkMessage& msg) {
    NetworkMessage toSend = msg;
    if (encryptor_.isActive()) {
        toSend.payload = encryptor_.encrypt(msg.payload, sendSeq_++);
    }
    return socket_.sendMessage(toSend);
}

NetworkMessage NetworkRestoreClient::receiveEncrypted() {
    NetworkMessage msg = socket_.receiveMessage();
    if (msg.type == 0) return msg;
    if (encryptor_.isActive()) {
        msg.payload = encryptor_.decrypt(msg.payload, recvSeq_++);
    }
    return msg;
}

// ===== 握手 =====

bool NetworkRestoreClient::doHandshake() {
    std::vector<uint8_t> payload;
    writeUint16BE(payload, PROTOCOL_VERSION);
    if (!socket_.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, std::move(payload)))) {
        std::cerr << "Error: failed to send CLIENT_HELLO." << std::endl;
        return false;
    }

    NetworkMessage resp = socket_.receiveMessage();
    if (resp.type != static_cast<uint16_t>(MessageType::SERVER_HELLO)) {
        std::cerr << "Error: server rejected connection." << std::endl;
        return false;
    }

    if (resp.payload.size() >= 10) {
        size_t off = 0;
        readUint16BE(resp.payload.data(), off);  // server version
        serverChallenge_.assign(resp.payload.begin() + static_cast<ptrdiff_t>(off),
                                resp.payload.end());
    }
    return true;
}

// ===== 登录 =====

bool NetworkRestoreClient::doLogin() {
    // 第一步：发送用户名
    std::vector<uint8_t> payload;
    writeStringBE(payload, username_);
    if (!sendEncrypted(NetworkMessage::make(MessageType::LOGIN_REQUEST, std::move(payload)))) {
        return false;
    }

    // 第二步：接收盐值
    NetworkMessage saltMsg = receiveEncrypted();
    if (saltMsg.type != static_cast<uint16_t>(MessageType::LOGIN_SALT)) return false;
    std::vector<uint8_t> userSalt = saltMsg.payload;

    // 第三步：用正确的盐值计算哈希
    auto computedHash = UserManager::computeHash(password_, userSalt);

    if (!sendEncrypted(NetworkMessage::make(MessageType::LOGIN_PROOF, computedHash))) {
        return false;
    }

    // 第四步：接收登录结果
    NetworkMessage resp = receiveEncrypted();
    if (resp.type != static_cast<uint16_t>(MessageType::LOGIN_RESPONSE)) return false;
    if (resp.payload.empty()) return false;

    if (resp.payload[0] == 0) {
        encryptor_.initSession(username_, serverChallenge_);
        return true;
    }

    std::cerr << "Error: login failed." << std::endl;
    return false;
}

// ===== 列出备份 =====

bool NetworkRestoreClient::listBackups() {
    // 连接（重试最多 5 次）
    bool connected = false;
    for (int retry = 0; retry < 5; ++retry) {
        if (socket_.connect(host_, port_)) {
            connected = true;
            break;
        }
        if (retry < 4) {
            std::cerr << "Connection failed, retrying in 500ms..." << std::endl;
            usleep(500000);
        }
    }
    if (!connected) {
        std::cerr << "Error: cannot connect to server after 5 attempts." << std::endl;
        return false;
    }
    if (!doHandshake()) return false;
    if (!doLogin()) return false;

    if (!sendEncrypted(NetworkMessage::make(MessageType::BACKUP_LIST_REQUEST))) {
        return false;
    }

    NetworkMessage resp = receiveEncrypted();
    if (resp.type != static_cast<uint16_t>(MessageType::BACKUP_LIST_RESPONSE)) {
        std::cerr << "Error: unexpected response." << std::endl;
        return false;
    }

    size_t off = 0;
    uint32_t count = readUint32BE(resp.payload.data(), off);
    std::cout << "Backups for " << username_ << " (" << count << " total):" << std::endl;
    for (uint32_t i = 0; i < count; ++i) {
        std::string id = readStringBE(resp.payload.data(), off);
        uint64_t ts = readUint64BE(resp.payload.data(), off);
        std::cout << "  " << id << "  (timestamp: " << ts << ")" << std::endl;
    }

    return true;
}

// ===== 还原 =====

bool NetworkRestoreClient::run(const std::string& destDir,
                                 const std::string& backupId,
                                 const std::string& filePassword) {
    std::cout << "=== Remote Restore ===" << std::endl;
    std::cout << "Server: " << host_ << ":" << port_ << std::endl;
    std::cout << "Destination: " << destDir << std::endl;

    // 1. 连接 + 握手 + 登录（连接重试最多 5 次）
    bool connected = false;
    for (int retry = 0; retry < 5; ++retry) {
        if (socket_.connect(host_, port_)) {
            connected = true;
            break;
        }
        if (retry < 4) {
            std::cerr << "Connection failed, retrying in 500ms..." << std::endl;
            usleep(500000);
        }
    }
    if (!connected) {
        std::cerr << "Error: cannot connect to server after 5 attempts." << std::endl;
        return false;
    }
    if (!doHandshake()) return false;
    if (!doLogin()) return false;

    // 2. 发送还原请求
    std::vector<uint8_t> reqPayload;
    writeStringBE(reqPayload, backupId);
    if (!sendEncrypted(NetworkMessage::make(MessageType::RESTORE_REQUEST, std::move(reqPayload)))) {
        return false;
    }

    // 3. 接收文件数据
    std::vector<uint8_t> wholeBak;
    std::vector<uint8_t> metadataBytes;

    while (true) {
        NetworkMessage msg = receiveEncrypted();
        if (msg.type == 0) {
            std::cerr << "Error: connection lost." << std::endl;
            return false;
        }

        if (msg.type == static_cast<uint16_t>(MessageType::RESTORE_COMPLETE)) {
            break;
        }

        if (msg.type == static_cast<uint16_t>(MessageType::RESTORE_FILE_DATA) ||
            msg.type == static_cast<uint16_t>(MessageType::FILE_DATA)) {
            if (msg.payload.size() < 10) continue;
            size_t off = 0;
            std::string path = readStringBE(msg.payload.data(), off);
            uint64_t dataLen = readUint64BE(msg.payload.data(), off);

            std::vector<uint8_t> fileData(
                msg.payload.begin() + static_cast<ptrdiff_t>(off),
                msg.payload.begin() + static_cast<ptrdiff_t>(off + dataLen));

            if (path == ".bak") {
                wholeBak = std::move(fileData);
            } else if (path == ".bak_metadata") {
                metadataBytes = std::move(fileData);
            }
        } else if (msg.type == static_cast<uint16_t>(MessageType::ERROR_MESSAGE)) {
            if (msg.payload.size() >= 4) {
                size_t off = 0;
                readUint16BE(msg.payload.data(), off);  // error code
                std::string errMsg = readStringBE(msg.payload.data(), off);
                std::cerr << "Server error: " << errMsg << std::endl;
            }
            return false;
        }
    }

    if (wholeBak.empty()) {
        std::cerr << "Error: no backup data received." << std::endl;
        return false;
    }

    std::cout << "Received backup (" << wholeBak.size() << " bytes)." << std::endl;

    // 4. 写入临时 .bak 文件
    std::string tmpBakPath = "/tmp/remote_restore_" + username_ + ".bak";
    std::ofstream tmpOut(tmpBakPath, std::ios::binary);
    if (!tmpOut) {
        std::cerr << "Error: cannot create temp file." << std::endl;
        return false;
    }
    tmpOut.write(reinterpret_cast<const char*>(wholeBak.data()),
                 static_cast<std::streamsize>(wholeBak.size()));
    tmpOut.close();

    // 5. 用本地 RestoreEngine 还原
    // 注册所有策略
    PackManager packMgr;
    packMgr.registerStrategy(std::make_unique<TarPackStrategy>());
    packMgr.registerStrategy(std::make_unique<IndexPackStrategy>());

    CompressManager compressMgr;
    compressMgr.registerStrategy(std::make_unique<RleCompressStrategy>());
    compressMgr.registerStrategy(std::make_unique<HuffmanCompressStrategy>());

    EncryptManager encryptMgr;
    encryptMgr.registerStrategy(std::make_unique<XorEncryptStrategy>());
    encryptMgr.registerStrategy(std::make_unique<VigenereEncryptStrategy>());

    RestoreEngine engine;
    engine.setPackManager(&packMgr);
    engine.setCompressManager(&compressMgr);
    engine.setEncryptManager(&encryptMgr);

    std::cout << "Restoring..." << std::endl;
    if (!engine.run(tmpBakPath, destDir, filePassword)) {
        std::cerr << "Error: restore failed." << std::endl;
        unlink(tmpBakPath.c_str());
        return false;
    }

    // 6. 清理
    unlink(tmpBakPath.c_str());
    std::cout << "Restore complete!" << std::endl;
    return true;
}
