#include "workers/RegisterWorker.h"
#include "network/NetworkSocket.h"
#include "network/NetworkProtocol.h"
#include "network/UserManager.h"

RegisterWorker::RegisterWorker(const QString& host,
                               uint16_t port,
                               const QString& username,
                               const QString& password,
                               QObject* parent)
    : QObject(parent)
    , host_(host)
    , port_(port)
    , username_(username)
    , password_(password) {}

void RegisterWorker::run() {
    emit started();

    // 1. 连接服务器
    NetworkSocket sock;
    if (!sock.connect(host_.toStdString(), port_)) {
        emit finished(false, QString("无法连接到服务器 %1:%2").arg(host_).arg(port_));
        return;
    }

    // 2. 握手: CLIENT_HELLO
    {
        std::vector<uint8_t> payload;
        writeUint16BE(payload, PROTOCOL_VERSION);
        if (!sock.sendMessage(NetworkMessage::make(MessageType::CLIENT_HELLO, std::move(payload)))) {
            emit finished(false, "握手失败：无法发送 CLIENT_HELLO。");
            return;
        }
    }

    // 3. 接收 SERVER_HELLO
    NetworkMessage hello = sock.receiveMessage();
    if (hello.type != static_cast<uint16_t>(MessageType::SERVER_HELLO)) {
        emit finished(false, "服务器拒绝了连接请求。");
        return;
    }

    // 4. 生成盐值并计算密码哈希
    auto salt = UserManager::generateSalt();
    auto pwHash = UserManager::computeHash(password_.toStdString(), salt);

    // 5. 发送 REGISTER_REQUEST
    {
        std::vector<uint8_t> payload;
        writeStringBE(payload, username_.toStdString());
        payload.insert(payload.end(), salt.begin(), salt.end());
        payload.insert(payload.end(), pwHash.begin(), pwHash.end());

        if (!sock.sendMessage(NetworkMessage::make(MessageType::REGISTER_REQUEST, std::move(payload)))) {
            emit finished(false, "发送注册请求失败。");
            return;
        }
    }

    // 6. 接收 REGISTER_RESPONSE
    NetworkMessage resp = sock.receiveMessage();
    if (resp.type != static_cast<uint16_t>(MessageType::REGISTER_RESPONSE) || resp.payload.empty()) {
        emit finished(false, "服务器返回了无效的响应。");
        return;
    }

    uint8_t status = resp.payload[0];
    if (status == 0) {
        emit finished(true, QString("用户 '%1' 注册成功！").arg(username_));
    } else {
        emit finished(false, QString("注册失败：用户 '%1' 可能已存在。").arg(username_));
    }
}
