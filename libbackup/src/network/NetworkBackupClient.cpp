#include "network/NetworkBackupClient.h"
#include "network/UserManager.h"
#include "core/FileScanner.h"
#include "core/BackupEngine.h"
#include "metadata/MetadataSerializer.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <unistd.h>

NetworkBackupClient::NetworkBackupClient(const std::string& host, uint16_t port,
                                           const std::string& username,
                                           const std::string& password)
    : host_(host), port_(port), username_(username), password_(password) {}

void NetworkBackupClient::setBackupName(const std::string& name) {
    backupName_ = name;
}

void NetworkBackupClient::setFileFilter(IFileFilter* filter) {
    fileFilter_ = filter;
}

// ===== 收发 =====

bool NetworkBackupClient::sendEncrypted(const NetworkMessage& msg) {
    NetworkMessage toSend = msg;
    if (encryptor_.isActive()) {
        toSend.payload = encryptor_.encrypt(msg.payload, sendSeq_++);
    }
    return socket_.sendMessage(toSend);
}

NetworkMessage NetworkBackupClient::receiveEncrypted() {
    NetworkMessage msg = socket_.receiveMessage();
    if (msg.type == 0) return msg;
    if (encryptor_.isActive()) {
        msg.payload = encryptor_.decrypt(msg.payload, recvSeq_++);
    }
    return msg;
}

// ===== 握手 =====

bool NetworkBackupClient::doHandshake() {
    // 发送 CLIENT_HELLO
    std::vector<uint8_t> payload;
    writeUint16BE(payload, PROTOCOL_VERSION);
    if (!socket_.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, std::move(payload)))) {
        std::cerr << "Error: failed to send CLIENT_HELLO." << std::endl;
        return false;
    }

    // 接收 SERVER_HELLO
    NetworkMessage resp = socket_.receiveMessage();
    if (resp.type != static_cast<uint16_t>(MessageType::SERVER_HELLO)) {
        std::cerr << "Error: server rejected connection." << std::endl;
        return false;
    }

    if (resp.payload.size() >= 10) {
        size_t off = 0;
        /*uint16_t serverVer =*/ readUint16BE(resp.payload.data(), off);
        serverChallenge_.assign(resp.payload.begin() + static_cast<ptrdiff_t>(off),
                                resp.payload.end());
    }

    return true;
}

// ===== 登录 =====

bool NetworkBackupClient::doLogin() {
    // 第一步：发送用户名
    std::vector<uint8_t> payload;
    writeStringBE(payload, username_);
    if (!sendEncrypted(NetworkMessage::make(MessageType::LOGIN_REQUEST, std::move(payload)))) {
        return false;
    }

    // 第二步：接收盐值
    NetworkMessage saltMsg = receiveEncrypted();
    if (saltMsg.type != static_cast<uint16_t>(MessageType::LOGIN_SALT)) return false;
    std::vector<uint8_t> userSalt = saltMsg.payload;  // 注册时的盐值

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

    std::cerr << "Error: login failed (wrong password?)." << std::endl;
    return false;
}

// ===== 注册 =====

bool NetworkBackupClient::doRegister() {
    // 客户端预先计算 hash
    auto salt = UserManager::generateSalt();
    auto pwHash = UserManager::computeHash(password_, salt);

    std::vector<uint8_t> payload;
    writeStringBE(payload, username_);
    payload.insert(payload.end(), salt.begin(), salt.end());
    payload.insert(payload.end(), pwHash.begin(), pwHash.end());

    if (!sendEncrypted(NetworkMessage::make(MessageType::REGISTER_REQUEST, std::move(payload)))) {
        return false;
    }

    NetworkMessage resp = receiveEncrypted();
    if (resp.type != static_cast<uint16_t>(MessageType::REGISTER_RESPONSE)) return false;
    if (resp.payload.empty()) return false;

    uint8_t status = resp.payload[0];
    if (status == 0) {
        // 注册成功 = 自动登录
        encryptor_.initSession(username_, serverChallenge_);
        return true;
    }

    std::cerr << "Error: registration failed (user may already exist)." << std::endl;
    return false;
}

// ===== 主流程 =====

bool NetworkBackupClient::run(const std::string& sourceDir,
                               IPackStrategy* packStrategy,
                               ICompressStrategy* compressStrategy,
                               IEncryptStrategy* encryptStrategy,
                               const std::string& filePassword) {
    std::cout << "=== Remote Backup ===" << std::endl;
    std::cout << "Server: " << host_ << ":" << port_ << std::endl;
    std::cout << "Source: " << sourceDir << std::endl;

    // 1. 连接（重试最多 5 次，应对服务器尚未就绪的情况）
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

    // 2. 握手
    if (!doHandshake()) return false;

    // 3. 认证（先尝试登录，失败则尝试注册）
    if (!doLogin()) {
        std::cout << "Login failed, trying to register..." << std::endl;
        if (!doRegister()) return false;
        std::cout << "Registered successfully." << std::endl;
    }

    // 4. 发送自定义备份名（如有）
    if (!backupName_.empty()) {
        std::vector<uint8_t> namePayload;
        writeStringBE(namePayload, backupName_);
        if (!sendEncrypted(NetworkMessage::make(MessageType::BACKUP_START, std::move(namePayload)))) {
            std::cerr << "Error: failed to send backup name." << std::endl;
            return false;
        }
    }

    // 5. 扫描源目录
    std::cout << "Scanning directory tree..." << std::endl;
    FileScanner scanner;
    if (fileFilter_) scanner.setFilter(fileFilter_);
    auto files = scanner.scan(sourceDir);
    std::cout << "  Found " << files.size() << " entries." << std::endl;

    // 6. 构建 .bak 文件
    std::cout << "Building backup..." << std::endl;
    BackupEngine engine;
    engine.setPackStrategy(packStrategy);
    engine.setCompressStrategy(compressStrategy);
    engine.setEncryptStrategy(encryptStrategy);
    if (fileFilter_) engine.setFileFilter(fileFilter_);

    // 写入临时文件
    std::string tmpBakPath = "/tmp/remote_backup_" + username_ + ".bak";
    if (!engine.run(sourceDir, tmpBakPath, filePassword)) {
        std::cerr << "Error: backup engine failed." << std::endl;
        return false;
    }

    // 6. 读取 .bak 文件
    std::ifstream bakFile(tmpBakPath, std::ios::binary | std::ios::ate);
    if (!bakFile) {
        std::cerr << "Error: cannot read temp .bak file." << std::endl;
        return false;
    }
    size_t bakSize = static_cast<size_t>(bakFile.tellg());
    bakFile.seekg(0);
    std::vector<uint8_t> wholeBak(bakSize);
    bakFile.read(reinterpret_cast<char*>(wholeBak.data()), static_cast<std::streamsize>(bakSize));
    bakFile.close();

    // 7. 序列化元数据（不含 content）
    // 创建一个不含 content 的 FileInfo 副本
    std::vector<FileInfo> metaOnly;
    for (auto& f : files) {
        FileInfo mf = f;
        mf.content.clear();  // 清空文件内容，只保留元数据
        metaOnly.push_back(std::move(mf));
    }
    MetadataSerializer serializer;
    auto metaBytes = serializer.serialize(metaOnly);

    // 8. 发送 .bak 文件
    std::cout << "Sending backup to server (" << bakSize << " bytes)..." << std::endl;
    {
        std::vector<uint8_t> filePayload;
        writeStringBE(filePayload, ".bak_payload");
        writeUint64BE(filePayload, wholeBak.size());
        filePayload.insert(filePayload.end(), wholeBak.begin(), wholeBak.end());
        if (!sendEncrypted(NetworkMessage::make(MessageType::FILE_DATA, std::move(filePayload)))) {
            std::cerr << "Error: failed to send backup data." << std::endl;
            return false;
        }
    }

    // 9. 发送元数据
    {
        std::vector<uint8_t> metaPayload;
        writeStringBE(metaPayload, ".bak_metadata");
        writeUint64BE(metaPayload, metaBytes.size());
        metaPayload.insert(metaPayload.end(), metaBytes.begin(), metaBytes.end());
        if (!sendEncrypted(NetworkMessage::make(MessageType::FILE_DATA, std::move(metaPayload)))) {
            std::cerr << "Error: failed to send metadata." << std::endl;
            return false;
        }
    }

    // 10. 发送完成信号
    if (!sendEncrypted(NetworkMessage::make(MessageType::BACKUP_COMPLETE))) {
        std::cerr << "Error: failed to send completion." << std::endl;
        return false;
    }

    // 11. 接收确认
    NetworkMessage ack = receiveEncrypted();
    if (ack.type == static_cast<uint16_t>(MessageType::BACKUP_COMPLETE) && ack.payload.size() >= 2) {
        size_t off = 0;
        std::string backupId = readStringBE(ack.payload.data(), off);
        std::cout << "Backup complete! ID: " << backupId << std::endl;
    }

    // 12. 清理临时文件
    unlink(tmpBakPath.c_str());

    return true;
}
