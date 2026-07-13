#include "workers/RemoteListWorker.h"
#include "network/NetworkSocket.h"
#include "network/NetworkProtocol.h"
#include "network/TransportEncryptor.h"
#include "network/UserManager.h"
#include <QDebug>

RemoteListWorker::RemoteListWorker(const QString& host,
                                     uint16_t port,
                                     const QString& username,
                                     const QString& password,
                                     QObject* parent)
    : QObject(parent)
    , host_(host)
    , port_(port)
    , username_(username)
    , password_(password) {}

void RemoteListWorker::run() {
    emit started();

    NetworkSocket sock;
    TransportEncryptor encryptor;
    uint64_t sendSeq = 0;
    uint64_t recvSeq = 0;
    std::vector<uint8_t> serverChallenge;

    // 1. 连接
    if (!sock.connect(host_.toStdString(), port_)) {
        emit finished(false, "无法连接到服务器。", {});
        return;
    }

    // 2. 握手
    {
        std::vector<uint8_t> payload;
        writeUint16BE(payload, PROTOCOL_VERSION);
        if (!sock.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, std::move(payload)))) {
            emit finished(false, "握手失败：无法发送 CLIENT_HELLO。", {});
            return;
        }

        NetworkMessage resp = sock.receiveMessage();
        if (resp.type != static_cast<uint16_t>(MessageType::SERVER_HELLO)) {
            emit finished(false, "握手失败：服务器拒绝连接。", {});
            return;
        }

        if (resp.payload.size() >= 10) {
            size_t off = 0;
            readUint16BE(resp.payload.data(), off);  // server version
            serverChallenge.assign(resp.payload.begin() + static_cast<ptrdiff_t>(off),
                                   resp.payload.end());
        }
    }

    // 3. 登录
    {
        // 第一步：发送用户名
        std::vector<uint8_t> payload;
        writeStringBE(payload, username_.toStdString());
        if (!sock.sendMessage(NetworkMessage::make(MessageType::LOGIN_REQUEST, std::move(payload)))) {
            emit finished(false, "登录失败：无法发送请求。", {});
            return;
        }

        // 第二步：接收盐值
        NetworkMessage saltMsg = sock.receiveMessage();
        if (saltMsg.type != static_cast<uint16_t>(MessageType::LOGIN_SALT)) {
            emit finished(false, "登录失败：服务器响应异常。", {});
            return;
        }

        // 第三步：计算哈希证明
        auto computedHash = UserManager::computeHash(password_.toStdString(), saltMsg.payload);
        if (!sock.sendMessage(NetworkMessage::make(MessageType::LOGIN_PROOF, computedHash))) {
            emit finished(false, "登录失败：无法发送证明。", {});
            return;
        }

        // 第四步：接收结果
        NetworkMessage resp = sock.receiveMessage();
        if (resp.type != static_cast<uint16_t>(MessageType::LOGIN_RESPONSE) ||
            resp.payload.empty() || resp.payload[0] != 0) {
            emit finished(false, "登录失败：用户名或密码错误。", {});
            return;
        }

        // 激活传输加密
        encryptor.initSession(username_.toStdString(), serverChallenge);
    }

    // 4. 发送列表请求（加密）
    {
        NetworkMessage req = NetworkMessage::make(MessageType::BACKUP_LIST_REQUEST);
        if (encryptor.isActive()) {
            req.payload = encryptor.encrypt(req.payload, sendSeq++);
        }
        if (!sock.sendMessage(req)) {
            emit finished(false, "请求失败：无法发送列表请求。", {});
            return;
        }
    }

    // 5. 接收响应（加密）
    NetworkMessage resp = sock.receiveMessage();
    if (encryptor.isActive()) {
        resp.payload = encryptor.decrypt(resp.payload, recvSeq++);
    }

    if (resp.type != static_cast<uint16_t>(MessageType::BACKUP_LIST_RESPONSE)) {
        emit finished(false, "获取列表失败：服务器响应异常。", {});
        return;
    }

    // 6. 解析备份列表
    QVector<BackupEntry> entries;
    size_t off = 0;
    uint32_t count = readUint32BE(resp.payload.data(), off);

    for (uint32_t i = 0; i < count; ++i) {
        std::string id = readStringBE(resp.payload.data(), off);
        std::string name = readStringBE(resp.payload.data(), off);
        uint64_t ts = readUint64BE(resp.payload.data(), off);
        entries.append({QString::fromStdString(id),
                        QString::fromStdString(name), ts});
    }

    emit finished(true,
                  QString("获取成功，共 %1 个备份。").arg(count),
                  entries);
}
