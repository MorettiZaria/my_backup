#include "network/NetworkProtocol.h"
#include <cstring>

// ===== NetworkMessage =====

NetworkMessage::NetworkMessage() : type(0), reserved(0) {}

NetworkMessage::NetworkMessage(uint16_t t, std::vector<uint8_t> p)
    : type(t), reserved(0), payload(std::move(p)) {}

NetworkMessage NetworkMessage::make(MessageType t) {
    return NetworkMessage(static_cast<uint16_t>(t), {});
}

NetworkMessage NetworkMessage::make(MessageType t, std::vector<uint8_t> p) {
    return NetworkMessage(static_cast<uint16_t>(t), std::move(p));
}

std::vector<uint8_t> NetworkMessage::serialize() const {
    std::vector<uint8_t> result;
    // type (2B, big-endian)
    result.push_back(static_cast<uint8_t>((type >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(type & 0xFF));
    // reserved (2B)
    result.push_back(static_cast<uint8_t>((reserved >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(reserved & 0xFF));
    // payloadLen (4B, big-endian)
    uint32_t len = static_cast<uint32_t>(payload.size());
    result.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
    result.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
    result.push_back(static_cast<uint8_t>(len & 0xFF));
    // payload
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}

NetworkMessage NetworkMessage::deserialize(const uint8_t* data, size_t len) {
    NetworkMessage msg;
    if (len < MESSAGE_HEADER_SIZE) return msg;

    size_t off = 0;
    msg.type = (static_cast<uint16_t>(data[off]) << 8) | data[off + 1];
    off += 2;
    msg.reserved = (static_cast<uint16_t>(data[off]) << 8) | data[off + 1];
    off += 2;

    uint32_t payloadLen = (static_cast<uint32_t>(data[off]) << 24)
                        | (static_cast<uint32_t>(data[off + 1]) << 16)
                        | (static_cast<uint32_t>(data[off + 2]) << 8)
                        | static_cast<uint32_t>(data[off + 3]);
    off += 4;

    if (off + payloadLen > len) return msg;  // 数据不完整

    msg.payload.assign(data + off, data + off + payloadLen);
    return msg;
}

// ===== 大端序写入 =====

void writeUint16BE(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void writeUint32BE(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

void writeUint64BE(std::vector<uint8_t>& buf, uint64_t val) {
    for (int i = 7; i >= 0; --i) {
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
}

// ===== 大端序读取 =====

uint16_t readUint16BE(const uint8_t* data, size_t& offset) {
    uint16_t v = (static_cast<uint16_t>(data[offset]) << 8)
               | static_cast<uint16_t>(data[offset + 1]);
    offset += 2;
    return v;
}

uint32_t readUint32BE(const uint8_t* data, size_t& offset) {
    uint32_t v = (static_cast<uint32_t>(data[offset]) << 24)
               | (static_cast<uint32_t>(data[offset + 1]) << 16)
               | (static_cast<uint32_t>(data[offset + 2]) << 8)
               | static_cast<uint32_t>(data[offset + 3]);
    offset += 4;
    return v;
}

uint64_t readUint64BE(const uint8_t* data, size_t& offset) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v = (v << 8) | static_cast<uint64_t>(data[offset + i]);
    }
    offset += 8;
    return v;
}

// ===== 字符串带长度前缀 =====

void writeStringBE(std::vector<uint8_t>& buf, const std::string& s) {
    writeUint16BE(buf, static_cast<uint16_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

std::string readStringBE(const uint8_t* data, size_t& offset) {
    uint16_t len = readUint16BE(data, offset);
    std::string s(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return s;
}
