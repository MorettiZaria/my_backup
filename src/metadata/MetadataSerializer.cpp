#include "metadata/MetadataSerializer.h"
#include <cstring>

void MetadataSerializer::writeUint16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

void MetadataSerializer::writeUint32(std::vector<uint8_t>& buf, uint32_t val) {
    buf.push_back(static_cast<uint8_t>(val & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

void MetadataSerializer::writeUint64(std::vector<uint8_t>& buf, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
        buf.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
}

void MetadataSerializer::writeString(std::vector<uint8_t>& buf, const std::string& s) {
    writeUint16(buf, static_cast<uint16_t>(s.size()));
    buf.insert(buf.end(), s.begin(), s.end());
}

void MetadataSerializer::writeBytes(std::vector<uint8_t>& buf, const uint8_t* data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

uint16_t MetadataSerializer::readUint16(const uint8_t* data, size_t& offset) {
    uint16_t v = static_cast<uint16_t>(data[offset]) |
                 (static_cast<uint16_t>(data[offset + 1]) << 8);
    offset += 2;
    return v;
}

uint32_t MetadataSerializer::readUint32(const uint8_t* data, size_t& offset) {
    uint32_t v = static_cast<uint32_t>(data[offset]) |
                 (static_cast<uint32_t>(data[offset + 1]) << 8) |
                 (static_cast<uint32_t>(data[offset + 2]) << 16) |
                 (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return v;
}

uint64_t MetadataSerializer::readUint64(const uint8_t* data, size_t& offset) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) {
        v |= static_cast<uint64_t>(data[offset + i]) << (i * 8);
    }
    offset += 8;
    return v;
}

std::string MetadataSerializer::readString(const uint8_t* data, size_t& offset) {
    uint16_t len = readUint16(data, offset);
    std::string s(reinterpret_cast<const char*>(data + offset), len);
    offset += len;
    return s;
}

std::vector<uint8_t> MetadataSerializer::readBytes(const uint8_t* data, size_t& offset, size_t len) {
    std::vector<uint8_t> result(data + offset, data + offset + len);
    offset += len;
    return result;
}

// ===== 序列化格式 =====
// [文件数量:4B]
// [FileInfo 1 序列化字节]
// [FileInfo 2 序列化字节]
// ...

std::vector<uint8_t> MetadataSerializer::serialize(const std::vector<FileInfo>& files) {
    std::vector<uint8_t> buf;

    // 写入文件数量
    writeUint32(buf, static_cast<uint32_t>(files.size()));

    // 逐文件序列化
    for (const auto& f : files) {
        std::vector<uint8_t> fb = f.serialize();
        // 写入该 FileInfo 的序列化长度
        writeUint32(buf, static_cast<uint32_t>(fb.size()));
        // 写入序列化数据
        writeBytes(buf, fb.data(), fb.size());
    }

    return buf;
}

std::vector<FileInfo> MetadataSerializer::deserialize(const std::vector<uint8_t>& data) {
    std::vector<FileInfo> result;
    size_t offset = 0;

    uint32_t count = readUint32(data.data(), offset);

    for (uint32_t i = 0; i < count; ++i) {
        /*uint32_t fiLen =*/ readUint32(data.data(), offset);
        FileInfo info = FileInfo::deserialize(data.data(), offset);
        result.push_back(std::move(info));
    }

    return result;
}
