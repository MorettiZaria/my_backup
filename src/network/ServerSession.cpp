#include "network/ServerSession.h"
#include "network/Logger.h"
#include <random>
#include <cstring>
#include <algorithm>

ServerSession::ServerSession(NetworkSocket socket, const std::string& storagePath)
    : socket_(std::move(socket))
    , storage_(storagePath)
    , userMgr_(storagePath + "/users.db") {}

// ===== 消息收发（带传输加密） =====

bool ServerSession::sendEncrypted(const NetworkMessage& msg) {
    NetworkMessage toSend = msg;
    if (encryptor_.isActive()) {
        toSend.payload = encryptor_.encrypt(msg.payload, sendSeq_++);
    }
    return socket_.sendMessage(toSend);
}

NetworkMessage ServerSession::receiveEncrypted() {
    NetworkMessage msg = socket_.receiveMessage();
    if (msg.type == 0) return msg;  // 空消息（错误）
    if (encryptor_.isActive()) {
        msg.payload = encryptor_.decrypt(msg.payload, recvSeq_++);
    }
    return msg;
}

void ServerSession::sendError(uint16_t code, const std::string& msg) {
    std::vector<uint8_t> payload;
    writeUint16BE(payload, code);
    writeStringBE(payload, msg);
    sendEncrypted(NetworkMessage::make(MessageType::ERROR_MESSAGE, std::move(payload)));
}

bool ServerSession::sendResponse(uint16_t type, uint8_t status) {
    std::vector<uint8_t> payload;
    payload.push_back(status);
    return socket_.sendMessage(NetworkMessage(type, std::move(payload)));
}

// ===== run() 状态机 =====

void ServerSession::run() {
    auto& log = Logger::instance();
    log.info("[Session] New connection.");

    // 设置 300 秒超时
    socket_.setReceiveTimeout(300);

    // 状态: HANDSHAKE
    if (!handleHandshake()) return;

    // 状态: AUTH
    while (!authenticated_) {
        NetworkMessage msg = receiveEncrypted();
        if (msg.type == 0) return;  // 连接断开

        switch (static_cast<MessageType>(msg.type)) {
        case MessageType::LOGIN_REQUEST:
            handleLogin(msg);
            break;
        case MessageType::REGISTER_REQUEST:
            handleRegister(msg);
            break;
        case MessageType::LOGOUT:
            return;
        default:
            sendError(ErrorCode::NOT_LOGGED_IN, "Please login or register first.");
            break;
        }
    }

    // 状态: READY
    while (true) {
        NetworkMessage msg = receiveEncrypted();
        if (msg.type == 0) {
            log.info("[Session] Client disconnected.");
            return;
        }

        switch (static_cast<MessageType>(msg.type)) {
        case MessageType::FILE_DATA:
        case MessageType::BACKUP_COMPLETE:
            if (!handleBackup(msg)) return;
            break;
        case MessageType::RESTORE_REQUEST:
            if (!handleRestore(msg)) return;
            break;
        case MessageType::BACKUP_LIST_REQUEST:
            handleListBackups();
            break;
        case MessageType::LOGOUT:
            handleLogout();
            return;
        default:
            sendError(ErrorCode::INVALID_MESSAGE, "Unknown command.");
            break;
        }
    }
}

// ===== 握手 =====

bool ServerSession::handleHandshake() {
    auto& log = Logger::instance();

    // 等待 CLIENT_HELLO
    NetworkMessage hello = socket_.receiveMessage();  // 握手阶段不加密
    if (hello.type != static_cast<uint16_t>(MessageType::CLIENT_HELLO)) {
        sendError(ErrorCode::INCOMPATIBLE_VERSION, "Expected CLIENT_HELLO.");
        return false;
    }

    if (hello.payload.size() >= 2) {
        size_t off = 0;
        uint16_t clientVer = readUint16BE(hello.payload.data(), off);
        if (clientVer != PROTOCOL_VERSION) {
            sendError(ErrorCode::INCOMPATIBLE_VERSION, "Protocol version mismatch.");
            return false;
        }
    }

    // 生成 8 字节随机挑战码
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);
    serverChallenge_.resize(8);
    for (int i = 0; i < 8; ++i) {
        serverChallenge_[i] = static_cast<uint8_t>(dist(gen));
    }

    // 发送 SERVER_HELLO
    std::vector<uint8_t> payload;
    writeUint16BE(payload, PROTOCOL_VERSION);
    payload.insert(payload.end(), serverChallenge_.begin(), serverChallenge_.end());
    if (!socket_.sendMessage(NetworkMessage::make(MessageType::SERVER_HELLO, std::move(payload)))) {
        return false;
    }

    log.info("[Session] Handshake complete.");
    return true;
}

// ===== 登录 =====

bool ServerSession::handleLogin(NetworkMessage& msg) {
    auto& log = Logger::instance();

    // 第一步：LOGIN_REQUEST 仅含用户名
    if (msg.payload.size() < 2) {
        sendError(ErrorCode::INVALID_MESSAGE, "Invalid LOGIN_REQUEST.");
        return true;
    }

    size_t off = 0;
    std::string name = readStringBE(msg.payload.data(), off);

    // 查找用户
    std::vector<uint8_t> storedSalt, storedHash;
    if (!userMgr_.getUserHash(name, storedSalt, storedHash)) {
        log.info("[Session] Login failed: user '" + name + "' not found.");
        sendResponse(static_cast<uint16_t>(MessageType::LOGIN_RESPONSE), 1);
        return true;
    }

    // 第二步：发送用户注册时的盐值
    if (!socket_.sendMessage(NetworkMessage(static_cast<uint16_t>(MessageType::LOGIN_SALT), storedSalt))) {
        return false;
    }

    // 第三步：接收客户端计算的哈希 (直接比对密码哈希，不用挑战-响应)
    NetworkMessage proofMsg = receiveEncrypted();
    if (proofMsg.type == 0 || proofMsg.payload.size() < 32) {
        sendError(ErrorCode::INVALID_MESSAGE, "Invalid login proof.");
        return true;
    }
    std::vector<uint8_t> clientHash(proofMsg.payload.begin(), proofMsg.payload.begin() + 32);

    // 第四步：直接比对哈希（salt + hash(password, salt)）
    if (clientHash == storedHash) {
        log.info("[Session] User '" + name + "' logged in.");
        encryptor_.initSession(name, serverChallenge_);
        currentUser_ = name;
        authenticated_ = true;
        sendResponse(static_cast<uint16_t>(MessageType::LOGIN_RESPONSE), 0);
    } else {
        log.info("[Session] Login failed: wrong password for '" + name + "'.");
        sendResponse(static_cast<uint16_t>(MessageType::LOGIN_RESPONSE), 1);
    }

    return true;
}

// ===== 注册 =====

bool ServerSession::handleRegister(NetworkMessage& msg) {
    auto& log = Logger::instance();

    if (msg.payload.size() < 2) {
        sendError(ErrorCode::INVALID_MESSAGE, "Invalid REGISTER_REQUEST.");
        return true;
    }

    size_t off = 0;
    std::string name = readStringBE(msg.payload.data(), off);

    if (msg.payload.size() - off < 40) {
        sendError(ErrorCode::INVALID_MESSAGE, "Invalid REGISTER_REQUEST payload.");
        return true;
    }

    std::vector<uint8_t> salt(msg.payload.begin() + static_cast<ptrdiff_t>(off),
                               msg.payload.begin() + static_cast<ptrdiff_t>(off + 8));
    off += 8;
    std::vector<uint8_t> pwHash(msg.payload.begin() + static_cast<ptrdiff_t>(off),
                                 msg.payload.begin() + static_cast<ptrdiff_t>(off + 32));

    // 客户端已预先计算 hash，服务器直接存储
    if (!userMgr_.registerUserRaw(name, salt, pwHash)) {
        log.info("[Session] Register failed: user '" + name + "' already exists.");
        sendResponse(static_cast<uint16_t>(MessageType::REGISTER_RESPONSE), 1);
        return true;
    }

    log.info("[Session] User '" + name + "' registered.");

    // 注册后自动初始化传输加密并登录
    encryptor_.initSession(name, serverChallenge_);
    currentUser_ = name;
    authenticated_ = true;

    sendResponse(static_cast<uint16_t>(MessageType::REGISTER_RESPONSE), 0);
    return true;
}

// ===== 备份 =====

bool ServerSession::handleBackup(NetworkMessage& firstMsg) {
    auto& log = Logger::instance();
    log.info("[Session] Receiving backup...");

    std::string backupId = storage_.createBackup(currentUser_);
    std::vector<uint8_t> wholeBak;
    std::vector<uint8_t> metadataBytes;

    // 处理已接收的第一条消息 + 后续消息
    NetworkMessage msg = std::move(firstMsg);
    bool first = true;

    while (true) {
        if (!first) {
            msg = receiveEncrypted();
            if (msg.type == 0) return false;
        }
        first = false;

        if (msg.type == static_cast<uint16_t>(MessageType::BACKUP_COMPLETE)) {
            break;
        }

        if (msg.type == static_cast<uint16_t>(MessageType::FILE_DATA)) {
            if (msg.payload.size() < 10) continue;
            size_t off = 0;
            std::string path = readStringBE(msg.payload.data(), off);
            uint64_t dataLen = readUint64BE(msg.payload.data(), off);

            std::vector<uint8_t> fileData(
                msg.payload.begin() + static_cast<ptrdiff_t>(off),
                msg.payload.begin() + static_cast<ptrdiff_t>(off + dataLen));

            if (path == ".bak_payload") {
                wholeBak = std::move(fileData);
            } else if (path == ".bak_metadata") {
                metadataBytes = std::move(fileData);
            }
        }
    }

    // 保存到服务器
    // 解析 .bak 文件：前 19+metaSize 字节是头部，其余是载荷
    if (!wholeBak.empty()) {
        // 从 .bak 中分离 header 和 payload
        // 头部格式: [MAGIC:4B][VERSION:4B][flags:4B][packAlgo:1B][compressAlgo:1B]
        //            [encryptAlgo:1B][metaSize:4B]
        // 总共 19 字节 + metaSize
        if (wholeBak.size() >= 19) {
            // 读 metaSize（偏移 15-18，小端序）
            uint32_t metaSize = static_cast<uint32_t>(wholeBak[15])
                              | (static_cast<uint32_t>(wholeBak[16]) << 8)
                              | (static_cast<uint32_t>(wholeBak[17]) << 16)
                              | (static_cast<uint32_t>(wholeBak[18]) << 24);

            size_t headerTotalSize = 19 + metaSize;

            std::vector<uint8_t> headerBytes(wholeBak.begin(),
                                              wholeBak.begin() + static_cast<ptrdiff_t>(headerTotalSize));
            std::vector<uint8_t> payloadBytes(wholeBak.begin() + static_cast<ptrdiff_t>(headerTotalSize),
                                               wholeBak.end());

            storage_.saveHeader(currentUser_, backupId, headerBytes);
            storage_.savePayload(currentUser_, backupId, payloadBytes);
        }
    }

    // 保存元数据
    if (!metadataBytes.empty()) {
        MetadataSerializer serializer;
        auto files = serializer.deserialize(metadataBytes);
        storage_.saveMetadata(currentUser_, backupId, files);
    }

    log.info("[Session] Backup " + backupId + " saved for user " + currentUser_
              + " (" + std::to_string(wholeBak.size()) + " bytes).");

    // 发送确认
    std::vector<uint8_t> ack;
    writeStringBE(ack, backupId);
    sendEncrypted(NetworkMessage::make(MessageType::BACKUP_COMPLETE, std::move(ack)));

    return true;
}

// ===== 还原 =====

bool ServerSession::handleRestore(NetworkMessage& msg) {
    auto& log = Logger::instance();

    // 解析 backupId
    size_t off = 0;
    std::string backupId;
    if (msg.payload.size() >= 2) {
        backupId = readStringBE(msg.payload.data(), off);
    }
    if (backupId.empty()) {
        backupId = storage_.getLatestBackupId(currentUser_);
    }

    if (backupId.empty()) {
        sendError(ErrorCode::BACKUP_NOT_FOUND, "No backup found.");
        return true;
    }

    log.info("[Session] Restoring backup " + backupId
              + " for user " + currentUser_);

    // 加载备份数据
    auto headerBytes = storage_.loadHeader(currentUser_, backupId);
    auto payloadBytes = storage_.loadPayload(currentUser_, backupId);
    auto metadata = storage_.loadMetadata(currentUser_, backupId);

    if (headerBytes.empty() && payloadBytes.empty()) {
        sendError(ErrorCode::BACKUP_NOT_FOUND, "Backup not found: " + backupId);
        return true;
    }

    // 重建完整 .bak 文件 = header + payload
    std::vector<uint8_t> wholeBak;
    wholeBak.insert(wholeBak.end(), headerBytes.begin(), headerBytes.end());
    wholeBak.insert(wholeBak.end(), payloadBytes.begin(), payloadBytes.end());

    // 序列化元数据
    MetadataSerializer serializer;
    auto metadataBytes = serializer.serialize(metadata);

    // 发送 .bak 文件
    {
        std::vector<uint8_t> filePayload;
        writeStringBE(filePayload, ".bak");
        writeUint64BE(filePayload, wholeBak.size());
        filePayload.insert(filePayload.end(), wholeBak.begin(), wholeBak.end());
        sendEncrypted(NetworkMessage::make(MessageType::FILE_DATA, std::move(filePayload)));
    }

    // 发送元数据
    {
        std::vector<uint8_t> metaPayload;
        writeStringBE(metaPayload, ".bak_metadata");
        writeUint64BE(metaPayload, metadataBytes.size());
        metaPayload.insert(metaPayload.end(), metadataBytes.begin(), metadataBytes.end());
        sendEncrypted(NetworkMessage::make(MessageType::FILE_DATA, std::move(metaPayload)));
    }

    // 发送完成信号
    std::vector<uint8_t> completePayload;
    writeUint32BE(completePayload, static_cast<uint32_t>(metadata.size()));
    sendEncrypted(NetworkMessage::make(MessageType::RESTORE_COMPLETE, std::move(completePayload)));

    log.info("[Session] Restore data sent.");
    return true;
}

// ===== 列出备份 =====

bool ServerSession::handleListBackups() {
    auto backups = storage_.listBackups(currentUser_);

    std::vector<uint8_t> payload;
    writeUint32BE(payload, static_cast<uint32_t>(backups.size()));

    for (const auto& id : backups) {
        writeStringBE(payload, id);
        time_t ts = storage_.getBackupTimestamp(currentUser_, id);
        writeUint64BE(payload, static_cast<uint64_t>(ts));
    }

    sendEncrypted(NetworkMessage::make(MessageType::BACKUP_LIST_RESPONSE, std::move(payload)));
    return true;
}

void ServerSession::handleLogout() {
    auto& log = Logger::instance();
    log.info("[Session] User '" + currentUser_ + "' logged out.");
}
