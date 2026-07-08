#ifndef BACKUP_NETWORKPROTOCOL_H
#define BACKUP_NETWORKPROTOCOL_H

#include <cstdint>
#include <vector>
#include <string>

/// 消息类型枚举（大端序 wire 传输）
enum class MessageType : uint16_t {
    // === 握手阶段 ===
    CLIENT_HELLO         = 0x0001,  // C→S: [protocolVersion:2B]
    SERVER_HELLO         = 0x0002,  // S→C: [protocolVersion:2B][challenge:8B]

    // === 认证阶段 ===
    LOGIN_REQUEST        = 0x0003,  // C→S: [nameLen:2B][name][challengeResponse:32B]
    LOGIN_RESPONSE       = 0x0004,  // S→C: [status:1B] (0=OK, 1=密码错, 2=已登录)
    REGISTER_REQUEST     = 0x0005,  // C→S: [nameLen:2B][name][salt:8B][passwordHash:32B]
    REGISTER_RESPONSE    = 0x0006,  // S→C: [status:1B] (0=OK, 1=用户已存在)

    // === 备份阶段 ===
    FILE_DATA            = 0x0009,  // 双向: [pathLen:2B][path][dataLen:8B][data]
    BACKUP_COMPLETE      = 0x000A,  // C→S: 空载荷

    // === 还原阶段 ===
    RESTORE_REQUEST      = 0x000B,  // C→S: [backupIdLen:2B][backupId]（空=最新）
    RESTORE_FILE_DATA    = 0x000C,  // S→C: 同 FILE_DATA 格式
    RESTORE_COMPLETE     = 0x000D,  // S→C: [fileCount:4B]

    // === 管理命令 ===
    BACKUP_LIST_REQUEST  = 0x000E,  // C→S: 空载荷
    BACKUP_LIST_RESPONSE = 0x000F,  // S→C: [count:4B][backupIdLen:2B][backupId][timestamp:8B]...
    LOGIN_SALT           = 0x0013,  // S→C: [salt:8B]（登录时返回用户注册盐值）
    LOGIN_PROOF          = 0x0014,  // C→S: [proof:32B]（客户端计算的挑战-响应证明）
    ERROR_MESSAGE        = 0x0011,  // 双向: [errorCode:2B][msgLen:2B][msg]
    LOGOUT               = 0x0012,  // C→S: 空载荷
};

/// 网络消息：8 字节头部 + 可变载荷
struct NetworkMessage {
    uint16_t type;        // MessageType
    uint16_t reserved;    // 保留字段
    std::vector<uint8_t> payload;

    NetworkMessage();
    NetworkMessage(uint16_t t, std::vector<uint8_t> p);

    /// 序列化为 wire 格式（大端序）
    std::vector<uint8_t> serialize() const;

    /// 从原始字节反序列化
    static NetworkMessage deserialize(const uint8_t* data, size_t len);

    /// 便捷构造
    static NetworkMessage make(MessageType t);
    static NetworkMessage make(MessageType t, std::vector<uint8_t> p);
};

/// 错误码
namespace ErrorCode {
    constexpr uint16_t INCOMPATIBLE_VERSION = 0x0001;
    constexpr uint16_t AUTH_FAILED          = 0x0002;
    constexpr uint16_t USER_NOT_FOUND       = 0x0003;
    constexpr uint16_t USER_EXISTS          = 0x0004;
    constexpr uint16_t NOT_LOGGED_IN        = 0x0005;
    constexpr uint16_t BACKUP_NOT_FOUND     = 0x0006;
    constexpr uint16_t SERVER_ERROR         = 0x0007;
    constexpr uint16_t INVALID_MESSAGE      = 0x0008;
    constexpr uint16_t SESSION_TIMEOUT      = 0x0009;
}

// 协议常量
constexpr uint16_t PROTOCOL_VERSION = 1;
constexpr size_t   MESSAGE_HEADER_SIZE = 8;   // type(2) + reserved(2) + payloadLen(4)

/// 大端序写入辅助
void writeUint16BE(std::vector<uint8_t>& buf, uint16_t val);
void writeUint32BE(std::vector<uint8_t>& buf, uint32_t val);
void writeUint64BE(std::vector<uint8_t>& buf, uint64_t val);

/// 大端序读取辅助
uint16_t readUint16BE(const uint8_t* data, size_t& offset);
uint32_t readUint32BE(const uint8_t* data, size_t& offset);
uint64_t readUint64BE(const uint8_t* data, size_t& offset);

/// 写入带长度前缀的字符串（2B 长度，大端序）
void writeStringBE(std::vector<uint8_t>& buf, const std::string& s);

/// 读取带长度前缀的字符串
std::string readStringBE(const uint8_t* data, size_t& offset);

#endif // BACKUP_NETWORKPROTOCOL_H
